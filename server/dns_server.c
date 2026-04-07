#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "dns_server.h"

int main(int argc, char *argv[]) {
    const char *port = (argc > 1) ? argv[1] : DNS_PORT;
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // Protocol-agnostic (IPv4 or IPv6)
    hints.ai_socktype = SOCK_DGRAM; // UDP
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            continue;
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "DNS: error al iniciar el servicio UDP\n");
        return 2;
    }

    freeaddrinfo(servinfo);
    DnsEntry dynamic_table[100];
    int num_entries = 0;

    // Poblar con la tabla estática al inicio
    for (int i = 0; dns_table[i].name[0] != '\0' && i < 100; i++) {
        strncpy(dynamic_table[num_entries].name, dns_table[i].name, 64);
        strncpy(dynamic_table[num_entries].ip, dns_table[i].ip, 64);
        num_entries++;
    }

    char buffer[BUFFER_SIZE];
    struct sockaddr_storage cliaddr;
    socklen_t addr_len;

    while (1) {
        addr_len = sizeof(cliaddr);
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&cliaddr, &addr_len);
        if (n <= 0) continue;
        buffer[n] = '\0';

        char remote_ip[INET6_ADDRSTRLEN];
        struct sockaddr_in *s4 = (struct sockaddr_in *)&cliaddr;
        inet_ntop(AF_INET, &(s4->sin_addr), remote_ip, sizeof(remote_ip));

        printf("[%s] RECV: %s\n", remote_ip, buffer);

        // Registro Dinámico: "REGISTER <hostname>"
        if (strncmp(buffer, "REGISTER ", 9) == 0) {
            char *hostname = buffer + 9;
            int found = 0;
            for (int i = 0; i < num_entries; i++) {
                if (strcmp(dynamic_table[i].name, hostname) == 0) {
                    strncpy(dynamic_table[i].ip, remote_ip, 64);
                    found = 1;
                    break;
                }
            }
            if (!found && num_entries < 100) {
                strncpy(dynamic_table[num_entries].name, hostname, 64);
                strncpy(dynamic_table[num_entries].ip, remote_ip, 64);
                num_entries++;
            }
            printf("  -> Registered: %s at %s\n", hostname, remote_ip);
        }
        // Consulta: "QUERY <hostname>"
        else if (strncmp(buffer, "QUERY ", 6) == 0) {
            char *hostname = buffer + 6;
            char response[BUFFER_SIZE] = "ANSWER NOT_FOUND";
            
            for (int i = 0; i < num_entries; i++) {
                if (strcmp(dynamic_table[i].name, hostname) == 0) {
                    snprintf(response, BUFFER_SIZE, "ANSWER %s %s", hostname, dynamic_table[i].ip);
                    break;
                }
            }
            sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)&cliaddr, addr_len);
            printf("  -> Sent: %s\n", response);
        }
    }

    close(sockfd);
    return 0;
}
