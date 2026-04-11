#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: %s <host> <puerto>\n", argv[0]);
        return 1;
    }

    char *host = argv[1];
    int port = atoi(argv[2]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Error creando socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error conectando al servidor");
        return 1;
    }

    printf("Conectado al servidor %s:%d\n", host, port);

    // 🔹 Enviar comando AUTH
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "AUTH andre");
    send(sock, buffer, strlen(buffer), 0);

    // 🔹 Recibir respuesta del servidor
    char respuesta[128];
    int n = recv(sock, respuesta, sizeof(respuesta)-1, 0);
    if (n > 0) {
        respuesta[n] = '\0';
        printf("Respuesta del servidor: %s\n", respuesta);
    }

// 🔹 Enviar comando MOVE
snprintf(buffer, sizeof(buffer), "MOVE 2 3");  // ejemplo: mover a posición (2,3)
send(sock, buffer, strlen(buffer), 0);

// 🔹 Recibir respuesta del servidor
n = recv(sock, respuesta, sizeof(respuesta)-1, 0);
if (n > 0) {
    respuesta[n] = '\0';
    printf("Respuesta del servidor: %s\n", respuesta);
}

// 🔹 Enviar comando ATTACK
snprintf(buffer, sizeof(buffer), "ATTACK enemigo1");
send(sock, buffer, strlen(buffer), 0);

n = recv(sock, respuesta, sizeof(respuesta)-1, 0);
if (n > 0) {
    respuesta[n] = '\0';
    printf("Respuesta del servidor: %s\n", respuesta);
}

// 🔹 Enviar comando SCAN
snprintf(buffer, sizeof(buffer), "SCAN zona1");
send(sock, buffer, strlen(buffer), 0);

n = recv(sock, respuesta, sizeof(respuesta)-1, 0);
if (n > 0) {
    respuesta[n] = '\0';
    printf("Respuesta del servidor: %s\n", respuesta);
}

// 🔹 Enviar comando MITIGATE
snprintf(buffer, sizeof(buffer), "MITIGATE enemigo1");
send(sock, buffer, strlen(buffer), 0);

n = recv(sock, respuesta, sizeof(respuesta)-1, 0);
if (n > 0) {
    respuesta[n] = '\0';
    printf("Respuesta del servidor: %s\n", respuesta);
}

// 🔹 Enviar comando STATUS
snprintf(buffer, sizeof(buffer), "STATUS");
send(sock, buffer, strlen(buffer), 0);

n = recv(sock, respuesta, sizeof(respuesta)-1, 0);
if (n > 0) {
    respuesta[n] = '\0';
    printf("Respuesta del servidor: %s\n", respuesta);
}

    close(sock);
    return 0;
}



