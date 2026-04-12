#ifndef DNS_SERVER_H
#define DNS_SERVER_H

#define DNS_PORT "5353"
#define BUFFER_SIZE 1024

// Estructura para las entradas del mini-DNS
typedef struct {
    char name[64];
    char ip[64];
} DnsEntry;

// los servicios se registran con REGISTER <hostname>

#endif
