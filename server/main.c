#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netdb.h>
#include "protocol.h" 
#include "game.h"

#define BUFFER_SIZE 1024
#define MAX_CLIENTES 10

// Estructura que le pasamos a cada hilo con los datos del cliente
typedef struct {
    int fd;
    char ip[INET_ADDRSTRLEN];
    int puerto;
    FILE *log_file;
} ClienteInfo;

// Hilo para la lógica global (cronómetro y victorias)
void *ciclo_juego(void *arg) {
    while (1) {
        game_tick();
        sleep(1);
    }
    return NULL;
}


void registrar_en_dns(int server_port) {
    const char *dns_host = getenv("DNS_SERVER");
    const char *dns_port_str = getenv("DNS_PORT");

    if (!dns_host) dns_host = "127.0.0.1";
    if (!dns_port_str) dns_port_str = "5353";

    int sockfd;
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) return;

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(atoi(dns_port_str));
    servaddr.sin_addr.s_addr = inet_addr(dns_host);

    char reg[128];
    snprintf(reg, sizeof(reg), "REGISTER server.cdsp %d", server_port);
    sendto(sockfd, reg, strlen(reg), 0, (const struct sockaddr *)&servaddr, sizeof(servaddr));

    // Esperar confirmación
    char buffer[128];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    struct timeval tv = {2, 0};
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&from, &fromlen);
    if (n > 0) {
        buffer[n] = '\0';
        printf("DNS: Registro exitoso — %s\n", buffer);
    } else {
        printf("DNS: Sin respuesta del servidor DNS\n");
    }

    close(sockfd);
}


// Consulta el rol de un usuario al auth server
// Retorna 0 si encontró el usuario, -1 si no
int consultar_auth_server(const char *username, char *rol_out) {
    const char *auth_host = getenv("AUTH_SERVER");
    const char *auth_port = getenv("AUTH_PORT");

    if (!auth_host) auth_host = "127.0.0.1";
    if (!auth_port) auth_port = "9090";

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // Resolver nombre de dominio — cumple Req. 3
    if (getaddrinfo(auth_host, auth_port, &hints, &res) != 0) {
        printf("Auth server: error resolviendo %s:%s\n", auth_host, auth_port);
        return -1;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(res);
        return -1;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        printf("Auth server: no se pudo conectar a %s:%s\n", auth_host, auth_port);
        close(sock);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);

    // Enviar el username
    char msg[128];
    snprintf(msg, sizeof(msg), "%s\n", username);
    send(sock, msg, strlen(msg), 0);

    // Recibir respuesta
    char buffer[128] = "";
    recv(sock, buffer, sizeof(buffer) - 1, 0);
    close(sock);

    // Parsear respuesta: "OK attacker" o "ERR usuario no encontrado"
    if (strncmp(buffer, "OK ", 3) == 0) {
        // Extraer el rol y quitar el \n
        strncpy(rol_out, buffer + 3, 15);
        rol_out[strcspn(rol_out, "\n")] = '\0';
        return 0;
    }

    return -1;
}


// Ejecutar cada hilo — atiende a un cliente
void *atender_cliente(void *arg) {
    ClienteInfo *cliente = (ClienteInfo *)arg;
    char buffer[BUFFER_SIZE];
    char respuesta[512];
    char room_actual[16] = "";    // sala en la que está el jugador
    char rol_actual[16] = "";     // rol del jugador
    char username_actual[64] = ""; // nombre del jugador

    printf("Cliente conectado: %s:%d\n", cliente->ip, cliente->puerto);
    fprintf(cliente->log_file, "Cliente conectado: %s:%d\n", cliente->ip, cliente->puerto);
    fflush(cliente->log_file);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes = recv(cliente->fd, buffer, BUFFER_SIZE - 1, 0);

        if (bytes <= 0) {
            printf("Cliente desconectado: %s:%d\n", cliente->ip, cliente->puerto);
            fprintf(cliente->log_file, "Cliente desconectado: %s:%d\n", cliente->ip, cliente->puerto);
            fflush(cliente->log_file);
            break;
        }

        // Loggear lo recibido
        printf("[%s:%d] Recibido: %s", cliente->ip, cliente->puerto, buffer);
        fprintf(cliente->log_file, "[%s:%d] Recibido: %s", cliente->ip, cliente->puerto, buffer);
        fflush(cliente->log_file);

        // Parsear el mensaje y construir respuesta
        Mensaje msg = parsear_mensaje(buffer);

        switch (msg.verbo) {
            case VERB_AUTH:
                if (msg.num_params < 1) {
                    construir_error(respuesta, 400, "AUTH requiere un username");
                } else {
                    char rol[16] = "";
                    if (consultar_auth_server(msg.params[0], rol) == 0) {
                        char datos[256];
                        snprintf(datos, sizeof(datos), "\"%s\" ROLE=%s", msg.params[0], rol);
                        construir_respuesta(respuesta, "AUTH", datos);
                        // Guardar datos para usarlo en los comandos del juego
                        strncpy(rol_actual, rol, sizeof(rol_actual) - 1);
                        strncpy(username_actual, msg.params[0], sizeof(username_actual) - 1);
                    } else {
                        construir_error(respuesta, 401, "Usuario no encontrado");
                    }
                }
                break;

            case VERB_QUIT: {
                construir_respuesta(respuesta, "QUIT", NULL);
                send(cliente->fd, respuesta, strlen(respuesta), 0);
                
                char rm[16], un[64];
                if (game_desconectar_jugador(cliente->fd, rm, un)) {
                    char notify[128];
                    snprintf(notify, sizeof(notify), "NOTIFY PLAYER_LEFT %s\n", un);
                    game_notificar_sala(rm, -1, notify);
                }

                close(cliente->fd);
                free(cliente);
                return NULL;
            }

            case VERB_UNKNOWN:
            default:
                construir_error(respuesta, 400, "Verbo desconocido");
                break;

            case VERB_CREATE_ROOM: {
                char room_id[16];
                if (game_crear_sala(room_id) == 0) {
                    construir_respuesta(respuesta, "CREATE_ROOM", room_id);
                } else {
                    construir_error(respuesta, 500, "No hay espacio para más salas");
                }
                break;
            }

            case VERB_JOIN: {
                if (msg.num_params < 2) {
                    construir_error(respuesta, 400, "JOIN requiere room_id y rol");
                    break;
                }
                int x, y;
                if (game_unir_jugador(msg.params[0], cliente->fd, username_actual, msg.params[1], &x, &y) == 0) {
                    strncpy(room_actual, msg.params[0], sizeof(room_actual) - 1);
                    strncpy(rol_actual,  msg.params[1], sizeof(rol_actual)  - 1);
                    
                    char lista_jugadores[512] = "";
                    game_get_players_string(room_actual, lista_jugadores, sizeof(lista_jugadores));

                    char datos[1024];
                    // Si es defensor, incluir posiciones de recursos
                    if (strcmp(rol_actual, "defender") == 0) {
                        snprintf(datos, sizeof(datos),
                            "%s ROLE=%s POS=%d,%d PLAYERS=%s RESOURCES=srv_01:5,5;srv_02:15,15",
                            msg.params[0], msg.params[1], x, y, lista_jugadores);
                    } else {
                        snprintf(datos, sizeof(datos),
                            "%s ROLE=%s POS=%d,%d PLAYERS=%s",
                            msg.params[0], msg.params[1], x, y, lista_jugadores);
                    }

                    construir_respuesta(respuesta, "JOIN", datos);
                    char notify[128];
                    snprintf(notify, sizeof(notify), "NOTIFY PLAYER_JOIN %s %d %d\n", username_actual, x, y);
                    game_notificar_sala(room_actual, cliente->fd, notify);
                } else {
                    construir_error(respuesta, 404, "Sala no existe o esta llena");
                }
                break;
            }


            case VERB_LIST_ROOMS: {
                char lista[1024]; // Aumentado para coincidir con la capacidad del servidor
                game_listar_salas(lista, sizeof(lista));
                construir_respuesta(respuesta, "LIST_ROOMS", lista);
                break;
            }

            case VERB_MOVE: {
                //Print temporal
                printf("DEBUG MOVE: room_actual='%s' rol_actual='%s'\n", room_actual, rol_actual);

                if (msg.num_params < 2) {
                    construir_error(respuesta, 400, "MOVE requiere dx y dy");
                    break;
                }
                if (strlen(room_actual) == 0) {
                    construir_error(respuesta, 401, "Debes unirte a una sala primero");
                    break;
                }
                int dx = atoi(msg.params[0]);
                int dy = atoi(msg.params[1]);
                int nx, ny;
                char notify_found[256] = "";

                if (game_mover_jugador(room_actual, cliente->fd, dx, dy, &nx, &ny, notify_found) == 0) {
                    char datos[64];
                    snprintf(datos, sizeof(datos), "POS=%d,%d", nx, ny);
                    construir_respuesta(respuesta, "MOVE", datos);

                    // Notificar movimiento a los demás
                    char notify[128];
                    snprintf(notify, sizeof(notify), "NOTIFY MOVE %s %d %d\n", username_actual, nx, ny);
                    game_notificar_sala(room_actual, cliente->fd, notify);

                    // Si el atacante encontró un recurso, notificarlo solo a él
                    if (strlen(notify_found) > 0) {
                        send(cliente->fd, notify_found, strlen(notify_found), 0);
                    }
                } else {
                    construir_error(respuesta, 400, "Movimiento fuera de los limites");
                }
                break;
            }

            case VERB_SCAN: {
                if (strlen(room_actual) == 0) {
                    construir_error(respuesta, 401, "Debes unirte a una sala primero");
                    break;
                }
                if (strcmp(rol_actual, "attacker") != 0) {
                    construir_error(respuesta, 403, "Solo atacantes pueden usar SCAN");
                    break;
                }
                game_scan(room_actual, cliente->fd, respuesta);
                // game_scan ya construye la respuesta completa
                send(cliente->fd, respuesta, strlen(respuesta), 0);
                printf("[%s:%d] Enviado: %s", cliente->ip, cliente->puerto, respuesta);
                fprintf(cliente->log_file, "[%s:%d] Enviado: %s", cliente->ip, cliente->puerto, respuesta);
                fflush(cliente->log_file);
                continue;
            }

            case VERB_ATTACK: {
                if (msg.num_params < 1) {
                    construir_error(respuesta, 400, "ATTACK requiere resource_id");
                    break;
                }
                if (strcmp(rol_actual, "attacker") != 0) {
                    construir_error(respuesta, 403, "Solo atacantes pueden usar ATTACK");
                    break;
                }
                int resultado = game_atacar(room_actual, cliente->fd, msg.params[0]);
                if (resultado == 0) {
                    construir_respuesta(respuesta, "ATTACK", msg.params[0]);
                    char notify[128];
                    snprintf(notify, sizeof(notify), "NOTIFY ATTACK %s\n", msg.params[0]);
                    game_notificar_sala(room_actual, cliente->fd, notify);
                } else if (resultado == -2) {
                    construir_error(respuesta, 410, "No estas en la celda del recurso");
                } else if (resultado == -3) {
                    construir_error(respuesta, 411, "El recurso no esta en estado safe");
                } else {
                    construir_error(respuesta, 404, "Recurso no encontrado");
                }
                break;
            }

            case VERB_MITIGATE: {
                if (msg.num_params < 1) {
                    construir_error(respuesta, 400, "MITIGATE requiere resource_id");
                    break;
                }
                if (strcmp(rol_actual, "defender") != 0) {
                    construir_error(respuesta, 403, "Solo defensores pueden usar MITIGATE");
                    break;
                }
                int resultado = game_mitigar(room_actual, cliente->fd, msg.params[0]);
                if (resultado == 0) {
                    construir_respuesta(respuesta, "MITIGATE", msg.params[0]);
                    char notify[128];
                    snprintf(notify, sizeof(notify), "NOTIFY MITIGATED %s\n", msg.params[0]);
                    game_notificar_sala(room_actual, cliente->fd, notify);
                } else if (resultado == -2) {
                    construir_error(respuesta, 410, "No estas en la celda del recurso");
                } else if (resultado == -3) {
                    construir_error(respuesta, 411, "El recurso no esta bajo ataque");
                } else {
                    construir_error(respuesta, 404, "Recurso no encontrado");
                }
                break;
            }

            case VERB_STATUS: {
                if (strlen(room_actual) == 0) {
                    construir_error(respuesta, 401, "Debes unirte a una sala primero");
                    break;
                }
                char status_str[1024];
                game_status_sala(room_actual, status_str, sizeof(status_str));
                construir_respuesta(respuesta, "STATUS", status_str);
                break;
            }
        }

        // Loggear y enviar respuesta
        send(cliente->fd, respuesta, strlen(respuesta), 0);
        printf("[%s:%d] Enviado: %s", cliente->ip, cliente->puerto, respuesta);
        fprintf(cliente->log_file, "[%s:%d] Enviado: %s", cliente->ip, cliente->puerto, respuesta);
        fflush(cliente->log_file);
    }

    char rm[16], un[64];
    if (game_desconectar_jugador(cliente->fd, rm, un)) {
        char notify[128];
        snprintf(notify, sizeof(notify), "NOTIFY PLAYER_LEFT %s\n", un);
        game_notificar_sala(rm, -1, notify);
    }

    close(cliente->fd);
    free(cliente);
    return NULL;
}



int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: ./server <puerto> <archivoDeLogs>\n");
        return 1;
    }

    int puerto = atoi(argv[1]);
    char *archivo_logs = argv[2];

    FILE *log_file = fopen(archivo_logs, "a");
    if (log_file == NULL) {
        printf("Error: no se pudo abrir el archivo de logs\n");
        return 1;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("Error: no se pudo crear el socket\n");
        return 1;
    }

    // Evita el error "Address already in use" al reiniciar el servidor
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in direccion;
    direccion.sin_family = AF_INET;
    direccion.sin_addr.s_addr = INADDR_ANY;
    direccion.sin_port = htons(puerto);

    
    if (bind(server_fd, (struct sockaddr *)&direccion, sizeof(direccion)) < 0) {
        printf("Error: no se pudo hacer bind al puerto %d\n", puerto);
        return 1;
    }

    listen(server_fd, MAX_CLIENTES);
    printf("Servidor escuchando en puerto %d...\n", puerto);

    // Inicializar estado del juego
    game_init();
    registrar_en_dns(puerto);

    // Iniciar hilo de tick del juego
    pthread_t hilo_tick;
    pthread_create(&hilo_tick, NULL, ciclo_juego, NULL);
    pthread_detach(hilo_tick);

    // Bucle principal: acepta clientes indefinidamente
    while (1) {
        struct sockaddr_in cliente_addr;
        socklen_t cliente_len = sizeof(cliente_addr);
        int cliente_fd = accept(server_fd, (struct sockaddr *)&cliente_addr, &cliente_len);

        if (cliente_fd < 0) {
            printf("Error al aceptar conexión\n");
            continue;
        }

        // Crear estructura con datos del cliente
        ClienteInfo *cliente = malloc(sizeof(ClienteInfo));
        cliente->fd = cliente_fd;
        cliente->puerto = ntohs(cliente_addr.sin_port);
        cliente->log_file = log_file;
        inet_ntop(AF_INET, &cliente_addr.sin_addr, cliente->ip, INET_ADDRSTRLEN);

        // Crear hilo para este cliente
        pthread_t hilo;
        pthread_create(&hilo, NULL, atender_cliente, cliente);
        pthread_detach(hilo); // El hilo se limpia solo al terminar
    }

    close(server_fd);
    fclose(log_file);
    return 0;
}
