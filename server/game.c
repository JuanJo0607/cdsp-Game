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
    s->activa = 1;         
    s->estado = SALA_ESPERANDO; 
    s->num_jugadores = 0;
    s->inicio_partida = time(NULL); 
    snprintf(s->id, sizeof(s->id), "room_%03d", sala_counter);

    // Colocar los 2 recursos críticos en posiciones aleatorias 
    for(int i = 0; i < MAX_RECURSOS; i++) {
        snprintf(s->recursos[i].id, sizeof(s->recursos[i].id), "srv_0%d", i+1);
        s->recursos[i].x = rand() % (PLANO_ANCHO - 4) + 2;
        s->recursos[i].y = rand() % (PLANO_ALTO - 4) + 2;
        s->recursos[i].estado = RECURSO_SAFE;
        s->recursos[i].tiempo_ataque = 0;
    }
    
    // Evitar colisión
    if (s->recursos[1].x == s->recursos[0].x && s->recursos[1].y == s->recursos[0].y) {
        s->recursos[1].x = (s->recursos[0].x + 5) % PLANO_ANCHO;
    }

    strncpy(id_out, s->id, 16); 
    pthread_mutex_unlock(&mutex);
    return 0;
}

// Búsqueda de Sala sin protección interna. Asume mutex locked.
Sala *game_buscar_sala(const char *room_id) {
    for (int i = 0; i < MAX_SALAS; i++) {
        if (salas[i].activa && strcmp(salas[i].id, room_id) == 0) return &salas[i];
    }
    return NULL;
}


static Jugador* buscar_jugador(int fd, Sala **sala_ptr) {
    for (int i = 0; i < MAX_SALAS; i++) {
        if (!salas[i].activa) continue;
        for (int j = 0; j < MAX_JUGADORES; j++) {
            if (salas[i].jugadores[j].activo && salas[i].jugadores[j].fd == fd) {
                if (sala_ptr) *sala_ptr = &salas[i]; // Escribir en puntero
                return &salas[i].jugadores[j];
            }
        }
    }
    return NULL; 
}

// Unir un jugador a una sala existente
// Retorna 0 si tuvo éxito, -1 si la sala no existe o está llena
int game_unir_jugador(const char *room_id, int fd, const char *username, const char *rol, const char *ip, int port, int *x_out, int *y_out) {
    pthread_mutex_lock(&mutex);
    Sala *s = game_buscar_sala(room_id);
    if (!s || s->num_jugadores >= MAX_JUGADORES) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    int free_slot = -1;
    for (int i = 0; i < MAX_JUGADORES; i++) {
        if (!s->jugadores[i].activo && free_slot == -1) free_slot = i;
        if (s->jugadores[i].activo && strcmp(s->jugadores[i].username, username) == 0) {
            pthread_mutex_unlock(&mutex);
            return -3; 
        }
    }

    if (free_slot == -1) { pthread_mutex_unlock(&mutex); return -1; }

    Jugador *p = &s->jugadores[free_slot];
    p->fd = fd;
    p->activo = 1;
    p->port = port;
    p->x = rand() % PLANO_ANCHO;
    p->y = rand() % PLANO_ALTO;
    strncpy(p->username, username, 64);
    strncpy(p->ip, ip, 64);
    strncpy(p->rol, rol, 16);
    
    *x_out = p->x;
    *y_out = p->y;
    s->num_jugadores++; 
    
    pthread_mutex_unlock(&mutex);
    return 0;
}

// Llenar buffer_out con la lista de salas activas
void game_listar_salas(char *buffer_out) {
    pthread_mutex_lock(&mutex);
    int count = 0;
    
    for (int i = 0; i < MAX_SALAS; i++) {
        if (salas[i].activa) count++;
    }
    
    int offset = snprintf(buffer_out, 512, "%d", count);
    
    for (int i = 0; i < MAX_SALAS; i++) {
        if (salas[i].activa && (512 - offset > 0)) {
            offset += snprintf(buffer_out + offset, 512 - offset, " %s", salas[i].id);
        }
    }
    pthread_mutex_unlock(&mutex);
}

// Notificar a todos los jugadores en una sala sobre un evento
void game_notificar_sala(const char *room_id, int fd_emisor, const char *mensaje) {
    typedef struct { int fd; char ip[64]; int port; } Target;
    Target targets[MAX_JUGADORES];
    int t_count = 0;

    pthread_mutex_lock(&mutex);
    Sala *s = game_buscar_sala(room_id);
    if (!s) { pthread_mutex_unlock(&mutex); return; }

    for (int i = 0; i < MAX_JUGADORES; i++) {
        if (s->jugadores[i].activo && s->jugadores[i].fd != fd_emisor) {
            targets[t_count].fd = s->jugadores[i].fd;
            strncpy(targets[t_count].ip, s->jugadores[i].ip, 64);
            targets[t_count].port = s->jugadores[i].port;
            t_count++;
        }
    }
    pthread_mutex_unlock(&mutex); 

    for (int i = 0; i < t_count; i++) {
        send(targets[i].fd, mensaje, strlen(mensaje), 0);
        registrar_log(targets[i].ip, targets[i].port, "Broadcast", mensaje);
    }
}

// Desconectar un jugador de cualquier sala donde esté
void game_desconectar_jugador(int fd) {
    typedef struct { int fd; char ip[64]; int port; } Target;
    Target targets[MAX_JUGADORES];
    int t_count = 0;
    char mensaje[128] = "";

    pthread_mutex_lock(&mutex);
    Sala *s = NULL;
    Jugador *p = buscar_jugador(fd, &s);
    
    if (p && s) { 
        snprintf(mensaje, sizeof(mensaje), "NOTIFY PLAYER_LEFT %s\n", p->username);
        
        p->activo = 0; 
        s->num_jugadores--; 
        for (int i = 0; i < MAX_JUGADORES; i++) {
            if (s->jugadores[i].activo) {
                targets[t_count].fd = s->jugadores[i].fd;
                strncpy(targets[t_count].ip, s->jugadores[i].ip, 64);
                targets[t_count].port = s->jugadores[i].port;
                t_count++;
            }
        }
    }
    pthread_mutex_unlock(&mutex);

  
    if (t_count > 0 && strlen(mensaje) > 0) {
        for (int i = 0; i < t_count; i++) {
            send(targets[i].fd, mensaje, strlen(mensaje), 0);
            registrar_log(targets[i].ip, targets[i].port, "Broadcast Left", mensaje);
        }
    }
}

// Implementación de lógica de juego 

int game_mover_jugador(int fd, int dx, int dy, int *nx, int *ny, char *room_id_out) {
    pthread_mutex_lock(&mutex);
    Sala *s; Jugador *p = buscar_jugador(fd, &s);
    if (!p || s->estado == SALA_TERMINADA) { pthread_mutex_unlock(&mutex); return -1; }

    p->x = (p->x + dx < 0) ? 0 : (p->x + dx >= PLANO_ANCHO) ? PLANO_ANCHO - 1 : p->x + dx;
    p->y = (p->y + dy < 0) ? 0 : (p->y + dy >= PLANO_ALTO) ? PLANO_ALTO - 1 : p->y + dy;
    
    *nx = p->x; *ny = p->y;
    strncpy(room_id_out, s->id, 16);
    pthread_mutex_unlock(&mutex);
    return 0; 
}

int game_scan_recurso(int fd, char *res_id_out, int *x_out, int *y_out) {
    pthread_mutex_lock(&mutex);
    Sala *s; Jugador *p = buscar_jugador(fd, &s);
    if (!p || s->estado == SALA_TERMINADA) { pthread_mutex_unlock(&mutex); return -1; }

    for (int r = 0; r < MAX_RECURSOS; r++) {
        Recurso *res = &s->recursos[r];
        if (abs(res->x - p->x) <= 1 && abs(res->y - p->y) <= 1) { 
            strncpy(res_id_out, res->id, 16);
            *x_out = res->x; *y_out = res->y;
            pthread_mutex_unlock(&mutex);
            return 0; 
        }
    }
    pthread_mutex_unlock(&mutex);
    return -1; 
}

static int interactuar_recurso(int fd, const char *res_id, char *room_id_out, char *real_id_out, int es_ataque) {
    pthread_mutex_lock(&mutex);
    Sala *s; Jugador *p = buscar_jugador(fd, &s);
    if (!p || s->estado == SALA_TERMINADA) { pthread_mutex_unlock(&mutex); return -1; }

    if (es_ataque && strcmp(p->rol, "atacante") != 0) { pthread_mutex_unlock(&mutex); return -2; }
    if (!es_ataque && strcmp(p->rol, "defensor") != 0) { pthread_mutex_unlock(&mutex); return -2; }
    
    for (int r = 0; r < MAX_RECURSOS; r++) {
        Recurso *res = &s->recursos[r];
        
        if ((!res_id || strlen(res_id) == 0) || strcmp(res->id, res_id) == 0) {
            
            if (res->x != p->x || res->y != p->y) { 
                if (res_id && strlen(res_id) > 0) {
                    pthread_mutex_unlock(&mutex); return -3; 
                } else {
                    continue; // Ignorar y seguir buscando el recurso que realmente está pisando
                }
            } 
            
            if (es_ataque && res->estado != RECURSO_SAFE) { pthread_mutex_unlock(&mutex); return -4; } 
            if (!es_ataque && res->estado != RECURSO_BAJO_ATAQUE) { pthread_mutex_unlock(&mutex); return -4; }

            res->estado = es_ataque ? RECURSO_BAJO_ATAQUE : RECURSO_SAFE;
            if (es_ataque) res->tiempo_ataque = time(NULL);
            strncpy(room_id_out, s->id, 16);
            if (real_id_out) strncpy(real_id_out, res->id, 16);
            
            pthread_mutex_unlock(&mutex);
            return 0; // Win
        }
    }
    pthread_mutex_unlock(&mutex);
    return -5; // NOT FOUND
}

int game_atacar_recurso(int fd, const char *res_id, char *room_id_out, char *real_id_out) { return interactuar_recurso(fd, res_id, room_id_out, real_id_out, 1); }
int game_mitigar_recurso(int fd, const char *res_id, char *room_id_out, char *real_id_out) { return interactuar_recurso(fd, res_id, room_id_out, real_id_out, 0); }

void game_obtener_estado_jugador(int fd, char *buffer_out) {
    pthread_mutex_lock(&mutex);
    Sala *s; Jugador *p = buscar_jugador(fd, &s);
    if (!p) {
        strcpy(buffer_out, "ERR 401 Autentiquese primero"); 
    } else {
        char res_info[256] = ""; 
        for (int r = 0; r < MAX_RECURSOS; r++) {
            const char *st = (s->recursos[r].estado == RECURSO_SAFE) ? "safe" : 
                              (s->recursos[r].estado == RECURSO_BAJO_ATAQUE) ? "under_attack" :
                              (s->recursos[r].estado == RECURSO_MITIGADO) ? "mitigated" : "compromised";
            snprintf(res_info + strlen(res_info), sizeof(res_info) - strlen(res_info), " %s=%s", s->recursos[r].id, st);
        }
        snprintf(buffer_out, 512, "%d %d%s", p->x, p->y, res_info); 
    }
    pthread_mutex_unlock(&mutex);
}

const char* game_obtener_rol(int fd) {
     pthread_mutex_lock(&mutex);
     Sala *s; Jugador *p = buscar_jugador(fd, &s);
     
     const char *r = "cliente"; 
     if (p != NULL) {
         if (strcmp(p->rol, "atacante") == 0) r = "atacante";
         else if (strcmp(p->rol, "defensor") == 0) r = "defensor";
     }
     pthread_mutex_unlock(&mutex);
     return r;
}

void game_tick() {
    typedef struct { int fd; char ip[64]; int port; char msg[128]; } TargetMsg;
    TargetMsg targets[MAX_SALAS * MAX_JUGADORES];
    int t_count = 0;

    pthread_mutex_lock(&mutex); 
    time_t now = time(NULL); 

    for (int i = 0; i < MAX_SALAS; i++) {
        Sala *s = &salas[i];
        if (!s->activa || s->estado == SALA_TERMINADA) continue;

        int compromised_count = 0; 
        for (int r = 0; r < MAX_RECURSOS; r++) {
            Recurso *res = &s->recursos[r];
            if (res->estado == RECURSO_BAJO_ATAQUE && difftime(now, res->tiempo_ataque) >= 30.0) {
                res->estado = RECURSO_COMPROMETIDO; 
                char nt[128]; snprintf(nt, sizeof(nt), "NOTIFY COMPROMISED %s \"Recurso hackeado\"\n", res->id);
                for (int k = 0; k < MAX_JUGADORES; k++) {
                    if (s->jugadores[k].activo && s->jugadores[k].fd > 0 && t_count < MAX_SALAS * MAX_JUGADORES) {
                        targets[t_count].fd = s->jugadores[k].fd;
                        strncpy(targets[t_count].ip, s->jugadores[k].ip, 64);
                        targets[t_count].port = s->jugadores[k].port;
                        strncpy(targets[t_count].msg, nt, 128);
                        t_count++;
                    }
                }
            }
            if (res->estado == RECURSO_COMPROMETIDO) compromised_count++;
        }

        char *msg_fin = NULL;
        if (compromised_count == MAX_RECURSOS) {
            msg_fin = "NOTIFY GAME_OVER atacante \"Atacante gana\"\n";
        } else if (difftime(now, s->inicio_partida) >= 300.0) {
            msg_fin = "NOTIFY GAME_OVER defensor \"Defensores ganan por tiempo\"\n";
        }

        if (msg_fin != NULL) {
            s->estado = SALA_TERMINADA; 
            for (int k = 0; k < MAX_JUGADORES; k++) {
                if (s->jugadores[k].activo && s->jugadores[k].fd > 0 && t_count < MAX_SALAS * MAX_JUGADORES) {
                     targets[t_count].fd = s->jugadores[k].fd;
                     strncpy(targets[t_count].ip, s->jugadores[k].ip, 64);
                     targets[t_count].port = s->jugadores[k].port;
                     strncpy(targets[t_count].msg, msg_fin, 128);
                     t_count++;
                }
            }
            s->activa = 0; 
        }
    }
    pthread_mutex_unlock(&mutex);

    for (int i = 0; i < t_count; i++) {
        send(targets[i].fd, targets[i].msg, strlen(targets[i].msg), 0);
        registrar_log(targets[i].ip, targets[i].port, "Broadcast Tick", targets[i].msg);
    }
}