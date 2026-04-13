#ifndef GAME_H
#define GAME_H

#include <pthread.h>

#define MAX_SALAS      10
#define MAX_JUGADORES  10
#define PLANO_ANCHO    20
#define PLANO_ALTO     20
#define MAX_RECURSOS    2

// Estados posibles de un recurso crítico
typedef enum {
    RECURSO_SAFE,
    RECURSO_BAJO_ATAQUE,
    RECURSO_MITIGADO
} EstadoRecurso;

// Estados posibles de una sala
typedef enum {
    SALA_ESPERANDO,
    SALA_EN_JUEGO
} EstadoSala;

// Representa un recurso crítico en el plano
typedef struct {
    char id[16];           // "srv_01", "srv_02"
    int x, y;              // Posición en el plano
    EstadoRecurso estado;
} Recurso;

// Representa un jugador conectado
typedef struct {
    int activo;            // 1 si está conectado, 0 si no
    int fd;                // Socket del cliente
    char username[64];
    char rol[16];          // "atacante" o "defensor"
    int x, y;              // Posición en el plano
} Jugador;

// Representa una sala de juego
typedef struct {
    int activa;            // 1 si existe, 0 si no
    char id[16];           // "room_001"
    EstadoSala estado;
    Jugador jugadores[MAX_JUGADORES];
    int num_jugadores;
    Recurso recursos[MAX_RECURSOS];
} Sala;

// Funciones del juego
void game_init();
int  game_crear_sala(char *id_out);
int  game_unir_jugador(const char *room_id, int fd, const char *username, const char *rol, int *x_out, int *y_out);
void game_listar_salas(char *buffer_out);
Sala *game_buscar_sala(const char *room_id);
void game_notificar_sala(const char *room_id, int fd_emisor, const char *mensaje);
void game_desconectar_jugador(int fd);
int  game_mover_jugador(const char *room_id, int fd, int dx, int dy, int *x_out, int *y_out, char *notify_out);
void game_scan(const char *room_id, int fd, char *resultado_out);
int  game_atacar(const char *room_id, int fd, const char *resource_id);
int  game_mitigar(const char *room_id, int fd, const char *resource_id);


#endif