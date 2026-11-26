/**
 * @file test_utils.c
 * @brief Pruebas unitarias del modulo de utilidades.
 *
 * Comprueba las operaciones de lectura y escritura sobre
 * tuberias, la comunicacion mediante sockets, la recepcion
 * de codigos, el formato de respuestas y el manejo de errores.
 *
 * Proyecto Final de Programacion de Sistemas.
 * Curso 2026-2027. Grupo 9.
 */

#include "utils.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/**
 * @brief Comprueba la transferencia de datos mediante tuberias.
 */
static void probar_tuberia(void)
{
    int p[2];
    char codigo[LONGITUD_CODIGO + 1];

    assert(pipe(p) == 0);

    /* Escribir el codigo en dos operaciones independientes. */
    assert(escribir_completo(p[1], "S01", 3) == 0);
    assert(escribir_completo(p[1], "E05", 3) == 0);

    /* Cerrar el extremo de escritura. */
    assert(close(p[1]) == 0);

    /* Recuperar todos los bytes enviados. */
    assert(leer_completo(
        p[0],
        codigo,
        LONGITUD_CODIGO
    ) == 0);

    codigo[LONGITUD_CODIGO] = '\0';

    assert(strcmp(codigo, "S01E05") == 0);

    /* Comprobar la deteccion del fin del flujo. */
    assert(leer_completo(p[0], codigo, 1) == 1);

    assert(close(p[0]) == 0);

    puts("OK: lectura y escritura mediante tuberias");
}

/**
 * @brief Comprueba el envio y la recepcion mediante sockets.
 *
 * socketpair crea dos sockets conectados localmente,
 * permitiendo verificar las funciones sin iniciar un servidor.
 */
static void probar_sockets(void)
{
    int s[2];
    char codigo[LONGITUD_CODIGO + 1];

    assert(socketpair(
        AF_UNIX,
        SOCK_STREAM,
        0,
        s
    ) == 0);

    /* Enviar un codigo completo mediante dos operaciones. */
    assert(enviar_completo(s[0], "S01", 3) == 0);
    assert(enviar_completo(s[0], "E05", 3) == 0);

    assert(recibir_codigo(s[1], codigo) == 0);
    assert(strcmp(codigo, "S01E05") == 0);

    /*
     * Enviar un codigo incompleto y finalizar el flujo
     * de escritura para simular una solicitud interrumpida.
     */
    assert(enviar_completo(s[0], "S01", 3) == 0);
    assert(shutdown(s[0], SHUT_WR) == 0);

    assert(recibir_codigo(s[1], codigo) == 1);
    assert(codigo[0] == '\0');

    assert(close(s[1]) == 0);
    assert(close(s[0]) == 0);

    puts("OK: recepcion de codigos completos e incompletos");
}

/**
 * @brief Verifica el comportamiento ante una desconexion.
 *
 * Comprueba que el proceso no termine por SIGPIPE cuando
 * se intenta enviar informacion a un socket cerrado.
 */
static void probar_desconexion(void)
{
    int s[2];

    assert(socketpair(
        AF_UNIX,
        SOCK_STREAM,
        0,
        s
    ) == 0);

    assert(close(s[1]) == 0);

    assert(enviar_completo(s[0], "x", 1) == -1);
    assert(errno == EPIPE);

    assert(close(s[0]) == 0);

    puts("OK: gestion de desconexiones y SIGPIPE");
}

/**
 * @brief Verifica la construccion de respuestas textuales.
 */
static void probar_formato(void)
{
    const Episodio episodio = {
        "S01E05",
        "In the Name of the Mother",
        9.7,
        68300
    };

    char respuesta[CAPACIDAD_RESPUESTA];

    int n = formatear_episodio(
        &episodio,
        respuesta,
        sizeof respuesta
    );

    /* Comprobar la longitud y el contenido de la respuesta. */
    assert(n > 0);
    assert((size_t)n == strlen(respuesta));

    assert(strstr(respuesta, episodio.codigo) != NULL);
    assert(strstr(respuesta, episodio.titulo) != NULL);
    assert(strstr(respuesta, "9.7/10") != NULL);
    assert(strstr(respuesta, "68300") != NULL);

    /* Comprobar la deteccion de un buffer insuficiente. */
    assert(formatear_episodio(
        &episodio,
        respuesta,
        8
    ) == -1);

    assert(errno == ENOSPC);
    assert(respuesta[0] == '\0');

    /* Comprobar el tratamiento de un episodio nulo. */
    assert(formatear_episodio(
        NULL,
        respuesta,
        sizeof respuesta
    ) == -1);

    assert(errno == EINVAL);

    puts("OK: formato de respuestas y control de capacidad");
}

/**
 * @brief Comprueba el tratamiento de argumentos invalidos.
 *
 * Verifica el rechazo de descriptores negativos, buffers
 * nulos y el comportamiento de las operaciones de cero bytes.
 */
static void probar_errores(void)
{
    char codigo[LONGITUD_CODIGO + 1];

    /* Comprobar el rechazo de descriptores negativos. */
    assert(leer_completo(-1, codigo, 1) == -1);
    assert(errno == EBADF);

    assert(escribir_completo(-1, "x", 1) == -1);
    assert(errno == EBADF);

    assert(enviar_completo(-1, "x", 1) == -1);
    assert(errno == EBADF);

    /* Comprobar el rechazo de buffers nulos. */
    assert(leer_completo(0, NULL, 1) == -1);
    assert(errno == EINVAL);

    assert(escribir_completo(1, NULL, 1) == -1);
    assert(errno == EINVAL);

    assert(enviar_completo(1, NULL, 1) == -1);
    assert(errno == EINVAL);

    /* Las operaciones de cero bytes no requieren realizar E/S. */
    assert(leer_completo(-1, NULL, 0) == 0);
    assert(escribir_completo(-1, NULL, 0) == 0);
    assert(enviar_completo(-1, NULL, 0) == 0);

    puts("OK: tratamiento de errores de entrada/salida");
}

/**
 * @brief Ejecuta todas las pruebas del modulo utils.
 *
 * @return 0 si todas las pruebas finalizan correctamente.
 */
int main(void)
{
    probar_tuberia();
    probar_sockets();
    probar_desconexion();
    probar_formato();
    probar_errores();

    puts("Todas las pruebas de utils superadas.");

    return 0;
}