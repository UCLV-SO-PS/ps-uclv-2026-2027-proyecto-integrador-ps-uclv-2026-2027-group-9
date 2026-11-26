/**
 * @file utils.c
 * @brief Implementacion de las utilidades de comunicacion y formato.
 *
 * Contiene operaciones auxiliares para la transferencia fiable
 * de datos mediante descriptores y sockets, asi como la
 * construccion de respuestas textuales.
 *
 * Proyecto Final de Programacion de Sistemas.
 * Curso 2026-2027. Grupo 9.
 */

#include "utils.h"

#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

/** Cantidad maxima de bytes por operacion individual de E/S. */
#define BLOQUE_IO 4096

/**
 * Lee una cantidad exacta de bytes, gestionando lecturas
 * parciales e interrupciones por senales.
 */
int leer_completo(int fd, void *buffer, size_t cantidad)
{
    unsigned char *destino = buffer;
    size_t total = 0;

    /* Validar el buffer antes de utilizarlo. */
    if (buffer == NULL && cantidad != 0) {
        errno = EINVAL;
        return -1;
    }

    /*
     * Rechazar descriptores negativos antes de realizar
     * una llamada al sistema que requiera un descriptor valido.
     *
     * Las operaciones de cero bytes no requieren realizar E/S.
     */
    if (fd < 0 && cantidad != 0) {
        errno = EBADF;
        return -1;
    }
    
    /* Continuar hasta recibir todos los bytes solicitados. */
    while (total < cantidad) {
        size_t restante = cantidad - total;

        size_t bloque = restante > BLOQUE_IO
                       ? BLOQUE_IO
                       : restante;

        ssize_t n = read(fd, destino + total, bloque);

        /* El descriptor ha alcanzado el fin del flujo. */
        if (n == 0) {
            return 1;
        }

        if (n < 0) {
            /*
             * Una senal puede interrumpir la llamada al sistema.
             * En ese caso se reintenta la operacion.
             */
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        /* Actualizar la cantidad de bytes recibidos. */
        total += (size_t)n;
    }

    return 0;
}

/**
 * @brief Implementa la transferencia completa de datos.
 *
 * Funcion privada que concentra la logica comun de escritura
 * sobre descriptores y envio mediante sockets.
 *
 * @param fd Descriptor de destino.
 * @param buffer Datos que se desean transferir.
 * @param cantidad Numero total de bytes.
 * @param socket_tcp true para utilizar send();
 *                   false para utilizar write().
 *
 * @return 0 si la transferencia se completa;
 *        -1 si ocurre un error, con errno establecido.
 */
static int transferir(int fd, const void *buffer,size_t cantidad, bool socket_tcp){
    const unsigned char *origen = buffer;
    size_t total = 0;

    /* Validar el buffer de origen. */
    if (buffer == NULL && cantidad != 0) {
        errno = EINVAL;
        return -1;
    }

    /*
     * Validar el descriptor antes de iniciar la transferencia.
     * Las operaciones de cero bytes no requieren realizar E/S.
     */
    if (fd < 0 && cantidad != 0) {
        errno = EBADF;
        return -1;
    }

    while (total < cantidad) {
        size_t restante = cantidad - total;

        size_t bloque = restante > BLOQUE_IO
                       ? BLOQUE_IO
                       : restante;

        ssize_t n;

        if (socket_tcp) {
            /*
             * MSG_NOSIGNAL evita que el proceso termine
             * por SIGPIPE si el extremo remoto se desconecta.
             */
            n = send(
                fd,
                origen + total,
                bloque,
                MSG_NOSIGNAL
            );
        } else {
            n = write(fd, origen + total, bloque);
        }

        if (n < 0) {
            /* Reintentar si la operacion fue interrumpida. */
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        /*
         * Una transferencia sin progreso impediria finalizar
         * el ciclo. Se considera un error de entrada/salida.
         */
        if (n == 0) {
            errno = EIO;
            return -1;
        }

        total += (size_t)n;
    }

    return 0;
}

/**
 * Escribe completamente los datos sobre un descriptor.
 */
int escribir_completo(
    int fd,
    const void *buffer,
    size_t cantidad
)
{
    return transferir(fd, buffer, cantidad, false);
}

/**
 * Envia completamente los datos mediante un socket conectado.
 */
int enviar_completo(
    int socket_fd,
    const void *buffer,
    size_t cantidad
)
{
    return transferir(socket_fd, buffer, cantidad, true);
}

/**
 * Recibe un codigo de longitud fija y construye una
 * cadena terminada en nulo.
 */
int recibir_codigo(
    int socket_fd,
    char codigo[LONGITUD_CODIGO + 1]
)
{
    if (codigo == NULL) {
        errno = EINVAL;
        return -1;
    }

    int resultado = leer_completo(
        socket_fd,
        codigo,
        LONGITUD_CODIGO
    );

    /*
     * Si la recepcion falla, se invalida la cadena para
     * impedir que se procese accidentalmente un codigo parcial.
     */
    if (resultado != 0) {
        codigo[0] = '\0';
    }

    /* El terminador nulo no se transmite por la red. */
    codigo[LONGITUD_CODIGO] = '\0';

    return resultado;
}

/**
 * Genera una respuesta textual utilizando los datos
 * de un episodio y un buffer de capacidad limitada.
 */
int formatear_episodio(
    const Episodio *episodio,
    char *buffer,
    size_t capacidad
)
{
    /* Comprobar los argumentos recibidos. */
    if (episodio == NULL ||
        buffer == NULL ||
        capacidad == 0) {
        errno = EINVAL;
        return -1;
    }

    /*
     * snprintf limita la escritura a la capacidad disponible
     * e incluye el terminador nulo cuando existe espacio.
     */
    int n = snprintf(
        buffer,
        capacidad,
        "========================================\n"
        "     IMDb - MONITOR DE EPISODIOS\n"
        "========================================\n"
        "Codigo:     %s\n"
        "Titulo:     %s\n"
        "Puntuacion: %.1f/10\n"
        "Votos:      %d\n"
        "========================================\n",
        episodio->codigo,
        episodio->titulo,
        episodio->puntuacion,
        episodio->votos
    );

    /* Comprobar posibles errores durante el formato. */
    if (n < 0) {
        buffer[0] = '\0';
        return -1;
    }

    /* Detectar si la respuesta no cabe completamente. */
    if ((size_t)n >= capacidad) {
        buffer[0] = '\0';
        errno = ENOSPC;
        return -1;
    }

    return n;
}