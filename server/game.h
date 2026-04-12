#ifndef GAME_H
#define GAME_H

#include <pthread.h>
#include <time.h>

#define MAX_SALAS      10
#define MAX_JUGADORES  10
#define PLANO_ANCHO    20
#define PLANO_ALTO     20
#define MAX_RECURSOS    2

// Estados posibles de un recurso crítico
typedef enum {
    RECURSO_SAFE,
    RECURSO_BAJO_ATAQUE,
    RECURSO_MITIGADO,
    RECURSO_COMPROMETIDO
} EstadoRecurso;

// Estados posibles de una sala
typedef enum {
    SALA_ESPERANDO,
    SALA_EN_JUEGO,
    SALA_TERMINADA
} EstadoSala;

// Representa un recurso crítico en el plano
typedef struct {
    char id[16];           // "srv_01", "srv_02"
    int x, y;              // Posición en el plano
    EstadoRecurso estado;
    time_t tiempo_ataque;  // Momento en que inició el ataque
} Recurso;

// Representa un jugador conectado
typedef struct {
    int activo;            // 1 si está conectado, 0 si no
    int fd;                // Socket del cliente
    char username[64];
    char rol[16];          // "atacante" o "defensor"
    char ip[64];           // IP para logs
    int port;              // Puerto para logs
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
    time_t inicio_partida; // Momento en que se creó la sala
} Sala;

// Funciones disponibles
void game_init();
int  game_crear_sala(char *id_out);
int  game_unir_jugador(const char *room_id, int fd, const char *username, const char *rol, const char *ip, int port, int *x_out, int *y_out);
void game_listar_salas(char *buffer_out);
Sala *game_buscar_sala(const char *room_id);
void game_notificar_sala(const char *room_id, int fd_emisor, const char *mensaje);
void game_desconectar_jugador(int fd);

// Funciones de lógica de juego
int  game_mover_jugador(int fd, int dx, int dy, int *nx, int *ny, char *room_id_out);
int  game_scan_recurso(int fd, char *res_id_out, int *x_out, int *y_out);
int  game_atacar_recurso(int fd, const char *res_id, char *room_id_out, char *real_id_out);
int  game_mitigar_recurso(int fd, const char *res_id, char *room_id_out, char *real_id_out);
void game_obtener_estado_jugador(int fd, char *buffer_out);
const char* game_obtener_rol(int fd);
void game_tick();

#endif