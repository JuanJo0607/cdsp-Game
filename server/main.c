#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

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

        // Responder (Se cambiará por el protocolo después)
        char *respuesta = "OK\n";
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