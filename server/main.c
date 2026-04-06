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
} ClienteInfo;

// Ejecutar cada hilo — atiende a un cliente
void *atender_cliente(void *arg) {
    ClienteInfo *cliente = (ClienteInfo *)arg;
    char buffer[BUFFER_SIZE];
    char respuesta[256];

    printf("Cliente conectado: %s:%d\n", cliente->ip, cliente->puerto);
    fprintf(cliente->log_file, "Cliente conectado: %s:%d\n", cliente->ip, cliente->puerto);
    fflush(cliente->log_file);

    // Inicializar estado del juego
    game_init();

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
                    // Por ahora acepta cualquier usuario — aquí irá la consulta al auth server
                    char datos[128];
                    snprintf(datos, sizeof(datos), "\"%s\" ROLE=atacante", msg.params[0]);
                    construir_respuesta(respuesta, "AUTH", datos);
                }
                break;

            case VERB_QUIT:
                construir_respuesta(respuesta, "QUIT", NULL);
                send(cliente->fd, respuesta, strlen(respuesta), 0);
                printf("[%s:%d] Enviado: %s", cliente->ip, cliente->puerto, respuesta);
                fprintf(cliente->log_file, "[%s:%d] Enviado: %s", cliente->ip, cliente->puerto, respuesta);
                fflush(cliente->log_file);
                close(cliente->fd);
                free(cliente);
                return NULL;

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
                if (game_unir_jugador(msg.params[0], cliente->fd, "jugador", msg.params[1], &x, &y) == 0) {
                    char datos[128];
                    snprintf(datos, sizeof(datos), "%s ROLE=%s POS=%d,%d", msg.params[0], msg.params[1], x, y);
                    construir_respuesta(respuesta, "JOIN", datos);
                } else {
                    construir_error(respuesta, 404, "Sala no existe o está llena");
                }
                break;
            }

            case VERB_LIST_ROOMS: {
                char lista[256];
                game_listar_salas(lista);
                construir_respuesta(respuesta, "LIST_ROOMS", lista);
                break;
            }
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