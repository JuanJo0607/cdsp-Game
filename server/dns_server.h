#ifndef DNS_SERVER_H
#define DNS_SERVER_H

#define DNS_PORT "5353"
#define BUFFER_SIZE 1024

// Estructura para las entradas del mini-DNS
typedef struct {
    char name[64];
    char ip[64];
} DnsEntry;

// Tabla de resolución estática
static DnsEntry dns_table[] = {
    {"server.cdsp", "127.0.0.1"},
    {"auth.cdsp", "127.0.0.1"},
    {"", ""}
};

#endif
