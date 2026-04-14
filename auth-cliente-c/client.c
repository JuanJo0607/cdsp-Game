#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <sys/time.h>

int resolve_hostname(const char *hostname, char *ip_out, int *port_out) {
    // Si ya parece una IP, no resolver
    if (hostname[0] >= '0' && hostname[0] <= '9') {
        strcpy(ip_out, hostname);
        return 0;
    }

    const char *dns_host = getenv("DNS_SERVER");
    const char *dns_port_str = getenv("DNS_PORT");
    if (!dns_host) dns_host = "127.0.0.1";
    int dns_port = dns_port_str ? atoi(dns_port_str) : 5353;
    char buffer[1024];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) return -1;

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(dns_port); 
    servaddr.sin_addr.s_addr = inet_addr(dns_host);

    struct timeval tv = {2, 0};
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    printf("DNS: Resolviendo [%s]...\n", hostname);
    snprintf(buffer, sizeof(buffer), "QUERY %s", hostname);
    sendto(sockfd, buffer, strlen(buffer), 0, (const struct sockaddr *)&servaddr, sizeof(servaddr));

    socklen_t len = sizeof(servaddr);
    int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&servaddr, &len);
    close(sockfd);

    if (n > 0) {
        buffer[n] = '\0';
        if (strncmp(buffer, "ANSWER ", 7) == 0 && strstr(buffer, "NOT_FOUND") == NULL) {
            char r_name[64], r_ip[64], r_port[16];
            sscanf(buffer + 7, "%s %s %s", r_name, r_ip, r_port);
            strcpy(ip_out, r_ip);
            *port_out = atoi(r_port);
            printf("DNS: %s resuelto a %s:%d\n", hostname, ip_out, *port_out);
            return 0;
        }
    }
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Faltan argumentos. Abriendo porta web...\n");
        // Intentar abrir en WSL (Windows) o Linux nativo
        system("powershell.exe /c start http://localhost:8000 2>/dev/null || xdg-open http://localhost:8000 2>/dev/null");
        printf("\nUso: %s <host> <puerto> <usuario> <sala>\n", argv[0]);
        return 0;
    }

    char *host_arg = argv[1];
    int port = atoi(argv[2]);
    char *username = argv[3];
    char *room_id = argv[4];
    char resolved_ip[64];

    if (resolve_hostname(host_arg, resolved_ip, &port) != 0) {
        printf("DNS ERROR: No se pudo resolver %s. Intentando conexión directa...\n", host_arg);
        strcpy(resolved_ip, host_arg);
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Error creando socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, resolved_ip, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error conectando al servidor");
        return 1;
    }

    printf("CONECTADO (%s)\n", resolved_ip);

    // 1. AUTH
    char buffer[256], respuesta[512];
    snprintf(buffer, sizeof(buffer), "AUTH %s", username);
    send(sock, buffer, strlen(buffer), 0);
    int n = recv(sock, respuesta, sizeof(respuesta)-1, 0);
    if (n <= 0) { printf("Error en AUTH\n"); close(sock); return 1; }
    respuesta[n] = '\0';
    printf("Auth Res: %s\n", respuesta);

    // Parsear Rol de respuesta: "OK AUTH andrew ROLE=attacker ..."
    char role[16] = "attacker"; // default
    char *role_ptr = strstr(respuesta, "ROLE=");
    if (role_ptr) {
        sscanf(role_ptr, "ROLE=%s", role);
        // Cortar si hay espacios
        char *space = strchr(role, ' ');
        if (space) *space = '\0';
    }

    // 2. JOIN
    printf("Uniendo a sala %s con rol %s...\n", room_id, role);
    snprintf(buffer, sizeof(buffer), "JOIN %s %s", room_id, role);
    send(sock, buffer, strlen(buffer), 0);
    n = recv(sock, respuesta, sizeof(respuesta)-1, 0);
    if (n <= 0) return 1;
    respuesta[n] = '\0';
    printf("Join Res: %s\n", respuesta);

    // 3. SECUENCIA DE PRUEBA
    printf("\nEjecutando secuencia de prueba...\n");
    
    char *cmds[] = {"MOVE 1 0", "SCAN", "STATUS", "MOVE 0 1", "STATUS", NULL};
    for(int i=0; cmds[i] != NULL; i++) {
        printf("> %s\n", cmds[i]);
        send(sock, cmds[i], strlen(cmds[i]), 0);
        n = recv(sock, respuesta, sizeof(respuesta)-1, 0);
        if (n > 0) {
            respuesta[n] = '\0';
            printf("< %s\n", respuesta);
        }
        sleep(1);
    }

    printf("\nPrueba terminada.\n");
    close(sock);
    return 0;
}



