#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "game.h"

// Estado global del juego
static Sala salas[MAX_SALAS];
static int  sala_counter = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Inicializar el estado del juego
void game_init() {
    memset(salas, 0, sizeof(salas));
    sala_counter = 0;
}

// Crear una nueva sala y devolver su ID en id_out
// Retorna 0 si tuvo éxito, -1 si no hay espacio
int game_crear_sala(char *id_out) {
    pthread_mutex_lock(&mutex);

    int idx = -1;
    for (int i = 0; i < MAX_SALAS; i++) {
        if (!salas[i].activa) { idx = i; break; }
    }

    if (idx == -1) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    sala_counter++;
    Sala *s = &salas[idx];
    memset(s, 0, sizeof(Sala));
    s->activa      = 1;
    s->estado      = SALA_ESPERANDO;
    s->num_jugadores = 0;
    snprintf(s->id, sizeof(s->id), "room_%03d", sala_counter);

    // Colocar los 2 recursos críticos en posiciones fijas
    strncpy(s->recursos[0].id, "srv_01", sizeof(s->recursos[0].id));
    s->recursos[0].x = 5;
    s->recursos[0].y = 5;
    s->recursos[0].estado = RECURSO_SAFE;

    strncpy(s->recursos[1].id, "srv_02", sizeof(s->recursos[1].id));
    s->recursos[1].x = 15;
    s->recursos[1].y = 15;
    s->recursos[1].estado = RECURSO_SAFE;

    strncpy(id_out, s->id, 16);

    pthread_mutex_unlock(&mutex);
    return 0;
}

// Unir un jugador a una sala existente
// Retorna 0 si tuvo éxito, -1 si la sala no existe o está llena
int game_unir_jugador(const char *room_id, int fd, const char *username, const char *rol, int *x_out, int *y_out) {
    pthread_mutex_lock(&mutex);

    Sala *s = NULL;
    for (int i = 0; i < MAX_SALAS; i++) {
        if (salas[i].activa && strcmp(salas[i].id, room_id) == 0) {
            s = &salas[i]; break;
        }
    }

    if (s == NULL || s->num_jugadores >= MAX_JUGADORES) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    // Buscar un slot libre para el jugador
    int idx = -1;
    for (int i = 0; i < MAX_JUGADORES; i++) {
        if (!s->jugadores[i].activo) { idx = i; break; }
    }

    if (idx == -1) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    Jugador *j = &s->jugadores[idx];
    j->activo = 1;
    j->fd     = fd;
    j->x      = 0;
    j->y      = 0;
    strncpy(j->username, username, sizeof(j->username) - 1);
    strncpy(j->rol,      rol,      sizeof(j->rol) - 1);
    s->num_jugadores++;

    *x_out = j->x;
    *y_out = j->y;

    pthread_mutex_unlock(&mutex);
    return 0;
}

// Llenar buffer_out con la lista de salas activas
void game_listar_salas(char *buffer_out) {
    pthread_mutex_lock(&mutex);

    int count = 0;
    char ids[256] = "";
    for (int i = 0; i < MAX_SALAS; i++) {
        if (salas[i].activa) {
            strcat(ids, salas[i].id);
            strcat(ids, " ");
            count++;
        }
    }

    snprintf(buffer_out, 256, "%d %s", count, ids);
    pthread_mutex_unlock(&mutex);
}

// Buscar una sala por ID — NO usa mutex, llamar desde contexto ya bloqueado
Sala *game_buscar_sala(const char *room_id) {
    for (int i = 0; i < MAX_SALAS; i++) {
        if (salas[i].activa && strcmp(salas[i].id, room_id) == 0)
            return &salas[i];
    }
    return NULL;
}

// Enviar un mensaje a todos los jugadores de una sala excepto al emisor
void game_notificar_sala(const char *room_id, int fd_emisor, const char *mensaje) {
    pthread_mutex_lock(&mutex);

    Sala *s = game_buscar_sala(room_id);
    if (s == NULL) {
        pthread_mutex_unlock(&mutex);
        return;
    }

    for (int i = 0; i < MAX_JUGADORES; i++) {
        if (s->jugadores[i].activo && s->jugadores[i].fd != fd_emisor) {
            send(s->jugadores[i].fd, mensaje, strlen(mensaje), 0);
        }
    }

    pthread_mutex_unlock(&mutex);
}

// Desconectar un jugador de cualquier sala donde esté
void game_desconectar_jugador(int fd) {
    pthread_mutex_lock(&mutex);

    for (int i = 0; i < MAX_SALAS; i++) {
        if (!salas[i].activa) continue;
        for (int j = 0; j < MAX_JUGADORES; j++) {
            if (salas[i].jugadores[j].activo && salas[i].jugadores[j].fd == fd) {
                salas[i].jugadores[j].activo = 0;
                salas[i].num_jugadores--;
            }
        }
    }

    pthread_mutex_unlock(&mutex);
}