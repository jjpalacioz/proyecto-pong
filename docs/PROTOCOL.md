# MyAppGameProtocol — Especificación del Protocolo

**Versión:** 1.0
**Capa:** Aplicación (sobre TCP/IP, usando la API de Sockets Berkeley)
**Codificación:** Binaria (no texto)

Este documento define el protocolo de comunicación entre el cliente (`PongClient`,
en Python) y el servidor (`PongServer`, en C). Es el **contrato**: ambos lados
deben interpretar cada byte exactamente igual.

---

## 1. Especificación del servicio

`MyAppGameProtocol` ofrece los siguientes servicios entre cliente y servidor:

1. **Registro de jugador:** el cliente se identifica con nickname y email.
2. **Emparejamiento (matchmaking):** el servidor empareja dos jugadores en una partida.
3. **Envío de entrada (input):** el cliente informa qué tecla presiona (mover paleta).
4. **Difusión de estado:** el servidor informa a ambos clientes el estado del juego
   (posición de pelota, paletas y **score en tiempo real**).
5. **Notificación de eventos:** inicio de partida, punto anotado, fin de partida,
   desconexión del rival.
6. **Manejo de errores:** el servidor reporta condiciones de error de forma explícita.

El servicio es **orientado a conexión** (TCP): cada cliente mantiene una conexión
TCP persistente con el servidor durante toda la sesión.

---

## 2. ¿Por qué binario y no texto?

- **Eficiencia:** un entero de 2 bytes ocupa 2 bytes; como texto ("32767") ocupa
  más y hay que parsearlo.
- **Tamaño fijo y predecible:** los campos binarios tienen tamaño conocido, lo que
  hace el parsing más simple y rápido, ideal para un juego en tiempo real.
- **Requisito del proyecto:** el líder técnico exige codificación binaria.

---

## 3. El problema del "framing" en TCP (¡importante!)

TCP es un **flujo de bytes (stream)**, NO un protocolo de mensajes. Esto significa
que si el servidor envía dos mensajes seguidos, el receptor puede leerlos:
- pegados en una sola lectura, o
- partidos en varias lecturas.

TCP **no marca** dónde empieza o termina un "mensaje" lógico. Por eso nuestro
protocolo define un **header de tamaño fijo que incluye la longitud del payload**.
El receptor:
1. Lee **exactamente** los bytes del header (tamaño fijo: 6 bytes).
2. Del header saca `LENGTH`.
3. Lee **exactamente** `LENGTH` bytes más → ese es el payload completo.
4. Ya tiene un mensaje completo. Repite.

Esta técnica se llama **length-prefixed framing**.

---

## 4. Formato del Header (6 bytes, tamaño fijo)

Todo mensaje empieza con este header de **6 bytes**:

```
 Offset  Tamaño  Campo      Descripción
 ------  ------  ---------  --------------------------------------------
   0      2      MAGIC      Número mágico fijo: 0x50 0x47 ("PG" de PonG)
   2      1      VERSION    Versión del protocolo (actualmente 1)
   3      1      TYPE       Tipo de mensaje (ver tabla de vocabulario)
   4      2      LENGTH     Longitud del PAYLOAD en bytes (0 a 65535)
 ------  ------  ---------  --------------------------------------------
   6      LENGTH PAYLOAD    Contenido específico del tipo de mensaje
```

Representación visual:

```
 Byte:   0     1     2       3      4     5      6 ...
       +-----+-----+-------+------+-----+-----+------------------+
       | MAGIC (2) | VER   | TYPE | LENGTH (2)| PAYLOAD (LENGTH) |
       |  0x5047   | 0x01  | 0xNN |  0xNNNN   |     ...          |
       +-----+-----+-------+------+-----+-----+------------------+
```

### Detalle de cada campo

| Campo | Bytes | Tipo | Para qué sirve |
|-------|-------|------|----------------|
| MAGIC | 2 | uint16 | Detectar basura/desincronización. Si no es `0x5047`, el mensaje es inválido → se descarta/cierra conexión. |
| VERSION | 1 | uint8 | Permite evolucionar el protocolo sin romper compatibilidad. |
| TYPE | 1 | uint8 | Identifica qué mensaje es (define cómo interpretar el payload). |
| LENGTH | 2 | uint16 | Cuántos bytes de payload siguen. Permite el framing en TCP. |

### Orden de bytes (byte order) — ¡clásica pregunta!

Todos los campos multibyte (MAGIC, LENGTH y cualquier entero del payload) se
transmiten en **network byte order (big-endian)**.

- En C: se usa `htons()` al enviar y `ntohs()` al recibir (para 16 bits),
  `htonl()`/`ntohl()` para 32 bits.
- En Python: se usa el módulo `struct` con el prefijo `!` (network/big-endian),
  por ejemplo `struct.pack("!H", valor)`.

> **¿Por qué?** Distintas CPUs guardan los enteros en distinto orden de bytes
> (little-endian vs big-endian). Al fijar un orden de red común, cliente y
> servidor se entienden aunque corran en arquitecturas distintas.

---

## 5. Vocabulario de mensajes (tipos)

Los tipos se dividen por origen. **C→P** = del cliente al servidor.
**S→C** = del servidor al cliente.

| Valor (TYPE) | Nombre | Dirección | Descripción |
|--------------|--------|-----------|-------------|
| `0x01` | `MSG_REGISTER`      | C→S | Cliente se registra (nickname + email) |
| `0x02` | `MSG_REGISTER_OK`   | S→C | Registro aceptado, entrega un player_id |
| `0x03` | `MSG_REGISTER_ERR`  | S→C | Registro rechazado (con código de error) |
| `0x10` | `MSG_QUEUE`         | C→S | Cliente pide entrar a la cola de emparejamiento |
| `0x11` | `MSG_MATCH_FOUND`   | S→C | Se encontró rival; incluye datos de la partida |
| `0x20` | `MSG_INPUT`         | C→S | Entrada del jugador (mover paleta arriba/abajo/quieto) |
| `0x21` | `MSG_STATE`         | S→C | Estado del juego (pelota, paletas, score) |
| `0x22` | `MSG_GAME_START`    | S→C | La partida comienza |
| `0x23` | `MSG_GAME_OVER`     | S→C | La partida terminó (con ganador y score final) |
| `0x30` | `MSG_PING`          | C→S | Keepalive (verificar que la conexión sigue viva) |
| `0x31` | `MSG_PONG`          | S→C | Respuesta al ping |
| `0x40` | `MSG_ERROR`         | S→C | Error genérico del protocolo |
| `0x41` | `MSG_DISCONNECT`    | C↔S | Aviso de desconexión ordenada |

---

## 6. Formato del PAYLOAD por tipo de mensaje

> Todos los strings van con **1 byte de longitud** seguido de los bytes del texto
> (formato "length-prefixed"), en UTF-8. Máximo 255 bytes por string.

### `MSG_REGISTER` (0x01) — C→S
```
 [1 byte  len_nick][len_nick bytes nickname]
 [1 byte  len_mail][len_mail bytes email]
```

### `MSG_REGISTER_OK` (0x02) — S→C
```
 [4 bytes player_id]   (uint32, network order)
```

### `MSG_REGISTER_ERR` (0x03) — S→C
```
 [1 byte  error_code]   (ver tabla de errores)
```

### `MSG_QUEUE` (0x10) — C→S
```
 (payload vacío, LENGTH = 0)
```

### `MSG_MATCH_FOUND` (0x11) — S→C
```
 [4 bytes match_id]        (uint32)
 [1 byte  side]            (0 = jugador izquierdo, 1 = jugador derecho)
 [1 byte  len_rival][len_rival bytes nickname del rival]
```

### `MSG_INPUT` (0x20) — C→S
```
 [1 byte  direction]   (0 = quieto, 1 = arriba, 2 = abajo)
```

### `MSG_STATE` (0x21) — S→C  ← este viaja muchas veces por segundo
```
 [2 bytes ball_x]      (uint16) posición X de la pelota
 [2 bytes ball_y]      (uint16) posición Y de la pelota
 [2 bytes paddle_left] (uint16) posición Y de la paleta izquierda
 [2 bytes paddle_right](uint16) posición Y de la paleta derecha
 [1 byte  score_left]  (uint8)  puntaje jugador izquierdo
 [1 byte  score_right] (uint8)  puntaje jugador derecho
```

### `MSG_GAME_START` (0x22) — S→C
```
 (payload vacío, LENGTH = 0)
```

### `MSG_GAME_OVER` (0x23) — S→C
```
 [1 byte  winner_side]   (0 = izquierdo, 1 = derecho)
 [1 byte  score_left]
 [1 byte  score_right]
```

### `MSG_PING` (0x30) / `MSG_PONG` (0x31)
```
 (payload vacío)
```

### `MSG_ERROR` (0x40) — S→C
```
 [1 byte  error_code]
```

### `MSG_DISCONNECT` (0x41)
```
 (payload vacío)
```

---

## 7. Códigos de error

| Código | Nombre | Significado |
|--------|--------|-------------|
| `0x01` | `ERR_BAD_MAGIC`      | El header no tiene el número mágico correcto |
| `0x02` | `ERR_BAD_VERSION`    | Versión de protocolo no soportada |
| `0x03` | `ERR_UNKNOWN_TYPE`   | Tipo de mensaje desconocido |
| `0x04` | `ERR_BAD_LENGTH`     | LENGTH inconsistente con el payload |
| `0x05` | `ERR_NICK_TAKEN`     | El nickname ya está en uso |
| `0x06` | `ERR_INVALID_FIELD`  | Un campo del payload es inválido (ej. email vacío) |
| `0x07` | `ERR_NOT_REGISTERED` | El cliente intenta jugar sin registrarse |
| `0x08` | `ERR_SERVER_FULL`    | El servidor no acepta más clientes |

---

## 8. Reglas de procedimiento (máquina de estados del cliente)

El cliente pasa por estos estados. Cada flecha indica qué mensaje causa la transición.

```
   [DESCONECTADO]
        |  (abre socket TCP + connect)
        v
   [CONECTADO]
        |  --> envía MSG_REGISTER
        |  <-- recibe MSG_REGISTER_OK (ok)  ó  MSG_REGISTER_ERR (vuelve a intentar)
        v
   [REGISTRADO]
        |  --> envía MSG_QUEUE
        |  <-- recibe MSG_MATCH_FOUND
        v
   [EMPAREJADO]
        |  <-- recibe MSG_GAME_START
        v
   [JUGANDO]  <----------------------------+
        |  --> envía MSG_INPUT (cada tecla) |
        |  <-- recibe MSG_STATE (continuo) -+  (bucle del juego)
        |  <-- recibe MSG_GAME_OVER
        v
   [FIN DE PARTIDA]
        |  (puede volver a [REGISTRADO] para jugar otra vez, o desconectar)
        v
   [DESCONECTADO]
```

### Reglas clave
1. Un cliente **no puede** enviar `MSG_INPUT` si no está en estado `JUGANDO`.
   Si lo hace, el servidor responde `MSG_ERROR` con `ERR_NOT_REGISTERED` o lo ignora.
2. El servidor es la **única** autoridad sobre el estado. El cliente nunca calcula
   la física; solo dibuja lo que llega en `MSG_STATE`.
3. Ningún mensaje viaja cliente→cliente. **Todo pasa por el servidor.**
4. Si el rival se desconecta, el servidor envía `MSG_GAME_OVER` (o un error) al
   jugador que queda, y registra el evento en el log.

---

## 9. Ejemplo completo (bytes reales)

Registro de un jugador con nickname "juan" y email "j@u.co":

```
Header:
  50 47     -> MAGIC  (0x5047)
  01        -> VERSION (1)
  01        -> TYPE   (MSG_REGISTER)
  00 0E     -> LENGTH (14 bytes de payload)

Payload:
  04                -> len_nick = 4
  6A 75 61 6E       -> "juan"
  06                -> len_mail = 6
  6A 40 75 2E 63 6F -> "j@u.co"
```

Total en el cable: `50 47 01 01 00 0E 04 6A 75 61 6E 06 6A 40 75 2E 63 6F`
(6 bytes de header + 14 de payload = 20 bytes).
