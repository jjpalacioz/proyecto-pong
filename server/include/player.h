/* ============================================================================
 *  player.h  —  Representacion del perfil de un jugador
 * ----------------------------------------------------------------------------
 *  Cuando un cliente se registra, el servidor guarda su perfil: nickname,
 *  email y un identificador unico (player_id).
 * ========================================================================== */
#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>

/* Limites de tamano de los campos de texto (en bytes).
 * El protocolo permite hasta 255 (1 byte de longitud), pero acotamos a algo
 * razonable. El +1 es para el caracter terminador '\0' de las cadenas en C. */
#define MAX_NICK_LEN   32
#define MAX_EMAIL_LEN  64

/* Perfil de un jugador registrado. */
typedef struct {
    uint32_t id;                        /* identificador unico asignado por el servidor */
    char     nickname[MAX_NICK_LEN + 1];
    char     email[MAX_EMAIL_LEN + 1];
} Player;

#endif /* PLAYER_H */
