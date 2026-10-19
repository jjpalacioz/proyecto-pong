# 🏓 Proyecto Pong — Juego en Red (Cliente/Servidor)

**Curso:** Internet: Arquitectura y Protocolos (Telemática)
**Proyecto N°1 — Pong**
**Fecha de entrega:** 22 de octubre de 2026

## 👥 Integrantes

| Nombre | Rol principal |
|--------|---------------|
| Juan José Palacio | Líder técnico · Servidor C (núcleo, concurrencia, lógica del juego) |
| Alejandro Correa | Protocolo binario · Serialización · Robustez · UML |
| Samuel Granados | Cliente Python/Pygame · GUI · Score en tiempo real · Logger |

---

## 1. Introducción

Este proyecto implementa el clásico juego **Pong** en una arquitectura
**cliente/servidor** sobre la pila de protocolos **TCP/IP**, usando la
**API de Sockets Berkeley** directamente (sin librerías de sockets de alto nivel).

- El **servidor** está escrito en **C** y es el responsable de mantener el
  *estado* del juego: física de la pelota, colisiones, puntaje y validación
  de todos los movimientos. Soporta **múltiples parejas de jugadores de forma
  concurrente** mediante hilos.
- El **cliente** está escrito en **Python** con **Pygame**. Solo se encarga de
  capturar la entrada del usuario (teclado), enviarla al servidor y **dibujar**
  el estado que el servidor le envía (incluyendo el **score en tiempo real**).
- La comunicación se realiza mediante un **protocolo propio de aplicación,
  codificado de forma binaria** (no texto), llamado `MyAppGameProtocol`.

> **Regla clave de la arquitectura:** ningún cliente se comunica directamente
> con otro cliente. **Todo pasa por el servidor.**

## 2. Desarrollo

### 2.1 Arquitectura general

Ver [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md). En resumen: el servidor en C
mantiene el estado y simula la física (~60 ticks/seg) y los clientes Python solo
capturan teclas (`MSG_INPUT`) y dibujan el estado que reciben (`MSG_STATE`,
que incluye el score en tiempo real). Todo pasa por el servidor.

### 2.2 Diseño del protocolo binario (`MyAppGameProtocol`)

El protocolo de aplicación propio, **codificado en binario**, está completamente
especificado en [`docs/PROTOCOL.md`](docs/PROTOCOL.md). Incluye:

- **Header de tamaño fijo (6 bytes)** con `MAGIC`, `VERSION`, `TYPE` y `LENGTH`,
  que resuelve el *framing* sobre el flujo de bytes de TCP.
- **Vocabulario de 13 tipos de mensaje** (registro, emparejamiento, input, estado,
  eventos de juego, ping/pong y errores).
- **Formato byte a byte del payload** de cada mensaje.
- **Máquina de estados** del cliente (reglas de procedimiento).
- **Tabla de códigos de error** para casos anómalos.

Los valores numéricos (constantes) están centralizados en
[`common/protocol_spec.md`](common/protocol_spec.md) como fuente única de verdad
para el servidor (C) y el cliente (Python).

### 2.3 Servidor (C)

El servidor está escrito en C usando la **API de Sockets Berkeley** directamente
(sin librerías de sockets de alto nivel). Estructura:

```
server/
├── include/            # headers (.h)
│   ├── protocol.h      # constantes del protocolo (magic, tipos, errores, params)
│   ├── logger.h        # API del sistema de logging
│   ├── net.h           # recv_all / send_all (E/S robusta sobre TCP)
│   ├── protocol_io.h   # recv_message / send_message (framing)
│   ├── player.h        # perfil del jugador (id, nickname, email)
│   ├── client_handler.h# atención de cada cliente en su hilo
│   ├── game.h          # estado y física del juego
│   ├── match.h         # emparejamiento 1vs1 y partida compartida
│   └── registry.h      # registro global de clientes (nicks, cupo)
├── src/                # implementación (.c)
│   ├── server.c        # programa principal: socket → bind → listen → accept
│   ├── logger.c        # logger (consola + archivo, thread-safe)
│   ├── net.c           # E/S robusta
│   ├── protocol_io.c   # framing del protocolo
│   ├── client_handler.c# hilo por cliente: registro, validación, flujo
│   ├── game.c          # física: pelota, rebotes, colisiones, score
│   ├── match.c         # matchmaking y simulación de la partida
│   └── registry.c      # nicks en uso y límite de clientes (thread-safe)
└── Makefile            # compilación con gcc
```

**Compilar y ejecutar:**
```bash
cd server
make
./server <PORT> <LogFile>     # ej: ./server 5000 pong.log
```

**Flujo del servidor:** valida los argumentos → inicializa el logger →
crea el socket TCP (`socket`) → lo asocia al puerto (`bind`) → escucha
(`listen`) → acepta un cliente (`accept`) → **procesa el registro del cliente
según el protocolo** → cierra ordenadamente. Cada evento se registra en consola
y en el archivo de log.

**Módulos del servidor:**
- `net.c/.h` — `recv_all`/`send_all`: leen/escriben exactamente N bytes sobre
  TCP (resuelven las lecturas/escrituras parciales del stream).
- `protocol_io.c/.h` — `recv_message`/`send_message`: implementan el *framing*
  (header de 6 bytes + payload), validando `MAGIC`, `VERSION` y `LENGTH`.
- `player.h` — estructura del perfil del jugador (id, nickname, email).

**Registro (Fase 3):** el servidor lee un `MSG_REGISTER`, valida y parsea el
nickname y el email del payload binario, asigna un `player_id` y responde
`MSG_REGISTER_OK`. Ante cualquier anomalía (MAGIC/versión/longitud inválidos,
campos vacíos, tipo inesperado o desconexión abrupta) responde con `MSG_ERROR`
y el código adecuado, **sin caerse**.

> El **logger** cumple el requisito del enunciado: imprime por la terminal todas
> las peticiones/respuestas y las escribe también en el `<LogFile>`, con
> timestamp y nivel de severidad. Su escritura está protegida con un *mutex*
> para soportar la concurrencia que se añade en la Fase 4.

### 2.4 Cliente (Python + Pygame)

El cliente está escrito en **Python** con **Pygame**. Estructura en `client/src/`:

- `protocol.py` — constantes del protocolo (coinciden byte a byte con el servidor C).
- `network.py` — conexión TCP y framing binario con `struct` (formato `!`,
  big-endian, equivalente a `htons`/`ntohs` del servidor).
- `ui.py` — funciones de dibujo (campo, paletas, pelota, score, registro).
- `main.py` — programa principal con la máquina de estados del cliente.

**Separación de capas:** `network.py`/`protocol.py` no dependen de Pygame (solo
red), lo que permite probar la comunicación sin interfaz gráfica.

**Ejecutar:**
```bash
cd client
pip install -r requirements.txt
python src/main.py <IP_SERVIDOR> <PUERTO>   # ej: python src/main.py 127.0.0.1 5000
```

**Registro (Fase 5):** el cliente abre una ventana, el usuario escribe nickname
y email (TAB para cambiar de campo, ENTER para registrarse), y el cliente envía
`MSG_REGISTER` y procesa la respuesta del servidor. El movimiento y el estado del
juego en tiempo real se implementan en la Fase 6.

> El cliente **no calcula física**: solo captura las teclas y dibuja el estado
> que le envía el servidor (que es la única fuente de verdad).

### 2.5 Concurrencia

El servidor usa una arquitectura **thread-per-client** (un hilo por cliente)
con la librería `pthreads`:

1. El hilo principal ejecuta un **bucle infinito** de `accept()`.
2. Por cada cliente que llega, crea un **hilo** (`pthread_create`) que lo
   atiende de forma independiente (`client_thread` en `client_handler.c`).
3. El hilo se marca como *detached* (`pthread_detach`): libera sus recursos al
   terminar, así el hilo principal vuelve enseguida a aceptar más clientes.

De este modo, **múltiples parejas de jugadores pueden jugar simultáneamente**,
cada una atendida por sus propios hilos.

**Seguridad entre hilos (thread-safety):** los datos compartidos se protegen
con *mutex* para evitar condiciones de carrera:
- El contador de `player_id` (`assign_player_id`) — dos hilos no pueden asignar
  el mismo id.
- La escritura del *logger* — las líneas de log de distintos hilos no se mezclan.

> **Probado:** 5 clientes conectados de forma simultánea reciben 5 IDs únicos,
> confirmando que el mutex previene la condición de carrera.

### 2.6 Manejo de errores y casos límite

El contrato de robustez está en [`docs/EDGE_CASES.md`](docs/EDGE_CASES.md):
qué hace el servidor ante un número mágico malo, una versión incorrecta, un
`LENGTH` que no coincide, un registro inválido, un cliente que se cae y varios
clientes atacando a la vez. El orden de validación (mágico, versión, tipo,
longitud, estado, campos) está fijado ahí para que cliente y servidor
respondan siempre el mismo código.

Las pruebas viven en `tests/stress_protocol.py`. Arrancan el servidor, mandan
cada caso y comprueban que el proceso no se cae:

```bash
make -C server
python3 tests/stress_protocol.py
```

El servidor implementa todo el contrato de robustez: número mágico/versión/
longitud inválidos, registro mal formado, **tipo desconocido** (`ERR_UNKNOWN_TYPE`),
**nickname repetido** (`ERR_NICK_TAKEN`), **keepalive** `MSG_PING`/`MSG_PONG`,
**cupo máximo** de clientes (`ERR_SERVER_FULL`, 64), **timeout de lectura**
(`SO_RCVTIMEO`) para que un cliente callado no cuelgue su hilo, y el **aviso al
rival** (`MSG_GAME_OVER`) cuando un jugador se desconecta en una partida 1vs1.
El servidor nunca se cae: `send` usa `MSG_NOSIGNAL` para que un socket cerrado
no mate el proceso.

### 2.7 Emparejamiento (matchmaking) 1vs1

Implementado en `server/src/match.c`. Cuando un jugador envía `MSG_QUEUE`, queda
en una **cola de espera** (protegida con mutex). Al llegar un segundo jugador, el
servidor forma una **partida compartida**: ambos reciben `MSG_MATCH_FOUND` (con el
lado asignado y el nick del rival) y luego `MSG_GAME_START`. Los dos jugadores
comparten un mismo `GameState`; el hilo del jugador izquierdo (host) simula la
física y difunde `MSG_STATE` a ambos, mientras el otro solo aplica su input. La
memoria de la partida se libera con un **conteo de referencias** para evitar que
un hilo use datos que el otro ya liberó. Varias parejas juegan simultáneamente.

### 2.8 Despliegue en AWS Academy

La guía paso a paso está en [`docs/DESPLIEGUE_AWS.md`](docs/DESPLIEGUE_AWS.md):
crear la instancia EC2, abrir el puerto del servidor en el *security group*,
compilar con `gcc` en la nube y ejecutar `./server <PORT> <LogFile>`. Los
clientes se conectan a la **IP pública** de la instancia usando el mismo
protocolo binario que en local.

### 2.9 Documentación UML

- **Diagrama de secuencia** del protocolo: [`docs/SECUENCIA.md`](docs/SECUENCIA.md)
  (orden de los mensajes: registro → emparejamiento → juego → fin).
- **Diagrama de estados** del cliente: [`docs/ESTADOS.md`](docs/ESTADOS.md)
  (los estados de la aplicación y qué mensaje provoca cada transición).

## 3. Conclusiones

- Se logró implementar un juego Pong multijugador completo sobre una
  arquitectura **cliente/servidor** con **TCP/IP** y la **API de Sockets
  Berkeley**, cumpliendo que el servidor sea la única fuente de verdad y que
  **todo pase por el servidor**.
- El diseño de un **protocolo binario propio** con header de tamaño fijo y
  *framing length-prefixed* resolvió el reto de delimitar mensajes sobre el
  flujo continuo de TCP, y permitió que un servidor en C y un cliente en Python
  se entiendan byte a byte.
- El uso de **hilos (pthreads)** con datos compartidos protegidos por **mutex**
  permitió atender múltiples parejas de forma concurrente sin condiciones de
  carrera.
- El trabajo de **robustez** (validación estricta, timeouts, keepalive, manejo
  de desconexiones) hace que el servidor resista entradas malformadas y clientes
  que se caen sin interrumpir el servicio a los demás.
- El proyecto reforzó conceptos clave de redes: orden de bytes de red, la
  diferencia entre un *stream* (TCP) y un protocolo de mensajes, y la separación
  de responsabilidades entre presentación (cliente) y lógica (servidor).

### Cumplimiento de requisitos del enunciado

| Requisito | Estado |
|-----------|--------|
| Servidor en C con API de Sockets Berkeley (sin librerías de alto nivel) | ✅ |
| Cliente con interfaz gráfica (Python + Pygame) | ✅ |
| Protocolo de aplicación propio, **binario** (no texto) | ✅ |
| Todo pasa por el servidor (sin comunicación cliente↔cliente) | ✅ |
| Servidor concurrente (múltiples parejas con hilos) | ✅ |
| Score en tiempo real en la interfaz | ✅ |
| Registro de perfil (nickname + email) | ✅ |
| Logger (consola + archivo de log) | ✅ |
| Ejecución `./server <PORT> <LogFile>` | ✅ |
| Makefile y compilación con `gcc` (sin warnings) | ✅ |
| README con Introducción, Desarrollo, Conclusiones, Referencias | ✅ |
| Documentación del protocolo con UML | ✅ |
| Repositorio en GitHub con commits incrementales e historias de usuario | ✅ |
| Despliegue en AWS Academy | 📄 guía lista en `docs/DESPLIEGUE_AWS.md` |

## 4. Referencias

- Beej's Guide to Network Programming — https://beej.us/guide/bgnet/
- Beej's Guide to C — https://beej.us/guide/bgc/
- TCP Server-Client en C (GeeksforGeeks) — https://www.geeksforgeeks.org/tcp-server-client-implementation-in-c/
- Pong (Wikipedia) — https://en.wikipedia.org/wiki/Pong

---

## 🚀 Cómo ejecutar (referencia rápida)

### Servidor (C)
```bash
cd server
make
./server <PORT> <LogFile>
# Ejemplo:
./server 5000 pong.log
```

### Cliente (Python)
```bash
cd client
pip install -r requirements.txt
python src/main.py <IP_SERVIDOR> <PORT>
# Ejemplo:
python src/main.py 127.0.0.1 5000
```
