#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include "game.h"

extern void registrar_log(const char *ip, int port, const char *sent_recv, const char *msg);

// Estado global del juego
static Sala salas[MAX_SALAS];
static int  sala_counter = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Inicializar el estado del juego
void game_init() {
    memset(salas, 0, sizeof(salas));
    sala_counter = 0;
    srand(time(NULL));
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

    // Colocar los 2 recursos críticos en posiciones aleatorias 
    // Evitamos los bordes (distancia de 2 celdas)
    strncpy(s->recursos[0].id, "srv_01", sizeof(s->recursos[0].id));
    s->recursos[0].x = rand() % (PLANO_ANCHO - 4) + 2;
    s->recursos[0].y = rand() % (PLANO_ALTO - 4) + 2;
    s->recursos[0].estado = RECURSO_SAFE;
    s->recursos[0].tiempo_ataque = 0;

    strncpy(s->recursos[1].id, "srv_02", sizeof(s->recursos[1].id));
    s->recursos[1].x = rand() % (PLANO_ANCHO - 4) + 2;
    s->recursos[1].y = rand() % (PLANO_ALTO - 4) + 2;
    s->recursos[1].estado = RECURSO_SAFE;
    s->recursos[1].tiempo_ataque = 0;
    
    // Asegurar que no caigan en el mismo sitio
    if (s->recursos[1].x == s->recursos[0].x && s->recursos[1].y == s->recursos[0].y) {
        s->recursos[1].x = (s->recursos[0].x + 5) % PLANO_ANCHO;
    }

    strncpy(id_out, s->id, 16);

    pthread_mutex_unlock(&mutex);
    return 0;
}

// Unir un jugador a una sala existente
// Retorna 0 si tuvo éxito, -1 si la sala no existe o está llena
int game_unir_jugador(const char *room_id, int fd, const char *username, const char *rol, const char *ip, int port, int *x_out, int *y_out) {
    pthread_mutex_lock(&mutex);
    
    Sala *s = game_buscar_sala(room_id);
    if (s == NULL) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    // Verificar nombre duplicado en la sala
    for (int i = 0; i < MAX_JUGADORES; i++) {
        if (s->jugadores[i].activo && strcmp(s->jugadores[i].username, username) == 0) {
            pthread_mutex_unlock(&mutex);
            return -3; // 409 Conflict / Duplicado
        }
    }

    if (s->num_jugadores >= MAX_JUGADORES) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    int free_slot = -1;
    for (int i = 0; i < MAX_JUGADORES; i++) {
        if (!s->jugadores[i].activo) {
            free_slot = i;
            break;
        }
    }

    if (free_slot == -1) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    strncpy(s->jugadores[free_slot].username, username, 64);
    s->jugadores[free_slot].fd = fd;
    strncpy(s->jugadores[free_slot].ip, ip, 64);
    s->jugadores[free_slot].port = port;
    s->jugadores[free_slot].activo = 1;
    strncpy(s->jugadores[free_slot].rol, rol, 16);
    
    // Coordenadas iniciales
    s->jugadores[free_slot].x = rand() % PLANO_ANCHO;
    s->jugadores[free_slot].y = rand() % PLANO_ALTO;
    
    *x_out = s->jugadores[free_slot].x;
    *y_out = s->jugadores[free_slot].y;
    
    s->num_jugadores++;
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

void game_notificar_sala(const char *room_id, int fd_emisor, const char *mensaje) {
    typedef struct { int fd; char ip[64]; int port; } Target;
    Target targets[MAX_JUGADORES];
    int t_count = 0;

    pthread_mutex_lock(&mutex);

    Sala *s = game_buscar_sala(room_id);
    if (s != NULL) {
        for (int i = 0; i < MAX_JUGADORES; i++) {
            if (s->jugadores[i].activo && s->jugadores[i].fd != fd_emisor) {
                targets[t_count].fd = s->jugadores[i].fd;
                strncpy(targets[t_count].ip, s->jugadores[i].ip, 64);
                targets[t_count].port = s->jugadores[i].port;
                t_count++;
            }
        }
    }

    pthread_mutex_unlock(&mutex);

    for (int i = 0; i < t_count; i++) {
        send(targets[i].fd, mensaje, strlen(mensaje), 0);
        registrar_log(targets[i].ip, targets[i].port, "Enviado", mensaje);
    }
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
                if (salas[i].estado == SALA_TERMINADA) {
                    pthread_mutex_unlock(&mutex);
                    return -1;
                }
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

int game_scan_recurso(int fd, char *res_id_out, int *x_out, int *y_out) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_SALAS; i++) {
        if (!salas[i].activa) continue;
        for (int j = 0; j < MAX_JUGADORES; j++) {
            if (salas[i].jugadores[j].activo && salas[i].jugadores[j].fd == fd) {
                if (salas[i].estado == SALA_TERMINADA) {
                    pthread_mutex_unlock(&mutex);
                    return -1;
                }
                Jugador *player = &salas[i].jugadores[j];
                
                // Vecindario de Moore (dx, dy <= 1)
                for (int r = 0; r < MAX_RECURSOS; r++) {
                    Recurso *res = &salas[i].recursos[r];
                    if (abs(res->x - player->x) <= 1 && abs(res->y - player->y) <= 1) {
                        strncpy(res_id_out, res->id, 16);
                        *x_out = res->x;
                        *y_out = res->y;
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
    return -2;
}

int game_atacar_recurso(int fd, const char *res_id, char *room_id_out) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_SALAS; i++) {
        if (!salas[i].activa) continue;
        for (int j = 0; j < MAX_JUGADORES; j++) {
            if (salas[i].jugadores[j].activo && salas[i].jugadores[j].fd == fd) {
                if (salas[i].estado == SALA_TERMINADA) {
                    pthread_mutex_unlock(&mutex);
                    return -1;
                }
                Jugador *p = &salas[i].jugadores[j];
                
                if (strcmp(p->rol, "atacante") != 0) {
                    pthread_mutex_unlock(&mutex);
                    return -2; // 403
                }
                
                if (res_id == NULL || strlen(res_id) == 0) {
                    for (int r = 0; r < MAX_RECURSOS; r++) {
                        Recurso *res = &salas[i].recursos[r];
                        if (res->x == p->x && res->y == p->y) {
                            if (res->estado != RECURSO_SAFE) {
                                pthread_mutex_unlock(&mutex);
                                return -4;
                            }
                            res->estado = RECURSO_BAJO_ATAQUE;
                            res->tiempo_ataque = time(NULL);
                            strncpy(room_id_out, salas[i].id, 16);
                            pthread_mutex_unlock(&mutex);
                            return 0;
                        }
                    }
                    pthread_mutex_unlock(&mutex);
                    return -1;
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
        if (salas[i].estado == SALA_TERMINADA) {
            pthread_mutex_unlock(&mutex);
            return -1;
        }
        for (int j = 0; j < MAX_JUGADORES; j++) {
            if (salas[i].jugadores[j].activo && salas[i].jugadores[j].fd == fd) {
                Jugador *p = &salas[i].jugadores[j];
                
                if (strcmp(p->rol, "defensor") != 0) {
                    pthread_mutex_unlock(&mutex);
                    return -2; // 403
                }
                
                // Si res_id está vacío, buscamos por posición
                if (res_id == NULL || strlen(res_id) == 0) {
                    for (int r = 0; r < MAX_RECURSOS; r++) {
                        Recurso *res = &salas[i].recursos[r];
                        if (res->x == p->x && res->y == p->y) {
                            if (res->estado != RECURSO_BAJO_ATAQUE) {
                                pthread_mutex_unlock(&mutex);
                                return -4; // ERR 411
                            }
                            res->estado = RECURSO_SAFE;
                            strncpy(room_id_out, salas[i].id, 16);
                            pthread_mutex_unlock(&mutex);
                            return 0;
                        }
                    }
                    pthread_mutex_unlock(&mutex);
                    return -5; // No hay recurso acá (ERR 404)
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
                        
                        res->estado = RECURSO_SAFE;
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
    typedef struct { int fd; char ip[64]; int port; char msg[128]; } Target;
    Target targets[MAX_SALAS * MAX_JUGADORES];
    int t_count = 0;

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
                        if (salas[i].jugadores[k].activo && t_count < MAX_SALAS * MAX_JUGADORES) {
                            targets[t_count].fd = salas[i].jugadores[k].fd;
                            strncpy(targets[t_count].ip, salas[i].jugadores[k].ip, 64);
                            targets[t_count].port = salas[i].jugadores[k].port;
                            strncpy(targets[t_count].msg, nt, 128);
                            t_count++;
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
                if (salas[i].jugadores[k].activo && t_count < MAX_SALAS * MAX_JUGADORES) {
                    targets[t_count].fd = salas[i].jugadores[k].fd;
                    strncpy(targets[t_count].ip, salas[i].jugadores[k].ip, 64);
                    targets[t_count].port = salas[i].jugadores[k].port;
                    strncpy(targets[t_count].msg, msg, 128);
                    t_count++;
                }
            }
            salas[i].activa = 0; // Liberar sala
        }

        // Tiempo agotado? (5 minutos = 300s)
        else if (difftime(now, salas[i].inicio_partida) >= 300.0) {
            salas[i].estado = SALA_TERMINADA;
            char msg[128] = "NOTIFY GAME_OVER defensor \"Defensa exitosa: Se acabo el tiempo\"\n";
            for (int k = 0; k < MAX_JUGADORES; k++) {
                if (salas[i].jugadores[k].activo && t_count < MAX_SALAS * MAX_JUGADORES) {
                    targets[t_count].fd = salas[i].jugadores[k].fd;
                    strncpy(targets[t_count].ip, salas[i].jugadores[k].ip, 64);
                    targets[t_count].port = salas[i].jugadores[k].port;
                    strncpy(targets[t_count].msg, msg, 128);
                    t_count++;
                }
            }
            salas[i].activa = 0; // Liberar sala
        }
    }
    pthread_mutex_unlock(&mutex);

    for (int i = 0; i < t_count; i++) {
        send(targets[i].fd, targets[i].msg, strlen(targets[i].msg), 0);
        registrar_log(targets[i].ip, targets[i].port, "Enviado", targets[i].msg);
    }
}