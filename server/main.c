#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
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
    char rol[16];
    char username[64];
} ClienteInfo;

// Ejecutar cada hilo — atiende a un cliente
void *atender_cliente(void *arg) {
    ClienteInfo *cliente = (ClienteInfo *)arg;
    char buffer[BUFFER_SIZE];
    char respuesta[1024];

    printf("Cliente conectado: %s:%d\n", cliente->ip, cliente->puerto);
    fprintf(cliente->log_file, "Cliente conectado: %s:%d\n", cliente->ip, cliente->puerto);
    fflush(cliente->log_file);

    // Valores por defecto
    strcpy(cliente->rol, "defensor");
    strcpy(cliente->username, "anonimo");

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes = recv(cliente->fd, buffer, BUFFER_SIZE - 1, 0);

        if (bytes <= 0) {
            printf("Cliente desconectado: %s:%d\n", cliente->ip, cliente->puerto);
            fprintf(cliente->log_file, "Cliente desconectado: %s:%d\n", cliente->ip, cliente->puerto);
            fflush(cliente->log_file);
            game_desconectar_jugador(cliente->fd);
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
                    strncpy(cliente->username, msg.params[0], 63);
                    
                    // Lógica temporal de prefijos para asignar ROL
                    if (strncmp(cliente->username, "atacante", 8) == 0) {
                        strcpy(cliente->rol, "atacante");
                    } else {
                        strcpy(cliente->rol, "defensor");
                    }

                    snprintf(respuesta, 1024, "OK AUTH \"%s\" ROLE=%s\n", cliente->username, cliente->rol);
                }
                break;

            case VERB_QUIT:
                construir_respuesta(respuesta, "QUIT", NULL);
                send(cliente->fd, respuesta, strlen(respuesta), 0);
                game_desconectar_jugador(cliente->fd);
                close(cliente->fd);
                free(cliente);
                return NULL;

            case VERB_CREATE_ROOM: {
                char room_id[16];
                if (game_crear_sala(room_id) == 0) {
                    snprintf(respuesta, 256, "ROOM_CREATED %s\n", room_id);
                } else {
                    construir_error(respuesta, 500, "No hay espacio para más salas");
                }
                break;
            }

            case VERB_JOIN: {
                if (msg.num_params < 1) {
                    construir_error(respuesta, 400, "JOIN requiere room_id");
                    break;
                }
                int x, y;
                if (game_unir_jugador(msg.params[0], cliente->fd, cliente->username, cliente->rol, &x, &y) == 0) {
                    snprintf(respuesta, 1024, "OK JOIN %s ROLE=%s POS=%d,%d\n", msg.params[0], cliente->rol, x, y);
                    
                    // 1. Notificar a los demás que yo me uní (con mis coordenadas)
                    char nt[128];
                    snprintf(nt, 128, "NOTIFY PLAYER_JOIN %s %d %d\n", cliente->username, x, y);
                    game_notificar_sala(msg.params[0], cliente->fd, nt);

                    // 2. Enviarme a mi las posiciones de los que ya estaban (Sincronización)
                    Sala *s = game_buscar_sala(msg.params[0]);
                    if (s) {
                        for (int i = 0; i < MAX_JUGADORES; i++) {
                            if (s->jugadores[i].activo && s->jugadores[i].fd != cliente->fd) {
                                char sync_msg[128];
                                snprintf(sync_msg, 128, "NOTIFY MOVE %s %d %d\n", s->jugadores[i].username, s->jugadores[i].x, s->jugadores[i].y);
                                send(cliente->fd, sync_msg, strlen(sync_msg), 0);
                            }
                        }
                    }
                } else {
                    construir_error(respuesta, 409, "Sala no existe o está llena");
                }
                break;
            }

            case VERB_LIST_ROOMS: {
                char lista[512];
                game_listar_salas(lista);
                snprintf(respuesta, 1024, "ROOM_LIST %s\n", lista);
                break;
            }

            case VERB_MOVE: {
                if (msg.num_params < 2) {
                    construir_error(respuesta, 400, "MOVE requiere dx dy");
                    break;
                }
                int dx = atoi(msg.params[0]);
                int dy = atoi(msg.params[1]);
                int nx, ny;
                char rid[16];
                if (game_mover_jugador(cliente->fd, dx, dy, &nx, &ny, rid) == 0) {
                    snprintf(respuesta, 1024, "OK MOVE %d %d\n", nx, ny);
                    
                    // Notificar movimiento de otros jugadores (Visibilidad mutua)
                    char nt[128];
                    snprintf(nt, 128, "NOTIFY MOVE %s %d %d\n", cliente->username, nx, ny);
                    game_notificar_sala(rid, cliente->fd, nt);
                } else {
                    construir_error(respuesta, 401, "Autentiquese primero o entre a una sala");
                }
                break;
            }

            case VERB_SCAN: {
                char res_id[16];
                int rx, ry;
                int res = game_scan_recurso(cliente->fd, res_id, &rx, &ry);
                if (res == 0) {
                    snprintf(respuesta, 1024, "SCAN_RESULT found %s %d %d\n", res_id, rx, ry);
                } else if (res == -1) {
                    snprintf(respuesta, 1024, "SCAN_RESULT nothing\n");
                } else if (res == -2) {
                    construir_error(respuesta, 403, "Solo atacantes pueden SCAN (ahora deshabilitado)");
                } else {
                    construir_error(respuesta, 401, "Autentiquese primero");
                }
                break;
            }

            case VERB_ATTACK: {
                if (msg.num_params < 1) {
                    construir_error(respuesta, 400, "ATTACK requiere resource_id");
                    break;
                }
                char rid[16];
                int res = game_atacar_recurso(cliente->fd, msg.params[0], rid);
                if (res == 0) {
                    snprintf(respuesta, 256, "OK ATTACK %s\n", msg.params[0]);
                    char nt[128];
                    snprintf(nt, 128, "NOTIFY ATTACK %s \"Servidor bajo ataque\"\n", msg.params[0]);
                    game_notificar_sala(rid, cliente->fd, nt);
                } else if (res == -2) construir_error(respuesta, 403, "Solo atacantes pueden ATTACK");
                else if (res == -3) construir_error(respuesta, 410, "No estas en la celda del recurso");
                else if (res == -4) construir_error(respuesta, 411, "Recurso no esta safe");
                else construir_error(respuesta, 404, "Recurso no encontrado");
                break;
            }

            case VERB_MITIGATE: {
                if (msg.num_params < 1) {
                    construir_error(respuesta, 400, "MITIGATE requiere resource_id");
                    break;
                }
                char rid[16];
                int res = game_mitigar_recurso(cliente->fd, msg.params[0], rid);
                if (res == 0) {
                    snprintf(respuesta, 256, "OK MITIGATE %s\n", msg.params[0]);
                    char nt[128];
                    snprintf(nt, 128, "NOTIFY MITIGATED %s \"Ataque mitigado\"\n", msg.params[0]);
                    game_notificar_sala(rid, cliente->fd, nt);
                } else if (res == -2) construir_error(respuesta, 403, "Solo defensores pueden MITIGATE");
                else if (res == -3) construir_error(respuesta, 410, "No estas en la celda del recurso");
                else if (res == -4) construir_error(respuesta, 411, "Recurso no bajo ataque");
                else construir_error(respuesta, 404, "Recurso no encontrado");
                break;
            }

            case VERB_STATUS: {
                char st[512];
                game_obtener_estado_jugador(cliente->fd, st);
                if (strncmp(st, "ERR", 3) == 0) {
                    strncpy(respuesta, st, 1024);
                    strcat(respuesta, "\n");
                } else {
                    snprintf(respuesta, 1024, "GAME_STATE %s\n", st);
                }
                break;
            }

            case VERB_PING:
                snprintf(respuesta, 256, "PONG\n");
                break;

            case VERB_UNKNOWN:
            default:
                construir_error(respuesta, 400, "Verbo desconocido");
                break;
        }

        // Loggear y enviar respuesta
        send(cliente->fd, respuesta, strlen(respuesta), 0);
        printf("[%s:%d] Enviado: %s", cliente->ip, cliente->puerto, respuesta);
        fprintf(cliente->log_file, "[%s:%d] Enviado: %s", cliente->ip, cliente->puerto, respuesta);
        fflush(cliente->log_file);
    }

    close(cliente->fd);
    free(cliente);
    return NULL;
}



void *tick_thread_func(void *arg) {
    while (1) {
        sleep(1);
        game_tick();
    }
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
    game_init();
    
    // Lanzar hilo de monitoreo de tiempo
    pthread_t t_tick;
    pthread_create(&t_tick, NULL, tick_thread_func, NULL);
    pthread_detach(t_tick);

    printf("Servidor escuchando en puerto %d...\n", puerto);

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