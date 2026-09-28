# Casos límite — MyAppGameProtocol

**Fase 8 · Robustez**
**Autor del diseño:** Alejandro Correa

Este documento es el contrato de los peores casos: lo que el profesor puede
mandar al socket para intentar tumbar el servidor. Para cada caso dice qué
debe hacer el servidor y, si responde, con qué código.

El servidor **nunca** debe cerrarse solo, quedarse en un bucle infinito ni
mezclar el estado de un cliente con el de otro. Un cliente malicioso solo
puede afectar su propia conexión.

La lista se prueba con [`tests/stress_protocol.py`](../tests/stress_protocol.py).
Los casos **C** ya se cumplen con el servidor actual. Los casos **P** son el
contrato que todavía hay que implementar en el servidor (Juan, sobre este
diseño).

---

## 1. Cómo está armado un mensaje

```
 0        2      3      4        6
 +--------+------+------+--------+------------------+
 | MAGIC  | VER  | TYPE | LENGTH | PAYLOAD (LENGTH) |
 | 0x5047 | 0x01 | 0xNN | uint16 | ...              |
 +--------+------+------+--------+------------------+
```

MAGIC y LENGTH van en **big-endian** (network byte order). TCP no separa
mensajes: el servidor lee 6 bytes, saca `LENGTH` y lee exactamente esos bytes.
Eso es el framing length-prefixed.

## 2. Orden de validación

Cuando llega un mensaje, el servidor lo revisa en este orden. La primera
regla que falle es la única respuesta. Así se puede explicar por qué, si el
número mágico y la versión vienen mal a la vez, se responde el mágico.

1. **MAGIC** distinto de `0x5047` → `MSG_ERROR` + `ERR_BAD_MAGIC` y se cierra la conexión. No se intenta re-sincronizar: a partir de ahí el flujo ya no es confiable.
2. **VERSION** distinta de `0x01` → `MSG_ERROR` + `ERR_BAD_VERSION` y se cierra.
3. **TYPE** que no está en la tabla del protocolo → `MSG_ERROR` + `ERR_UNKNOWN_TYPE`.
4. **LENGTH** que no coincide con lo que ese tipo exige → `MSG_ERROR` + `ERR_BAD_LENGTH`. No se lee un payload enorme “por si acaso” antes de rechazar un tipo de tamaño fijo.
5. **TYPE** conocido, pero ilegal en el estado actual del cliente → `MSG_ERROR` + `ERR_NOT_REGISTERED`.
6. **Campos** vacíos, demasiado largos o con la longitud interna mentirosa → `MSG_ERROR` + `ERR_INVALID_FIELD`.

`MSG_ERROR` (`0x40`) lleva un payload de **1 byte**: el código de la tabla de
[`docs/PROTOCOL.md`](PROTOCOL.md).

Si el cliente cierra el socket antes de completar el header o el payload, no
hay a quién responder. El hilo termina, lo deja en el log y el servidor sigue
aceptando a los demás.

### Tamaño exigido por tipo (cliente → servidor)

| TYPE | Nombre | LENGTH exigido |
|------|--------|----------------|
| `0x01` | `MSG_REGISTER` | Variable. Tras el nick y el email no puede sobrar ni faltar un byte. |
| `0x10` | `MSG_QUEUE` | 0 |
| `0x20` | `MSG_INPUT` | 1, y el byte solo puede ser 0, 1 o 2 |
| `0x30` | `MSG_PING` | 0 |
| `0x41` | `MSG_DISCONNECT` | 0 |

Límites de los textos del registro (los aplica el servidor):

| Campo | Mínimo | Máximo |
|-------|--------|--------|
| Nickname | 1 byte | 32 bytes |
| Email | 1 byte | 64 bytes |

Un cliente registrado puede reintentar el registro en la **misma** conexión
si el intento anterior fue `ERR_INVALID_FIELD`. Un MAGIC o una VERSION malos
sí cierran el socket: el cliente tiene que abrir otro.

---

## 3. Casos que el servidor ya cumple

Estos los ejecuta `tests/stress_protocol.py` y tienen que pasar.

| ID | Qué se manda | Qué debe pasar |
|----|----------------|----------------|
| C01 | MAGIC `0x0000`, resto del header válido, `LENGTH = 0` | `ERR_BAD_MAGIC` y cierra |
| C02 | MAGIC bueno, VERSION `0x99` | `ERR_BAD_VERSION` y cierra |
| C03 | MAGIC y VERSION malos a la vez | `ERR_BAD_MAGIC` (el mágico se mira primero) |
| C04 | 6 bytes de basura (`BASURA`) | `ERR_BAD_MAGIC` |
| C05 | Un `MSG_REGISTER` válido partido en dos envíos | `MSG_REGISTER_OK`. `recv_all` junta las lecturas parciales |
| C06 | Solo 3 bytes de header y el cliente cierra | Sin crash. No hay `MSG_ERROR` (el peer ya no está) |
| C07 | `MSG_REGISTER` válido y `MSG_QUEUE` pegados en un solo `send` | El primer mensaje se interpreta solo. La respuesta es `MSG_REGISTER_OK` de 4 bytes, no una mezcla de los dos |
| C08 | Header con `LENGTH = 200` y solo 10 bytes, luego cierre | Sin crash. El hilo ve el cierre a mitad del payload |
| C09 | Registro válido (`nick` + email) | `MSG_REGISTER_OK` con `player_id` de 4 bytes |
| C10 | `MSG_REGISTER` con `LENGTH = 0` | `ERR_INVALID_FIELD` y la conexión sigue abierta |
| C11 | Nickname de longitud 0 | `ERR_INVALID_FIELD` |
| C12 | Email de longitud 0 | `ERR_INVALID_FIELD` |
| C13 | Nickname de exactamente 32 bytes | `MSG_REGISTER_OK` |
| C14 | Nickname de 33 bytes | `ERR_INVALID_FIELD` |
| C15 | Email de exactamente 64 bytes | `MSG_REGISTER_OK` |
| C16 | Email de 65 bytes | `ERR_INVALID_FIELD` |
| C17 | El byte de longitud del nick dice 5, pero solo viene 1 byte | `ERR_INVALID_FIELD` |
| C18 | Un registro inválido y, en el mismo socket, uno válido | Primero el error, después `MSG_REGISTER_OK` |
| C19 | `MSG_QUEUE` antes de registrarse | `ERR_NOT_REGISTERED` |
| C20 | `MSG_INPUT` antes de registrarse | `ERR_NOT_REGISTERED` |
| C21 | Conectar y cerrar sin mandar nada | Sin crash |
| C22 | Registrarse, pedir partida y cerrar el socket a mitad | Sin crash. Hoy es partida de práctica (un solo jugador) |
| C23 | Varios clientes con MAGIC malo a la vez, y luego uno válido | El válido recibe `MSG_REGISTER_OK`. Un hilo por cliente |
| C24 | En partida, `MSG_INPUT` con dirección `9` (no es 0, 1 ni 2) | Sin crash. La paleta no se mueve y siguen llegando `MSG_STATE` |
| C25 | Cinco registros en paralelo | Cinco `player_id` distintos (el mutex del contador) |
| C26 | Un cliente se queda callado anunciando `LENGTH = 500` y no manda el payload | Ese hilo espera, pero **otro** cliente sí se registra. Al cerrar el primero, su hilo termina |

---

## 4. Contrato pendiente en el servidor

El script también los manda. Si el servidor todavía no responde lo de esta
tabla, el caso sale como **pendiente**: no es un fallo del diseño, es trabajo
de implementación. Cuando estén hechos, el mismo script los da por buenos.

| ID | Qué se manda | Respuesta exigida | Hoy |
|----|----------------|-------------------|-----|
| P01 | TYPE `0xAB` (no existe), `LENGTH = 0` | `ERR_UNKNOWN_TYPE` | Responde `ERR_NOT_REGISTERED`, porque cualquier cosa que no sea registro cae en el mismo `else` |
| P02 | Dos clientes con el mismo nickname | El segundo: `ERR_NICK_TAKEN` | Los dos quedan registrados |
| P03 | `MSG_PING` con `LENGTH = 0` | `MSG_PONG` (`0x31`), payload vacío, sin cerrar la conexión | No hay keepalive. Antes de registrarse responde `ERR_NOT_REGISTERED` |
| P04 | Registro válido con un byte de sobra al final | `ERR_BAD_LENGTH` | El byte sobrante se ignora y el registro se acepta |
| P05 | Ya registrado, manda `MSG_INPUT` en vez de `MSG_QUEUE` | `MSG_ERROR` (el protocolo permite `ERR_NOT_REGISTERED` fuera de `JUGANDO`) y queda escrito en el log | Solo se escribe en el log y se cierra el socket, sin avisar al cliente |
| P06 | `MSG_QUEUE` con `LENGTH = 3` en vez de 0 | `ERR_BAD_LENGTH` (la forma se valida antes que el estado) | `ERR_NOT_REGISTERED` |
| P07 | 64 clientes registrados a la vez, y un 65.º | El 65.º recibe `ERR_SERVER_FULL` y no entra | No hay cupo. Entran todos. El máximo que hay que implementar es **64** conexiones registradas simultáneas |
| P08 | En una partida 1 contra 1, un jugador cierra el socket | El que se queda recibe `MSG_GAME_OVER`, el suceso queda en el log, la partida se libera y el servidor sigue. Depende del emparejamiento (Fase 7); hoy solo existe la partida de práctica de un jugador | No hay rival a quien avisar |

Reglas que acompañan a esa tabla:

- **Timeout de lectura.** Un cliente que anuncia un `LENGTH` grande y no manda
  los bytes no puede ocupar un hilo para siempre. Hay que poner un timeout en
  el socket (por ejemplo 15 s con `SO_RCVTIMEO`). Al vencer, se cierra esa
  conexión, se registra en el log y no se toca al resto.
- **Ping.** `MSG_PING` es legal en cualquier estado después del `connect`. La
  respuesta es `MSG_PONG`. Si pasan más de dos intervalos sin ping ni otro
  mensaje de un cliente ya en partida, se trata como desconexión (mismo camino
  que P08).
- **Dirección de input.** Un `MSG_INPUT` de 1 byte cuyo valor no sea 0, 1 o 2
  no mueve la paleta y no tumba el servidor (C24 ya lo cubre en la práctica).
  No hace falta un código de error nuevo: se ignora el movimiento.
- **Partida de práctica.** Hasta que exista el emparejamiento, cerrar el único
  jugador (C22) termina ese hilo. P08 reemplaza ese comportamiento cuando haya
  dos jugadores de verdad.

---

## 5. Lo que el servidor no debe hacer nunca

- Morir por `SIGSEGV`, `SIGPIPE` o una excepción no atrapada. `send` ya usa
  `MSG_NOSIGNAL` para que un cliente que se va no mate el proceso.
- Aceptar un payload cuyo `LENGTH` no leyó completo y tratarlo como mensaje válido.
- Asignar el mismo `player_id` a dos hilos. El contador va bajo mutex.
- Dejar que el log de dos hilos se mezcle en la misma línea. El logger también
  lleva mutex.
- Procesar el mensaje de un cliente como si fuera de otro. Cada hilo tiene su
  propio socket y su propio `Player`.

---

## 6. Cómo correr las pruebas

Desde la raíz del repositorio, en Linux o en WSL (el servidor es C con
sockets Berkeley y no compila en Windows nativo):

```bash
make -C server
python3 tests/stress_protocol.py
```

El script arranca `./server` en un puerto libre, manda cada caso y al final
lo apaga con SIGINT. Salida:

- `[ok]` el caso se comportó como dice este documento.
- `[pendiente]` el caso es de la sección 4 y el servidor todavía no lo cumple.
- `[FALLO]` el servidor crasheó, o un caso de la sección 3 no dio la respuesta exigida.

El proceso termina con código 0 si no hay ningún `[FALLO]`. Los pendientes no
bajan la ejecución: avisan lo que falta por programar en `server/`.

---

## 7. Para sustentar

**¿Qué pasa si te mando basura al socket?**
El servidor lee 6 bytes y compara el MAGIC con `0x5047`. Si no coincide,
responde `ERR_BAD_MAGIC`, cierra esa conexión y sigue aceptando clientes.

**¿Cómo sabes dónde termina un mensaje si TCP es un flujo?**
Por el campo `LENGTH` del header. Se leen 6 bytes, luego exactamente `LENGTH`
bytes. Si dos mensajes llegan pegados, el primero no se come al segundo.

**¿Qué pasa si `LENGTH` miente y el cliente cierra antes?**
`recv_all` recibe 0 a mitad de la lectura, el hilo termina y el proceso no
se cae. No se interpreta un payload a medias.

**¿Qué pasa si el cliente se queda callado a mitad del payload?**
Hoy ese hilo espera. El contrato (sección 4) exige un timeout para soltarlo.
Los demás clientes siguen, porque cada uno tiene su hilo. Eso es C26.

**¿Por qué un MAGIC malo cierra, pero un nickname vacío no?**
El nickname vacío es un campo mal formado de un mensaje que sí es nuestro
protocolo: se responde el error y se deja reintentar. Un MAGIC malo significa
que el flujo ya no está alineado; seguir leyendo mezclaría bytes de mensajes
distintos.
