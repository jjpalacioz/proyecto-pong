"""
protocol.py  —  Constantes del protocolo MyAppGameProtocol (lado cliente)
============================================================================
Este archivo es la traduccion a Python de:
    - server/include/protocol.h   (version C, del servidor)
    - common/protocol_spec.md     (la fuente unica de verdad)

Cliente (Python) y servidor (C) DEBEN usar EXACTAMENTE los mismos valores, o
no se entenderan. Si aqui cambias un numero, tambien debe cambiar en el
servidor C (y viceversa).
"""

# ----------------------------------------------------------------------------
#  Header del protocolo
# ----------------------------------------------------------------------------
# Numero magico: 0x5047 = letras "PG" (PonG). Va al inicio de CADA mensaje.
PROTO_MAGIC = 0x5047
# Version actual del protocolo.
PROTO_VERSION = 0x01
# Tamano fijo del header: MAGIC(2) + VERSION(1) + TYPE(1) + LENGTH(2) = 6 bytes.
HEADER_SIZE = 6
# Tamano maximo del payload (cabe en el campo LENGTH de 2 bytes).
MAX_PAYLOAD = 65535

# ----------------------------------------------------------------------------
#  Tipos de mensaje (campo TYPE del header)
# ----------------------------------------------------------------------------
# --- Registro ---
MSG_REGISTER     = 0x01  # C->S: nickname + email
MSG_REGISTER_OK  = 0x02  # S->C: registro aceptado, entrega player_id
MSG_REGISTER_ERR = 0x03  # S->C: registro rechazado (con codigo de error)

# --- Emparejamiento ---
MSG_QUEUE       = 0x10   # C->S: pedir entrar a la cola de emparejamiento
MSG_MATCH_FOUND = 0x11   # S->C: se encontro rival, datos de la partida

# --- Juego ---
MSG_INPUT      = 0x20    # C->S: direccion de la paleta (arriba/abajo/quieto)
MSG_STATE      = 0x21    # S->C: estado del juego (pelota, paletas, score)
MSG_GAME_START = 0x22    # S->C: la partida comienza
MSG_GAME_OVER  = 0x23    # S->C: la partida termino (ganador + score final)

# --- Keepalive ---
MSG_PING = 0x30          # C->S: verificar que la conexion sigue viva
MSG_PONG = 0x31          # S->C: respuesta al ping

# --- Control ---
MSG_ERROR      = 0x40    # S->C: error generico del protocolo
MSG_DISCONNECT = 0x41    # C<->S: aviso de desconexion ordenada

# ----------------------------------------------------------------------------
#  Direcciones de input (payload de MSG_INPUT)
# ----------------------------------------------------------------------------
DIR_NONE = 0  # paleta quieta
DIR_UP   = 1  # mover paleta hacia arriba
DIR_DOWN = 2  # mover paleta hacia abajo

# ----------------------------------------------------------------------------
#  Lados del jugador
# ----------------------------------------------------------------------------
SIDE_LEFT  = 0  # jugador de la izquierda
SIDE_RIGHT = 1  # jugador de la derecha

# ----------------------------------------------------------------------------
#  Codigos de error (payload de MSG_ERROR y MSG_REGISTER_ERR)
# ----------------------------------------------------------------------------
ERR_BAD_MAGIC      = 0x01
ERR_BAD_VERSION    = 0x02
ERR_UNKNOWN_TYPE   = 0x03
ERR_BAD_LENGTH     = 0x04
ERR_NICK_TAKEN     = 0x05
ERR_INVALID_FIELD  = 0x06
ERR_NOT_REGISTERED = 0x07
ERR_SERVER_FULL    = 0x08

# Diccionario para traducir un codigo de error a un texto legible (para la GUI).
ERROR_MESSAGES = {
    ERR_BAD_MAGIC:      "Numero magico invalido",
    ERR_BAD_VERSION:    "Version de protocolo no soportada",
    ERR_UNKNOWN_TYPE:   "Tipo de mensaje desconocido",
    ERR_BAD_LENGTH:     "Longitud de mensaje invalida",
    ERR_NICK_TAKEN:     "El nickname ya esta en uso",
    ERR_INVALID_FIELD:  "Campo invalido (revisa nickname/email)",
    ERR_NOT_REGISTERED: "Debes registrarte primero",
    ERR_SERVER_FULL:    "El servidor esta lleno",
}

# ----------------------------------------------------------------------------
#  Parametros del juego (los define el servidor; aqui solo para dibujar a escala)
# ----------------------------------------------------------------------------
FIELD_WIDTH   = 800  # ancho del campo, en unidades del servidor
FIELD_HEIGHT  = 600  # alto del campo
PADDLE_HEIGHT = 100  # alto de la paleta
WINNING_SCORE = 5    # puntos para ganar
