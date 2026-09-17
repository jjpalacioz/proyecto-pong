# Arquitectura del Sistema — Proyecto Pong

Este documento describe la arquitectura general. El diseño detallado del
protocolo está en [`PROTOCOL.md`](PROTOCOL.md).

## Vista de alto nivel

```
   +------------------+                         +------------------+
   |   PongClient A   |                         |   PongClient B   |
   |  (Python/Pygame) |                         |  (Python/Pygame) |
   +--------+---------+                         +---------+--------+
            |                                             |
            |  MyAppGameProtocol (binario, sobre TCP)     |
            |                                             |
            +---------------------+   +-------------------+
                                  |   |
                            +-----v---v-----+
                            |   PongServer  |
                            |      (C)      |
                            |  - estado     |
                            |  - física     |
                            |  - score      |
                            |  - hilos      |
                            |  - logger     |
                            +---------------+
```

## Principios de diseño

1. **El servidor es la única fuente de verdad.** Mantiene el estado del juego
   (posición de la pelota, paletas, puntaje). Los clientes NO calculan física.
2. **Todo pasa por el servidor.** Un cliente nunca habla con otro cliente
   directamente. El servidor recibe input, actualiza estado y notifica a ambos.
3. **El cliente es "tonto":** captura teclas → manda comando → recibe estado →
   dibuja. Nada más.
4. **Concurrencia con hilos:** cada pareja de jugadores (una partida) se maneja
   de forma que varias partidas corran simultáneamente sin bloquearse.

## ¿Por qué TCP y no UDP?

Se eligió **TCP (SOCK_STREAM)** por las siguientes razones:

- **Confiabilidad:** eventos críticos como anotar un punto o el registro del
  jugador NO se pueden perder. TCP garantiza entrega y orden.
- **Simplicidad de razonamiento:** al no tener que reimplementar control de
  pérdidas/reordenamiento (como sí tocaría en UDP), el código es más robusto
  y más fácil de defender y depurar.
- **Trade-off aceptado:** TCP puede introducir algo más de latencia que UDP,
  pero para Pong (pocos objetos, baja frecuencia de eventos) es imperceptible
  y la confiabilidad pesa más.

> Nota: como TCP es un flujo de bytes (stream) sin fronteras de mensaje, nuestro
> protocolo define un **header con longitud** para saber dónde empieza y termina
> cada mensaje. Ver `PROTOCOL.md`.

## Flujo típico de una jugada

1. El jugador presiona una tecla (ej. flecha arriba) → el cliente envía un
   mensaje `INPUT` al servidor.
2. El servidor valida y actualiza el estado (mueve la paleta, simula la pelota).
3. El servidor calcula colisiones, puntaje, etc.
4. El servidor envía un mensaje `STATE` (estado del juego) a **ambos** clientes.
5. Cada cliente dibuja el nuevo estado y el score en pantalla.

Todo evento se **imprime por consola** y se **escribe en el archivo de log**.
