# ============================================================
# Makefile - Proyecto Final de Programacion de Sistemas
# Curso 2026-2027. Grupo 9.
#
# Servidor Concurrente TCP de Calificaciones IMDb.
# A Knight of the Seven Kingdoms.
#
# Automatiza la compilacion independiente de los modulos,
# la construccion de la biblioteca estatica, el enlazado
# de los ejecutables, las pruebas y la limpieza del proyecto.
# ============================================================

# ------------------------------------------------------------
# Herramientas de compilacion
# ------------------------------------------------------------

CC = gcc
AR = ar
ARFLAGS = rcs

# ------------------------------------------------------------
# Opciones de compilacion y enlazado
# ------------------------------------------------------------

CPPFLAGS = -I.
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror -g
LDFLAGS =
LDLIBS =

# ------------------------------------------------------------
# Archivos generados
# ------------------------------------------------------------

LIB = libmonitor.a
LIB_OBJS = monitor.o utils.o

SERVER = server
CLIENT = client

TEST_MONITOR = tests/test_monitor
TEST_UTILS = tests/test_utils

# Establecer all como objetivo predeterminado.
.DEFAULT_GOAL := all

# Objetivos que no representan archivos fisicos.
.PHONY: all clean test smoke-test rebuild

# ------------------------------------------------------------
# Compilacion general del proyecto
# ------------------------------------------------------------

all: $(LIB) $(SERVER) $(CLIENT)

# ------------------------------------------------------------
# Construccion de la biblioteca estatica
# ------------------------------------------------------------

$(LIB): $(LIB_OBJS)
	$(AR) $(ARFLAGS) $@ $^

# ------------------------------------------------------------
# Compilacion independiente de los modulos
# ------------------------------------------------------------

monitor.o: monitor.c monitor.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

utils.o: utils.c utils.h monitor.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

server.o: server.c utils.h monitor.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

client.o: client.c utils.h monitor.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# ------------------------------------------------------------
# Enlazado de los ejecutables principales
# ------------------------------------------------------------

$(SERVER): server.o $(LIB)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(CLIENT): client.o $(LIB)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

# ------------------------------------------------------------
# Compilacion de las pruebas unitarias
# ------------------------------------------------------------

$(TEST_MONITOR): tests/test_monitor.c monitor.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< $(LIB) $(LDLIBS) -o $@

$(TEST_UTILS): tests/test_utils.c utils.h monitor.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< $(LIB) $(LDLIBS) -o $@

# ------------------------------------------------------------
# Ejecucion de las pruebas automatizadas
# ------------------------------------------------------------

test: all $(TEST_MONITOR) $(TEST_UTILS)
	./$(TEST_MONITOR)
	./$(TEST_UTILS)
	./$(SERVER) --test-run

# Compila el servidor si es necesario y ejecuta el Smoke Test.
smoke-test: $(SERVER)
	./$(SERVER) --test-run

# ------------------------------------------------------------
# Limpieza de archivos generados
# ------------------------------------------------------------

clean:
	rm -f *.o $(LIB) $(SERVER) $(CLIENT)
	rm -f $(TEST_MONITOR) $(TEST_UTILS)

# ------------------------------------------------------------
# Reconstruccion completa del proyecto
# ------------------------------------------------------------

rebuild:
	$(MAKE) clean
	$(MAKE) all