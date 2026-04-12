#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netdb.h>
#include <signal.h>
#include "protocol.h" 
#include "game.h"

#define BUFFER_SIZE 2048
#define MAX_CLIENTES 10

// Mutex para proteger la escritura en logs
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
char *archivo_logs_global = NULL;

void registrar_log(const char *ip, int port, const char *sent_recv, const char *msg) {
    if (!archivo_logs_global) return;
    pthread_mutex_lock(&log_mutex);
    FILE *f = fopen(archivo_logs_global, "a");
    if (f) {
        fprintf(f, "[%s:%d] %s: %s", ip, port, sent_recv, msg);
        if (msg[strlen(msg)-1] != '\n') fprintf(f, "\n");
        fclose(f);
    }
    pthread_mutex_unlock(&log_mutex);
}

typedef struct {
    int fd;
    char ip[INET_ADDRSTRLEN];
    int puerto;
    char rol[16];
    char username[64];
} ThreadInfo;

// Funciones para manejar los comandos

void handle_auth(ThreadInfo *cliente, Mensaje *msg, char *respuesta) {
    if (msg->num_params < 1) {
        construir_error(respuesta, 400, "AUTH requiere un username");
        return;
    }
    
    // Re-unir los parámetros para permitir nombres con espacios
    memset(cliente->username, 0, 64);
    for (int i = 0; i < msg->num_params; i++) {
        if (i > 0) strncat(cliente->username, " ", 63 - strlen(cliente->username));
        strncat(cliente->username, msg->params[i], 63 - strlen(cliente->username));
    }
    
    // Lógica temporal de prefijos para asignar ROL
    if (strncmp(cliente->username, "atacante", 8) == 0) {
        strcpy(cliente->rol, "atacante");
    } else {
        strcpy(cliente->rol, "defensor");
    }

    snprintf(respuesta, 1024, "OK AUTH \"%s\" ROLE=%s\n", cliente->username, cliente->rol);
}

void handle_create_room(char *respuesta) {
    char room_id[16];
    if (game_crear_sala(room_id) == 0) {
        snprintf(respuesta, 256, "ROOM_CREATED %s\n", room_id);
    } else {
        construir_error(respuesta, 500, "No hay espacio para más salas");
    }
}

void handle_join(ThreadInfo *cliente, Mensaje *msg, char *respuesta) {
    if (msg->num_params < 1) {
        construir_error(respuesta, 400, "JOIN requiere room_id");
        return;
    }
    int x, y;
    int join_res = game_unir_jugador(msg->params[0], cliente->fd, cliente->username, cliente->rol, cliente->ip, cliente->puerto, &x, &y);
    
    if (join_res == 0) {
        int offset = snprintf(respuesta, 2048, "OK JOIN %s ROLE=%s POS=%d,%d", msg->params[0], cliente->rol, x, y);
        Sala *s = game_buscar_sala(msg->params[0]);
        if (s) {
            // Sincronizar JUGADORES
            offset += snprintf(respuesta + offset, 2048 - offset, " PLAYERS=");
            for (int i = 0; i < MAX_JUGADORES; i++) {
                if (s->jugadores[i].activo && s->jugadores[i].fd != cliente->fd) {
                    offset += snprintf(respuesta + offset, 2048 - offset, "%s:%d,%d;", 
                                      s->jugadores[i].username, s->jugadores[i].x, s->jugadores[i].y);
                }
            }
            // Si es defensor, revelamos recursos
            if (strstr(cliente->rol, "defensor") != NULL) {
                offset += snprintf(respuesta + offset, 2048 - offset, " RESOURCES=");
                for (int i = 0; i < MAX_RECURSOS; i++) {
                    offset += snprintf(respuesta + offset, 2048 - offset, "%s:%d,%d;", 
                                      s->recursos[i].id, s->recursos[i].x, s->recursos[i].y);
                }
            }
        }
        strcat(respuesta, "\n");
        
        char nt[128];
        snprintf(nt, 128, "NOTIFY PLAYER_JOIN %s %d %d\n", cliente->username, x, y);
        game_notificar_sala(msg->params[0], cliente->fd, nt);
    } else if (join_res == -3) {
        construir_error(respuesta, 409, "Nombre de usuario ya en uso en esta sala");
    } else {
        construir_error(respuesta, 404, "Sala no encontrada o llena");
    }
}

void handle_list_rooms(char *respuesta) {
    char lista[512];
    game_listar_salas(lista);
    snprintf(respuesta, 1024, "ROOM_LIST %s\n", lista);
}

void handle_move(ThreadInfo *cliente, Mensaje *msg, char *respuesta) {
    if (msg->num_params < 2) {
        construir_error(respuesta, 400, "MOVE requiere dx dy");
        return;
    }
    int dx = atoi(msg->params[0]);
    int dy = atoi(msg->params[1]);
    int nx, ny;
    char rid[16];
    
    if (game_mover_jugador(cliente->fd, dx, dy, &nx, &ny, rid) == 0) {
        snprintf(respuesta, 1024, "OK MOVE %d %d\n", nx, ny);
        char nt[128];
        snprintf(nt, 128, "NOTIFY MOVE %s %d %d\n", cliente->username, nx, ny);
        game_notificar_sala(rid, cliente->fd, nt);
    } else {
        construir_error(respuesta, 401, "Autentiquese primero o entre a una sala");
    }
}

void handle_scan(ThreadInfo *cliente, Mensaje *msg, char *respuesta) {
    if (strcmp(cliente->rol, "atacante") != 0) {
        construir_error(respuesta, 403, "Solo atacantes pueden usar SCAN");
        return;
    }
    char res_id_found[16];
    int rx, ry;
    if (game_scan_recurso(cliente->fd, res_id_found, &rx, &ry) == 0) {
        snprintf(respuesta, 1024, "OK SCAN found %s %d %d\n", res_id_found, rx, ry);
    } else {
        construir_error(respuesta, 404, "No hay recursos cerca");
    }
}

void handle_attack(ThreadInfo *cliente, Mensaje *msg, char *respuesta) {
    char rid[16];
    char real_id[16] = "";
    const char *res_id = (msg->num_params > 0) ? msg->params[0] : "";
    
    int res = game_atacar_recurso(cliente->fd, res_id, rid, real_id);
    if (res == 0) {
        snprintf(respuesta, 256, "OK ATTACK %s\n", real_id);
        char nt[128];
        snprintf(nt, 128, "NOTIFY ATTACK %s \"Servidor bajo ataque\"\n", real_id);
        game_notificar_sala(rid, cliente->fd, nt);
    } else if (res == -2) construir_error(respuesta, 403, "Solo atacantes pueden ATTACK");
    else if (res == -3) construir_error(respuesta, 410, "No estas en la celda del recurso");
    else if (res == -4) construir_error(respuesta, 411, "Recurso no esta safe");
    else construir_error(respuesta, 404, "Recurso no encontrado en tu posicion");
}

void handle_mitigate(ThreadInfo *cliente, Mensaje *msg, char *respuesta) {
    char rid[16];
    char real_id[16] = "";
    const char *res_id = (msg->num_params > 0) ? msg->params[0] : "";
    
    int res = game_mitigar_recurso(cliente->fd, res_id, rid, real_id);
    if (res == 0) {
        snprintf(respuesta, 256, "OK MITIGATE %s\n", real_id);
        char nt[128];
        snprintf(nt, 128, "NOTIFY MITIGATED %s \"Ataque mitigado\"\n", real_id);
        game_notificar_sala(rid, cliente->fd, nt);
    } else if (res == -2) construir_error(respuesta, 403, "Solo defensores pueden MITIGATE");
    else if (res == -3) construir_error(respuesta, 410, "No estas en la celda del recurso");
    else if (res == -4) construir_error(respuesta, 411, "Recurso no bajo ataque");
    else construir_error(respuesta, 404, "Recurso no encontrado en tu posicion");
}

void handle_status(ThreadInfo *cliente, char *respuesta) {
    char st[512];
    game_obtener_estado_jugador(cliente->fd, st);
    if (strncmp(st, "ERR", 3) == 0) {
        strncpy(respuesta, st, 1024);
        strcat(respuesta, "\n");
    } else {
        snprintf(respuesta, 1024, "GAME_STATE %s\n", st);
    }
}

// Motor principal de conexiones

void *atender_cliente(void *arg) {
    ThreadInfo *cliente = (ThreadInfo *)arg;
    char buffer[BUFFER_SIZE];
    char respuesta[BUFFER_SIZE];

    pthread_mutex_lock(&log_mutex);
    printf("Cliente conectado: %s:%d\n", cliente->ip, cliente->puerto);
    pthread_mutex_unlock(&log_mutex);

    // Valores por defecto
    strcpy(cliente->rol, "defensor");
    strcpy(cliente->username, "anonimo");

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        memset(respuesta, 0, sizeof(respuesta));
        int bytes = recv(cliente->fd, buffer, BUFFER_SIZE - 1, 0);

        if (bytes <= 0) {
            printf("Cliente desconectado: %s:%d\n", cliente->ip, cliente->puerto);
            game_desconectar_jugador(cliente->fd);
            break;
        }

        printf("[%s:%d] Recibido: %s", cliente->ip, cliente->puerto, buffer);
        registrar_log(cliente->ip, cliente->puerto, "Recibido", buffer);

        Mensaje msg = parsear_mensaje(buffer);

        // Enrutamiuento de controladores
        switch (msg.verbo) {
            case VERB_AUTH:         handle_auth(cliente, &msg, respuesta); break;
            case VERB_CREATE_ROOM:  handle_create_room(respuesta); break;
            case VERB_JOIN:         handle_join(cliente, &msg, respuesta); break;
            case VERB_LIST_ROOMS:   handle_list_rooms(respuesta); break;
            case VERB_MOVE:         handle_move(cliente, &msg, respuesta); break;
            case VERB_SCAN:         handle_scan(cliente, &msg, respuesta); break;
            case VERB_ATTACK:       handle_attack(cliente, &msg, respuesta); break;
            case VERB_MITIGATE:     handle_mitigate(cliente, &msg, respuesta); break;
            case VERB_STATUS:       handle_status(cliente, respuesta); break;
            case VERB_PING:         snprintf(respuesta, 256, "PONG\n"); break;
            
            case VERB_QUIT:
                if (strlen(respuesta) > 0) {
                    send(cliente->fd, respuesta, strlen(respuesta), 0);
                    registrar_log(cliente->ip, cliente->puerto, "Enviado", respuesta);
                }
                close(cliente->fd);
                free(cliente);
                return NULL;

            case VERB_UNKNOWN:
            default:
                construir_error(respuesta, 400, "Verbo desconocido");
                break;
        }

        // Loggear y enviar respuesta oficial si el controlador armó algo
        if (strlen(respuesta) > 0) {
            send(cliente->fd, respuesta, strlen(respuesta), 0);
            registrar_log(cliente->ip, cliente->puerto, "Enviado", respuesta);
            printf("[%s:%d] Enviado: %s", cliente->ip, cliente->puerto, respuesta);
        }
    }

    close(cliente->fd);
    free(cliente);
    return NULL;
}

// Configuracion e infraestructura

void *tick_thread_func(void *arg) {
    while (1) {
        sleep(1);
        game_tick();
    }
    return NULL;
}

void registrar_en_dns(int server_port) {
    int sockfd;
    struct sockaddr_in servaddr;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) return;

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;

    const char *dns_port_str = getenv("DNS_PORT");
    int dns_port = dns_port_str ? atoi(dns_port_str) : 5353;
    servaddr.sin_port = htons(dns_port);

    const char *dns_ip = getenv("DNS_SERVER");
    if (!dns_ip) dns_ip = "127.0.0.1";

    servaddr.sin_addr.s_addr = inet_addr(dns_ip);

    char reg[128];
    snprintf(reg, sizeof(reg), "REGISTER server.cdsp %d", server_port);
    sendto(sockfd, reg, strlen(reg), 0, (const struct sockaddr *)&servaddr, sizeof(servaddr));

    char buffer[128];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);

    int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&from, &fromlen);
    if (n > 0) {
        buffer[n] = '\0';
        printf("DNS RESPONSE: %s\n", buffer);
    } else {
        printf("DNS: sin respuesta\n");
    }

    close(sockfd);
    printf("DNS: Registro enviado a %s:%d\n", dns_ip, dns_port);
}

int configurar_socket(const char *port_str) {
    int server_fd;
    struct addrinfo hints, *servinfo, *p;
    int yes = 1, rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // IPv4 Only
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // Usar IP local

    if ((rv = getaddrinfo(NULL, port_str, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((server_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) continue;

        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(server_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(server_fd);
            continue;
        }
        break; // Bind Exitoso
    }

    if (p == NULL) return -1;

    freeaddrinfo(servinfo);
    listen(server_fd, MAX_CLIENTES);
    
    return server_fd;
}

// Función principal

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);

    const char *env_port = getenv("SERVER_PORT");
    const char *env_log = getenv("SERVER_LOG_FILE");

    const char *port_str = (argc > 1) ? argv[1] : (env_port ? env_port : "8080");
    archivo_logs_global = (argc > 2) ? argv[2] : (char *)(env_log ? env_log : "logs.txt");

    int puerto = atoi(port_str);
    registrar_en_dns(puerto);

    // Check para permisos de archivo log
    FILE *log_file = fopen(archivo_logs_global, "a");
    if (log_file == NULL) {
        printf("Error: No se pudo abrir el archivo de logs %s\n", archivo_logs_global);
        return 1;
    }
    fclose(log_file);

    int server_fd = configurar_socket(port_str);
    if (server_fd < 0) {
        fprintf(stderr, "servidor: error configurando red\n");
        return 2;
    }

    game_init();
    
    pthread_t t_tick;
    pthread_create(&t_tick, NULL, tick_thread_func, NULL);
    pthread_detach(t_tick);

    printf("Servidor escuchando en puerto %d...\n", puerto);

    while (1) {
        struct sockaddr_in cliente_addr;
        socklen_t cliente_len = sizeof(cliente_addr);
        int cliente_fd = accept(server_fd, (struct sockaddr *)&cliente_addr, &cliente_len);
        
        if (cliente_fd < 0) continue;

        ThreadInfo *info = malloc(sizeof(ThreadInfo));
        info->fd = cliente_fd;
        
        inet_ntop(AF_INET, &cliente_addr.sin_addr, info->ip, sizeof(info->ip));
        info->puerto = ntohs(cliente_addr.sin_port);

        pthread_t hilo;
        pthread_create(&hilo, NULL, atender_cliente, info);
        pthread_detach(hilo);
    }

    close(server_fd);
    fclose(log_file);
    return 0;
}