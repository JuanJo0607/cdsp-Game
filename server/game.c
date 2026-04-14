#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "game.h"

#include <sys/socket.h>
#include <stdlib.h>

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

    // Iniciar la partida al entrar el primer jugador
    if (s->num_jugadores == 1) {
        s->estado = SALA_EN_JUEGO;
        s->tiempo_inicio = time(NULL);
        s->terminada = 0;
    }

    *x_out = j->x;
    *y_out = j->y;

    pthread_mutex_unlock(&mutex);
    return 0;
}

// Llenar buffer_out con la lista de salas activas
void game_listar_salas(char *buffer_out, size_t size) {
    pthread_mutex_lock(&mutex);

    int count = 0;
    char ids[1024] = "";
    for (int i = 0; i < MAX_SALAS; i++) {
        if (salas[i].activa && !salas[i].terminada) {
            strcat(ids, salas[i].id);
            strcat(ids, " ");
            count++;
        }
    }

    snprintf(buffer_out, size, "%d %s", count, ids);
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
// Retorna 1 si encontró al jugador y llenó room_id_out y username_out, 0 si no
int game_desconectar_jugador(int fd, char *room_id_out, char *username_out) {
    pthread_mutex_lock(&mutex);

    for (int i = 0; i < MAX_SALAS; i++) {
        if (!salas[i].activa) continue;
        for (int j = 0; j < MAX_JUGADORES; j++) {
            if (salas[i].jugadores[j].activo && salas[i].jugadores[j].fd == fd) {
                salas[i].jugadores[j].activo = 0;
                salas[i].num_jugadores--;
                if (room_id_out) strcpy(room_id_out, salas[i].id);
                if (username_out) strcpy(username_out, salas[i].jugadores[j].username);
                pthread_mutex_unlock(&mutex);
                return 1;
            }
        }
    }

    pthread_mutex_unlock(&mutex);
    return 0;
}

// Obtener lista de jugadores en una sala en formato "user1:x,y;user2:x,y;"
void game_get_players_string(const char *room_id, char *buffer_out, size_t size) {
    pthread_mutex_lock(&mutex);
    Sala *s = game_buscar_sala(room_id);
    if (s == NULL) {
        pthread_mutex_unlock(&mutex);
        return;
    }
    
    buffer_out[0] = '\0';
    for (int i = 0; i < MAX_JUGADORES; i++) {
        if (s->jugadores[i].activo) {
            char p[128];
            snprintf(p, sizeof(p), "%s:%d,%d;", s->jugadores[i].username, s->jugadores[i].x, s->jugadores[i].y);
            strncat(buffer_out, p, size - strlen(buffer_out) - 1);
        }
    }
    pthread_mutex_unlock(&mutex);
}



//Segunda Fase 
// Mover jugador — retorna 0 si OK, -1 si fuera de límites
// Si el atacante llega a un recurso, notify_out tendrá el mensaje NOTIFY
int game_mover_jugador(const char *room_id, int fd, int dx, int dy, int *x_out, int *y_out, char *notify_out) {
    pthread_mutex_lock(&mutex);

    notify_out[0] = '\0';

    Sala *s = game_buscar_sala(room_id);
    if (s == NULL) { pthread_mutex_unlock(&mutex); return -1; }

    // Buscar al jugador
    Jugador *j = NULL;
    for (int i = 0; i < MAX_JUGADORES; i++) {
        if (s->jugadores[i].activo && s->jugadores[i].fd == fd) {
            j = &s->jugadores[i]; break;
        }
    }
    if (j == NULL) { pthread_mutex_unlock(&mutex); return -1; }

    // Validar límites
    int nx = j->x + dx;
    int ny = j->y + dy;
    if (nx < 0 || nx >= PLANO_ANCHO || ny < 0 || ny >= PLANO_ALTO) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    j->x = nx;
    j->y = ny;
    *x_out = nx;
    *y_out = ny;

    // Si es atacante, verificar si llegó a un recurso
    if (strcmp(j->rol, "attacker") == 0) {
        for (int i = 0; i < MAX_RECURSOS; i++) {
            if (s->recursos[i].x == nx && s->recursos[i].y == ny &&
                s->recursos[i].estado == RECURSO_SAFE) {
                snprintf(notify_out, 256, "NOTIFY FOUND %s\n", s->recursos[i].id);
                break;
            }
        }
    }

    pthread_mutex_unlock(&mutex);
    return 0;
}

// SCAN — detecta recursos en las 8 celdas adyacentes
void game_scan(const char *room_id, int fd, char *resultado_out) {
    pthread_mutex_lock(&mutex);

    Sala *s = game_buscar_sala(room_id);
    if (s == NULL) {
        snprintf(resultado_out, 256, "SCAN_RESULT none\n");
        pthread_mutex_unlock(&mutex);
        return;
    }

    // Buscar al jugador
    Jugador *j = NULL;
    for (int i = 0; i < MAX_JUGADORES; i++) {
        if (s->jugadores[i].activo && s->jugadores[i].fd == fd) {
            j = &s->jugadores[i]; break;
        }
    }

    if (j == NULL) {
        snprintf(resultado_out, 256, "SCAN_RESULT none\n");
        pthread_mutex_unlock(&mutex);
        return;
    }

    // Revisar las 8 celdas adyacentes + la celda actual
    for (int i = 0; i < MAX_RECURSOS; i++) {
        int rx = s->recursos[i].x;
        int ry = s->recursos[i].y;
        if (abs(rx - j->x) <= 1 && abs(ry - j->y) <= 1) {
            snprintf(resultado_out, 256, "SCAN_RESULT found %s\n", s->recursos[i].id);
            pthread_mutex_unlock(&mutex);
            return;
        }
    }

    snprintf(resultado_out, 256, "SCAN_RESULT none\n");
    pthread_mutex_unlock(&mutex);
}

// ATTACK — retorna 0 si OK, -1 si error
int game_atacar(const char *room_id, int fd, const char *resource_id) {
    pthread_mutex_lock(&mutex);

    Sala *s = game_buscar_sala(room_id);
    if (s == NULL) { pthread_mutex_unlock(&mutex); return -1; }

    // Buscar jugador
    Jugador *j = NULL;
    for (int i = 0; i < MAX_JUGADORES; i++) {
        if (s->jugadores[i].activo && s->jugadores[i].fd == fd) {
            j = &s->jugadores[i]; break;
        }
    }
    if (j == NULL) { pthread_mutex_unlock(&mutex); return -1; }

    // Buscar recurso
    Recurso *r = NULL;
    for (int i = 0; i < MAX_RECURSOS; i++) {
        if (strcmp(s->recursos[i].id, resource_id) == 0) {
            r = &s->recursos[i]; break;
        }
    }
    if (r == NULL) { pthread_mutex_unlock(&mutex); return -1; }

    // Validar que el jugador está en la celda del recurso
    if (j->x != r->x || j->y != r->y) { pthread_mutex_unlock(&mutex); return -2; }

    // Validar que el recurso está en estado safe
    if (r->estado != RECURSO_SAFE) { pthread_mutex_unlock(&mutex); return -3; }

    r->estado = RECURSO_BAJO_ATAQUE;
    r->inicio_ataque = time(NULL);

    pthread_mutex_unlock(&mutex);
    return 0;
}

// MITIGATE — retorna 0 si OK, -1 si error
int game_mitigar(const char *room_id, int fd, const char *resource_id) {
    pthread_mutex_lock(&mutex);

    Sala *s = game_buscar_sala(room_id);
    if (s == NULL) { pthread_mutex_unlock(&mutex); return -1; }

    // Buscar jugador
    Jugador *j = NULL;
    for (int i = 0; i < MAX_JUGADORES; i++) {
        if (s->jugadores[i].activo && s->jugadores[i].fd == fd) {
            j = &s->jugadores[i]; break;
        }
    }
    if (j == NULL) { pthread_mutex_unlock(&mutex); return -1; }

    // Buscar recurso
    Recurso *r = NULL;
    for (int i = 0; i < MAX_RECURSOS; i++) {
        if (strcmp(s->recursos[i].id, resource_id) == 0) {
            r = &s->recursos[i]; break;
        }
    }
    if (r == NULL) { pthread_mutex_unlock(&mutex); return -1; }

    // Validar que el jugador está en la celda del recurso
    if (j->x != r->x || j->y != r->y) { pthread_mutex_unlock(&mutex); return -2; }

    // Validar que el recurso está bajo ataque
    if (r->estado != RECURSO_BAJO_ATAQUE) { pthread_mutex_unlock(&mutex); return -3; }

    r->estado = RECURSO_SAFE;

    pthread_mutex_unlock(&mutex);
    return 0;
}

// Bucle de lógica del juego (se llama cada segundo desde main.c)
void game_tick() {
    pthread_mutex_lock(&mutex);
    time_t now = time(NULL);
    for (int i = 0; i < MAX_SALAS; i++) {
        Sala *s = &salas[i];
        if (!s->activa || s->terminada) continue;

        // 1. Verificar tiempo global (5 minutos = 300s) - SOLAMENTE si la partida inició
        if (s->estado == SALA_EN_JUEGO) {
            int tiempo_transcurrido = (int)difftime(now, s->tiempo_inicio);
            if (tiempo_transcurrido >= 300) {
                s->terminada = 1;
                char msg[128];
                snprintf(msg, sizeof(msg), "NOTIFY GAME_OVER defender 5 minutos transcurridos\n");
                for (int k = 0; k < MAX_JUGADORES; k++) {
                    if (s->jugadores[k].activo) send(s->jugadores[k].fd, msg, strlen(msg), 0);
                }
                continue;
            }

            // 2. Progreso de ataques (30s para comprometer)
            int compromised_count = 0;
            for (int r = 0; r < MAX_RECURSOS; r++) {
                if (s->recursos[r].estado == RECURSO_BAJO_ATAQUE) {
                    if (difftime(now, s->recursos[r].inicio_ataque) >= 30) {
                        s->recursos[r].estado = RECURSO_COMPROMETIDO;
                        char msg[128];
                        snprintf(msg, sizeof(msg), "NOTIFY COMPROMISED %s\n", s->recursos[r].id);
                        for (int k = 0; k < MAX_JUGADORES; k++) {
                            if (s->jugadores[k].activo) send(s->jugadores[k].fd, msg, strlen(msg), 0);
                        }
                    }
                }
                if (s->recursos[r].estado == RECURSO_COMPROMETIDO) compromised_count++;
            }

            // 3. Verificar victoria del atacante
            if (compromised_count >= MAX_RECURSOS) {
                s->terminada = 1;
                char msg[128];
                snprintf(msg, sizeof(msg), "NOTIFY GAME_OVER attacker Todos los recursos comprometidos\n");
                for (int k = 0; k < MAX_JUGADORES; k++) {
                    if (s->jugadores[k].activo) send(s->jugadores[k].fd, msg, strlen(msg), 0);
                }
            }
        }
    }
    pthread_mutex_unlock(&mutex);
}

// Genera un string con el estado completo de la sala
void game_status_sala(const char *room_id, char *buffer_out, size_t size) {
    pthread_mutex_lock(&mutex);
    Sala *s = game_buscar_sala(room_id);
    if (s == NULL) {
        strcpy(buffer_out, "ERR_ROOM_NOT_FOUND");
        pthread_mutex_unlock(&mutex);
        return;
    }

    int seg_restantes = 300 - (int)difftime(time(NULL), s->tiempo_inicio);
    if (seg_restantes < 0) seg_restantes = 0;

   
    char players[512] = "";
    for (int i = 0; i < MAX_JUGADORES; i++) {
        if (s->jugadores[i].activo) {
            char p[128];
            snprintf(p, sizeof(p), "%s:%d,%d;", s->jugadores[i].username, s->jugadores[i].x, s->jugadores[i].y);
            strcat(players, p);
        }
    }

    char resources[128] = "";
    for (int i = 0; i < MAX_RECURSOS; i++) {
        char r[64];
        const char *st = (s->recursos[i].estado == RECURSO_SAFE) ? "safe" :
                         (s->recursos[i].estado == RECURSO_BAJO_ATAQUE) ? "under_attack" : "compromised";
        snprintf(r, sizeof(r), "%s:%s;", s->recursos[i].id, st);
        strcat(resources, r);
    }

    snprintf(buffer_out, size, "ROOM=%s TIME_LEFT=%d PLAYERS=%s RESOURCES=%s", s->id, seg_restantes, players, resources);
    pthread_mutex_unlock(&mutex);
}