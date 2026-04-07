#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "protocol.h"

// Recibe un texto como "AUTH juanito\n" y lo convierte en un Mensaje
Mensaje parsear_mensaje(const char *texto) {
    Mensaje msg;
    memset(&msg, 0, sizeof(msg));
    msg.verbo = VERB_UNKNOWN;
    msg.num_params = 0;

    // Copiar el texto para no modificar el original
    char copia[256];
    strncpy(copia, texto, sizeof(copia) - 1);

    // Eliminar el \n del final si existe
    copia[strcspn(copia, "\n")] = '\0';

    // Separar por espacios
    char *token = strtok(copia, " ");
    if (token == NULL) return msg;

    // Identificar el verbo
    if      (strcmp(token, "AUTH")        == 0) msg.verbo = VERB_AUTH;
    else if (strcmp(token, "LIST_ROOMS")  == 0) msg.verbo = VERB_LIST_ROOMS;
    else if (strcmp(token, "CREATE_ROOM") == 0) msg.verbo = VERB_CREATE_ROOM;
    else if (strcmp(token, "JOIN")        == 0) msg.verbo = VERB_JOIN;
    else if (strcmp(token, "MOVE")        == 0) msg.verbo = VERB_MOVE;
    else if (strcmp(token, "SCAN")        == 0) msg.verbo = VERB_SCAN;
    else if (strcmp(token, "ATTACK")      == 0) msg.verbo = VERB_ATTACK;
    else if (strcmp(token, "MITIGATE")    == 0) msg.verbo = VERB_MITIGATE;
    else if (strcmp(token, "STATUS")      == 0) msg.verbo = VERB_STATUS;
    else if (strcmp(token, "PING")        == 0) msg.verbo = VERB_PING;
    else if (strcmp(token, "QUIT")        == 0) msg.verbo = VERB_QUIT;
    else msg.verbo = VERB_UNKNOWN;

    // Leer los parámetros que siguen
    while ((token = strtok(NULL, " ")) != NULL && msg.num_params < 4) {
        strncpy(msg.params[msg.num_params], token, 63);
        msg.num_params++;
    }

    return msg;
}

// Construye una respuesta OK: "OK AUTH juanito\n"
void construir_respuesta(char *buffer, const char *verbo, const char *datos) {
    if (datos != NULL && strlen(datos) > 0)
        snprintf(buffer, 256, "OK %s %s\n", verbo, datos);
    else
        snprintf(buffer, 256, "OK %s\n", verbo);
}

// Construye un error: "ERR 400 \"Verbo desconocido\"\n"
void construir_error(char *buffer, int codigo, const char *descripcion) {
    snprintf(buffer, 256, "ERR %d \"%s\"\n", codigo, descripcion);
}