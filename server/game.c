#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
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
    s->inicio_partida = time(NULL);
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

    snprintf(buffer_out, 512, "%d %s", count, ids);
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
    char username_copia[64] = "";
    char room_id_copia[16] = "";
    int found = 0;

    for (int i = 0; i < MAX_SALAS; i++) {
        if (!salas[i].activa) continue;
        for (int j = 0; j < MAX_JUGADORES; j++) {
            if (salas[i].jugadores[j].activo && salas[i].jugadores[j].fd == fd) {
                strncpy(username_copia, salas[i].jugadores[j].username, 64);
                strncpy(room_id_copia, salas[i].id, 16);
                salas[i].jugadores[j].activo = 0;
                salas[i].num_jugadores--;
                found = 1;
                break;
            }
        }
        if (found) break;
    }
    pthread_mutex_unlock(&mutex);

    if (found) {
        char mensaje[128];
        snprintf(mensaje, 128, "NOTIFY PLAYER_LEFT %s\n", username_copia);
        game_notificar_sala(room_id_copia, -1, mensaje);
    }
}

// Implementación de lógica de juego 

int game_mover_jugador(int fd, int dx, int dy, int *nx, int *ny, char *room_id_out) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_SALAS; i++) {
        if (!salas[i].activa) continue;
        for (int j = 0; j < MAX_JUGADORES; j++) {
            if (salas[i].jugadores[j].activo && salas[i].jugadores[j].fd == fd) {
                Jugador *player = &salas[i].jugadores[j];
                
                // Nuevas coordenadas
                int proposed_x = player->x + dx;
                int proposed_y = player->y + dy;
                
                // Validar límites
                if (proposed_x < 0) proposed_x = 0;
                if (proposed_x >= PLANO_ANCHO) proposed_x = PLANO_ANCHO - 1;
                if (proposed_y < 0) proposed_y = 0;
                if (proposed_y >= PLANO_ALTO) proposed_y = PLANO_ALTO - 1;

                player->x = proposed_x;
                player->y = proposed_y;
                *nx = player->x;
                *ny = player->y;
                strncpy(room_id_out, salas[i].id, 16);
                
                pthread_mutex_unlock(&mutex);
                return 0;
            }
        }
    }
    pthread_mutex_unlock(&mutex);
    return -1;
}

int game_scan_recurso(int fd, char *res_id_out, int *rx, int *ry) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_SALAS; i++) {
        if (!salas[i].activa) continue;
        for (int j = 0; j < MAX_JUGADORES; j++) {
            if (salas[i].jugadores[j].activo && salas[i].jugadores[j].fd == fd) {
                Jugador *player = &salas[i].jugadores[j];
                
                // Ahora AMBOS pueden SCAN
                // Vecindario de Moore (dx, dy <= 1)
                for (int r = 0; r < MAX_RECURSOS; r++) {
                    Recurso *res = &salas[i].recursos[r];
                    if (abs(res->x - player->x) <= 1 && abs(res->y - player->y) <= 1) {
                        strncpy(res_id_out, res->id, 16);
                        *rx = res->x;
                        *ry = res->y;
                        pthread_mutex_unlock(&mutex);
                        return 0;
                    }
                }
                pthread_mutex_unlock(&mutex);
                return -1; // NOT FOUND
            }
        }
    }
    pthread_mutex_unlock(&mutex);
    return -3; // NOT IN ROOM
}

int game_atacar_recurso(int fd, const char *res_id, char *room_id_out) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_SALAS; i++) {
        if (!salas[i].activa) continue;
        for (int j = 0; j < MAX_JUGADORES; j++) {
            if (salas[i].jugadores[j].activo && salas[i].jugadores[j].fd == fd) {
                Jugador *p = &salas[i].jugadores[j];
                
                if (strcmp(p->rol, "atacante") != 0) {
                    pthread_mutex_unlock(&mutex);
                    return -2; // 403
                }
                
                for (int r = 0; r < MAX_RECURSOS; r++) {
                    Recurso *res = &salas[i].recursos[r];
                    if (strcmp(res->id, res_id) == 0) {
                        // Misma celda?
                        if (res->x != p->x || res->y != p->y) {
                            pthread_mutex_unlock(&mutex);
                            return -3; // 410 (fuera)
                        }
                        // Estado safe?
                        if (res->estado != RECURSO_SAFE) {
                            pthread_mutex_unlock(&mutex);
                            return -4; // 411 (ya bajo ataque)
                        }
                        
                        res->estado = RECURSO_BAJO_ATAQUE;
                        res->tiempo_ataque = time(NULL);
                        strncpy(room_id_out, salas[i].id, 16);
                        pthread_mutex_unlock(&mutex);
                        return 0;
                    }
                }
                pthread_mutex_unlock(&mutex);
                return -1; // NOT FOUND 404
            }
        }
    }
    pthread_mutex_unlock(&mutex);
    return -5;
}

int game_mitigar_recurso(int fd, const char *res_id, char *room_id_out) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_SALAS; i++) {
        if (!salas[i].activa) continue;
        for (int j = 0; j < MAX_JUGADORES; j++) {
            if (salas[i].jugadores[j].activo && salas[i].jugadores[j].fd == fd) {
                Jugador *p = &salas[i].jugadores[j];
                
                if (strcmp(p->rol, "defensor") != 0) {
                    pthread_mutex_unlock(&mutex);
                    return -2; // 403
                }
                
                for (int r = 0; r < MAX_RECURSOS; r++) {
                    Recurso *res = &salas[i].recursos[r];
                    if (strcmp(res->id, res_id) == 0) {
                        if (res->x != p->x || res->y != p->y) {
                            pthread_mutex_unlock(&mutex);
                            return -3; // 410
                        }
                        if (res->estado != RECURSO_BAJO_ATAQUE) {
                            pthread_mutex_unlock(&mutex);
                            return -4; // 411
                        }
                        
                        res->estado = RECURSO_MITIGADO;
                        strncpy(room_id_out, salas[i].id, 16);
                        pthread_mutex_unlock(&mutex);
                        return 0;
                    }
                }
                pthread_mutex_unlock(&mutex);
                return -1; // 404
            }
        }
    }
    pthread_mutex_unlock(&mutex);
    return -5;
}

void game_obtener_estado_jugador(int fd, char *buffer_out) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_SALAS; i++) {
        if (!salas[i].activa) continue;
        for (int j = 0; j < MAX_JUGADORES; j++) {
            if (salas[i].jugadores[j].activo && salas[i].jugadores[j].fd == fd) {
                Jugador *p = &salas[i].jugadores[j];
                char pos_info[64];
                snprintf(pos_info, 64, "%d %d", p->x, p->y);
                
                char res_info[128] = "";
                for (int r = 0; r < MAX_RECURSOS; r++) {
                    char temp[32];
                    const char *st = (salas[i].recursos[r].estado == RECURSO_SAFE) ? "safe" : 
                                      (salas[i].recursos[r].estado == RECURSO_BAJO_ATAQUE) ? "under_attack" :
                                      (salas[i].recursos[r].estado == RECURSO_MITIGADO) ? "mitigated" : "compromised";
                    snprintf(temp, 32, " %s=%s", salas[i].recursos[r].id, st);
                    strcat(res_info, temp);
                }
                
                snprintf(buffer_out, 512, "%s%s", pos_info, res_info);
                pthread_mutex_unlock(&mutex);
                return;
            }
        }
    }
    pthread_mutex_unlock(&mutex);
    strcpy(buffer_out, "ERR 401 Autentiquese primero");
}

const char* game_obtener_rol(int fd) {
    // Nota: El puntero retornado es a un recurso compartido, se debe retornar estático o copiar.
    // Para brevedad y seguridad en este ejercicio, implementaremos búsqueda rápida sin persistencia de puntero.
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_SALAS; i++) {
        if (!salas[i].activa) continue;
        for (int j = 0; j < MAX_JUGADORES; j++) {
            if (salas[i].jugadores[j].activo && salas[i].jugadores[j].fd == fd) {
                const char* r = salas[i].jugadores[j].rol;
                if (strcmp(r, "atacante") == 0) { pthread_mutex_unlock(&mutex); return "atacante"; }
                if (strcmp(r, "defensor") == 0) { pthread_mutex_unlock(&mutex); return "defensor"; }
            }
        }
    }
    pthread_mutex_unlock(&mutex);
    return "cliente"; // Rol default/fuera de sala
}

void game_tick() {
    pthread_mutex_lock(&mutex);
    time_t now = time(NULL);

    for (int i = 0; i < MAX_SALAS; i++) {
        if (!salas[i].activa || salas[i].estado == SALA_TERMINADA) continue;

        int compromised_count = 0;
        for (int r = 0; r < MAX_RECURSOS; r++) {
            Recurso *res = &salas[i].recursos[r];
            
            if (res->estado == RECURSO_BAJO_ATAQUE) {
                if (difftime(now, res->tiempo_ataque) >= 30.0) {
                    res->estado = RECURSO_COMPROMETIDO;
                    
                    char nt[128];
                    snprintf(nt, 128, "NOTIFY COMPROMISED %s \"Recurso perdido por tiempo\"\n", res->id);
                    for (int k = 0; k < MAX_JUGADORES; k++) {
                        if (salas[i].jugadores[k].activo) {
                            send(salas[i].jugadores[k].fd, nt, strlen(nt), 0);
                        }
                    }
                }
            }

            if (res->estado == RECURSO_COMPROMETIDO) {
                compromised_count++;
            }
        }

        // Victoria Atacante?
        if (compromised_count == MAX_RECURSOS) {
            salas[i].estado = SALA_TERMINADA;
            char msg[128] = "NOTIFY GAME_OVER atacante \"Atacante ha comprometido todo el sistema\"\n";
            for (int k = 0; k < MAX_JUGADORES; k++) {
                if (salas[i].jugadores[k].activo) {
                    send(salas[i].jugadores[k].fd, msg, strlen(msg), 0);
                }
            }
        }

        // Tiempo agotado? (5 minutos = 300s)
        if (difftime(now, salas[i].inicio_partida) >= 300.0) {
            salas[i].estado = SALA_TERMINADA;
            char msg[128] = "NOTIFY GAME_OVER defensor \"Tiempo de mision agotado\"\n";
            for (int k = 0; k < MAX_JUGADORES; k++) {
                if (salas[i].jugadores[k].activo) {
                    send(salas[i].jugadores[k].fd, msg, strlen(msg), 0);
                }
            }
        }
    }
    pthread_mutex_unlock(&mutex);
}