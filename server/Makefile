# ============================================================================
#  Makefile  —  Compilacion del servidor Pong (PongServer)
# ----------------------------------------------------------------------------
#  El profe pide compilar con gcc y entregar un Makefile.
#  Comandos disponibles:
#     make          -> compila el servidor (genera el ejecutable ./server)
#     make clean    -> borra los archivos generados (binarios y objetos)
#     make run      -> compila y ejecuta con valores de ejemplo
# ============================================================================

# --- Variables de configuracion ---

# Compilador a usar (el profe recomienda gcc).
CC = gcc

# Banderas (flags) de compilacion:
#   -Wall -Wextra : activan TODAS las advertencias (nos ayudan a detectar bugs)
#   -g            : incluye info de depuracion (para usar gdb si algo falla)
#   -Iinclude     : le dice al compilador donde buscar los archivos .h (headers)
#   -pthread      : habilita el soporte de hilos (lo usaremos en la Fase 4)
CFLAGS = -Wall -Wextra -g -Iinclude -pthread

# Nombre del ejecutable final.
TARGET = server

# Carpeta donde estan los archivos fuente (.c).
SRC_DIR = src

# Lista de todos los archivos fuente. Al agregar un .c nuevo, se anade aqui.
SOURCES = $(SRC_DIR)/server.c \
          $(SRC_DIR)/logger.c \
          $(SRC_DIR)/net.c \
          $(SRC_DIR)/protocol_io.c \
          $(SRC_DIR)/client_handler.c

# Convierte la lista de .c en la lista de .o (archivos objeto).
# Ej: src/server.c -> src/server.o
OBJECTS = $(SOURCES:.c=.o)

# --- Reglas ---

# Regla por defecto (la que corre al escribir solo "make"): construir el ejecutable.
all: $(TARGET)

# Como construir el ejecutable: enlazar (link) todos los .o en uno solo.
# $@ = el nombre del objetivo (server). $^ = todas las dependencias (los .o).
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "OK: ejecutable './$(TARGET)' generado."

# Como construir cada .o a partir de su .c.
# $< = el primer prerequisito (el archivo .c). $@ = el objetivo (.o).
$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Limpieza: borra objetos y el ejecutable.
clean:
	rm -f $(OBJECTS) $(TARGET)
	@echo "Limpieza completada."

# Atajo para compilar y ejecutar con valores de ejemplo (puerto 5000, log pong.log).
run: $(TARGET)
	./$(TARGET) 5000 pong.log

# Declara objetivos que NO son archivos (para que make no se confunda si
# existiera un archivo llamado "clean" o "all").
.PHONY: all clean run
