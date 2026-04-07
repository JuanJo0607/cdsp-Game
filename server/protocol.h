#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdio.h>

// Verbos que el servidor reconoce
typedef enum {
    VERB_AUTH,
    VERB_LIST_ROOMS,
    VERB_CREATE_ROOM,
    VERB_JOIN,
    VERB_MOVE,
    VERB_SCAN,
    VERB_ATTACK,
    VERB_MITIGATE,
    VERB_STATUS,
    VERB_PING,
    VERB_QUIT,
    VERB_UNKNOWN
} Verbo;

// Estructura que representa un mensaje parseado
typedef struct {
    Verbo verbo;
    char params[4][64];  // Máximo 4 parámetros de 64 caracteres cada uno
    int num_params;
} Mensaje;

// Funciones disponibles
Mensaje parsear_mensaje(const char *texto);
void construir_respuesta(char *buffer, const char *verbo, const char *datos);
void construir_error(char *buffer, int codigo, const char *descripcion);

#endif