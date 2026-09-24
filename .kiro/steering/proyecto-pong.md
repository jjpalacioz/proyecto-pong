# Contexto del Proyecto Pong (Telemática)

Este archivo da contexto a Kiro (la IA) sobre el proyecto. Todos los integrantes
del grupo lo comparten vía GitHub, así la IA trabaja con la misma información sin
que haya que explicarle el proyecto cada vez.

## Qué es el proyecto

Juego **Pong multijugador en red** con arquitectura **cliente/servidor** sobre
**TCP/IP**, usando la **API de Sockets Berkeley** directamente (sin librerías de
sockets de alto nivel). Es el Proyecto N°1 del curso "Internet: Arquitectura y
Protocolos". Fecha límite: **22 de octubre de 2026, 23:59**.

## Equipo y roles

- **Juan José Palacio** — Líder técnico. Dueño del **servidor en C**
  (concurrencia, lógica del juego, despliegue). Trabaja en `server/`.
- **Alejandro Correa** — Dueño del **protocolo**, la **robustez** y los **UML**.
  Trabaja en `docs/` y `common/`.
- **Samuel Granados** — Dueño del **cliente Python + GUI (Pygame)** y el score en
  pantalla. Trabaja en `client/`.

> Regla del curso: los 3 deben poder sustentar CUALQUIER parte del proyecto ante
> el profesor. Por eso, al generar código, SIEMPRE explica qué hace y por qué,
> con detalle suficiente para defenderlo.

## Decisiones técnicas ya tomadas (no cambiar sin acordarlo)

- **Servidor: C** (obligatorio por el enunciado; no se acepta en otro lenguaje).
- **Cliente: Python 3 con Pygame.**
- **Transporte: TCP** (SOCK_STREAM). Justificación: confiable y ordenado; no se
  pueden perder eventos como anotar un punto o el registro.
- **La física del juego la calcula el SERVIDOR** (única fuente de verdad). El
  cliente solo captura teclas y dibuja lo que recibe.
- **Protocolo propio, BINARIO (no texto)**, llamado `MyAppGameProtocol`.
- **Todo pasa por el servidor**: ningún cliente se comunica directo con otro.
- **Concurrencia con hilos** (pthreads), arquitectura thread-per-client.

## El protocolo binario (resumen; detalle en docs/PROTOCOL.md)

- **Header fijo de 6 bytes**: MAGIC (2, = 0x5047), VERSION (1, = 0x01),
  TYPE (1), LENGTH (2). Todo en **network byte order (big-endian)**.
- **Framing length-prefixed**: se lee el header, se saca LENGTH, se leen
  exactamente esos bytes de payload. Esto resuelve que TCP es un stream.
- En **C** usar `htons`/`ntohs` (y `htonl`/`ntohl`); en **Python** usar `struct`
  con el prefijo `!` (big-endian). Cliente y servidor DEBEN coincidir byte a byte.
- La fuente única de verdad de las constantes (tipos de mensaje, códigos de
  error, parámetros del juego) está en `common/protocol_spec.md`.

## Estructura del repositorio

- `server/` — código C del servidor (Juan). `src/`, `include/`, `Makefile`.
- `client/` — cliente Python/Pygame (Samuel).
- `common/` — definiciones compartidas del protocolo (Alejandro).
- `docs/` — arquitectura, protocolo y UML (Alejandro).

## Requisitos del enunciado (checklist a cumplir)

- Servidor en C con sockets Berkeley (sin clases de sockets prehechas).
- Cliente con interfaz gráfica (Python/Pygame).
- Protocolo propio binario, documentado (vocabulario, reglas, UML).
- Servidor concurrente (múltiples parejas simultáneas con hilos).
- Score en tiempo real en la interfaz.
- Registro de perfil (nickname + email).
- Logger: imprime en consola Y en archivo de log.
- Ejecución exacta: `./server <PORT> <LogFile>`.
- Makefile y compilación con `gcc` sin warnings (`-Wall -Wextra`).
- README con: Introducción, Desarrollo, Conclusiones, Referencias.
- Desplegar el servidor en AWS Academy.
- Repositorio en GitHub con commits paso a paso.

## Cómo debe comportarse la IA en este proyecto

1. **Explica siempre** el código que generes, línea por línea si es necesario,
   pensando en que el integrante debe sustentarlo ante un profesor exigente.
2. **Respeta el protocolo** ya definido en `docs/PROTOCOL.md` y
   `common/protocol_spec.md`. No inventes tipos ni formatos nuevos sin avisar.
3. **Maneja los peores casos** (el profe intenta "corchar"): mensajes corruptos,
   MAGIC/version/length inválidos, desconexiones abruptas, tipos inesperados. El
   servidor NUNCA debe crashear; debe responder el código de error adecuado y
   registrarlo en el log.
4. **Compila/prueba** lo que generes cuando sea posible, y no dejes warnings.
5. **Commits**: pequeños, temáticos y con prefijo de fase (ej.
   "Fase 6: física del juego en el servidor"). Cada integrante trabaja sobre todo
   en su carpeta para evitar conflictos.
6. **No cambies** las decisiones técnicas de arriba sin que el grupo lo acuerde.

## Estado de avance (actualizar a medida que avanza)

- Fase 0 — Estructura base: COMPLETADA.
- Fase 1 — Protocolo binario: COMPLETADA.
- Fase 2 — Servidor C básico (socket/bind/listen/accept, logger, Makefile): COMPLETADA.
- Fase 3 — Registro de clientes (framing, net, parsing, validaciones): COMPLETADA.
- Fase 4 — Concurrencia con hilos (thread-per-client, mutex): COMPLETADA.
- Fase 5 — Cliente Python + GUI Pygame: COMPLETADA (protocol.py, network.py,
  ui.py, main.py; registro funcional contra el servidor C; ventana base).
- Fase 6 — Lógica del juego + score en tiempo real: PENDIENTE.
- Fase 7 — Emparejamiento (matchmaking): PENDIENTE.
- Fase 8 — Robustez (peores casos): PENDIENTE.
- Fase 9 — Despliegue AWS + documentación final + UML: PENDIENTE.
