# Constantes del protocolo (referencia única)

Este archivo lista TODOS los valores numéricos del protocolo en un solo lugar.
Cuando implementemos el servidor (C) y el cliente (Python), ambos definirán estas
mismas constantes en su propio lenguaje. Este documento es la "fuente de verdad".

## Header
| Constante | Valor | Notas |
|-----------|-------|-------|
| `PROTO_MAGIC`   | `0x5047` | "PG" — 2 bytes, big-endian |
| `PROTO_VERSION` | `0x01`   | versión actual |
| `HEADER_SIZE`   | `6`      | bytes fijos del header |

## Tipos de mensaje (TYPE)
| Constante | Valor |
|-----------|-------|
| `MSG_REGISTER`     | `0x01` |
| `MSG_REGISTER_OK`  | `0x02` |
| `MSG_REGISTER_ERR` | `0x03` |
| `MSG_QUEUE`        | `0x10` |
| `MSG_MATCH_FOUND`  | `0x11` |
| `MSG_INPUT`        | `0x20` |
| `MSG_STATE`        | `0x21` |
| `MSG_GAME_START`   | `0x22` |
| `MSG_GAME_OVER`    | `0x23` |
| `MSG_PING`         | `0x30` |
| `MSG_PONG`         | `0x31` |
| `MSG_ERROR`        | `0x40` |
| `MSG_DISCONNECT`   | `0x41` |

## Direcciones de input (MSG_INPUT)
| Constante | Valor |
|-----------|-------|
| `DIR_NONE` | `0` |
| `DIR_UP`   | `1` |
| `DIR_DOWN` | `2` |

## Lados del jugador
| Constante | Valor |
|-----------|-------|
| `SIDE_LEFT`  | `0` |
| `SIDE_RIGHT` | `1` |

## Códigos de error
| Constante | Valor |
|-----------|-------|
| `ERR_BAD_MAGIC`      | `0x01` |
| `ERR_BAD_VERSION`    | `0x02` |
| `ERR_UNKNOWN_TYPE`   | `0x03` |
| `ERR_BAD_LENGTH`     | `0x04` |
| `ERR_NICK_TAKEN`     | `0x05` |
| `ERR_INVALID_FIELD`  | `0x06` |
| `ERR_NOT_REGISTERED` | `0x07` |
| `ERR_SERVER_FULL`    | `0x08` |

## Parámetros del juego (definidos por el servidor)
| Constante | Valor propuesto | Notas |
|-----------|-----------------|-------|
| `FIELD_WIDTH`   | `800` | ancho del campo en unidades |
| `FIELD_HEIGHT`  | `600` | alto del campo |
| `PADDLE_HEIGHT` | `100` | alto de la paleta |
| `WINNING_SCORE` | `5`   | puntos para ganar |
| `TICK_RATE`     | `60`  | actualizaciones por segundo del servidor |
