/**
 * @file utils.h
 * @brief Interfaz publica de las utilidades de comunicacion y formato.
 *
 * Proporciona operaciones de lectura y escritura completas sobre
 * descriptores, envio de datos mediante sockets TCP, recepcion
 * de codigos y construccion de respuestas textuales.
 *
 * Proyecto Final de Programacion de Sistemas.
 * Curso 2026-2027. Grupo 9.
 */

#ifndef UTILS_H
#define UTILS_H

#include "monitor.h"

/** Puerto TCP establecido en la orientacion del proyecto. */
#define PUERTO_SERVIDOR 6666

/** Capacidad del buffer destinado a las respuestas del servidor. */
#define CAPACIDAD_RESPUESTA 512

/**
 * @brief Lee una cantidad exacta de bytes desde un descriptor.
 *
 * Repite la operacion cuando se producen lecturas parciales
 * o interrupciones provocadas por senales (EINTR).
 *
 * @param fd Descriptor abierto para lectura.
 * @param buffer Buffer con capacidad suficiente.
 * @param cantidad Numero de bytes que se desea leer.
 *
 * @return 0 si se reciben todos los bytes solicitados;
 *         1 si se alcanza el fin del flujo antes de completarlos;
 *        -1 si ocurre un error, con errno establecido.
 *
 * @note Diseñada para descriptores en modo bloqueante.
 *       Ante un resultado distinto de cero, el buffer puede
 *       contener datos parciales.
 */
int leer_completo(int fd, void *buffer, size_t cantidad);

/**
 * @brief Escribe una cantidad exacta de bytes en un descriptor.
 *
 * Gestiona escrituras parciales e interrupciones por senales.
 *
 * @param fd Descriptor abierto para escritura.
 * @param buffer Buffer que contiene los datos que se escribiran.
 * @param cantidad Numero de bytes que se desea escribir.
 *
 * @return 0 si se escriben todos los bytes;
 *        -1 si ocurre un error, con errno establecido.
 *
 * @note Diseñada para descriptores en modo bloqueante.
 *       El llamador debe gestionar SIGPIPE si corresponde.
 */
int escribir_completo(int fd, const void *buffer, size_t cantidad);

/**
 * @brief Envia una cantidad exacta de bytes mediante un socket.
 *
 * Gestiona envios parciales e interrupciones por senales.
 * Evita la generacion de SIGPIPE mediante MSG_NOSIGNAL.
 *
 * @param socket_fd Descriptor del socket conectado.
 * @param buffer Buffer que contiene los datos que se enviaran.
 * @param cantidad Numero de bytes que se desea enviar.
 *
 * @return 0 si todos los bytes se envian correctamente;
 *        -1 si ocurre un error, con errno establecido.
 */
int enviar_completo(int socket_fd, const void *buffer, size_t cantidad);

/**
 * @brief Recibe los seis caracteres del codigo de un episodio.
 *
 * Agrega localmente el terminador nulo para obtener una
 * cadena valida del lenguaje C.
 *
 * Esta funcion realiza la recepcion, no la validacion
 * sintactica ni la busqueda del episodio.
 *
 * @param socket_fd Descriptor del socket conectado.
 * @param codigo Buffer de al menos LONGITUD_CODIGO + 1 bytes.
 *
 * @return 0 si se recibe el codigo completo;
 *         1 si el cliente cierra el flujo prematuramente;
 *        -1 si ocurre un error, con errno establecido.
 *
 * @note Ante un fallo, codigo representa una cadena vacia,
 *       siempre que el puntero proporcionado no sea NULL.
 */
int recibir_codigo(
    int socket_fd,
    char codigo[LONGITUD_CODIGO + 1]
);

/**
 * @brief Construye una respuesta textual con los datos del episodio.
 *
 * Utiliza un buffer proporcionado por el llamador y evita
 * escrituras fuera de sus limites.
 *
 * @param episodio Puntero constante a un episodio valido.
 * @param buffer Buffer donde se almacenara la respuesta.
 * @param capacidad Capacidad total del buffer en bytes.
 *
 * @return Numero de caracteres escritos, sin incluir el
 *         terminador nulo, si la operacion es exitosa;
 *        -1 si los argumentos son invalidos, el buffer resulta
 *         insuficiente o se produce un error de formato.
 *
 * @note No realiza asignaciones dinamicas de memoria.
 */
int formatear_episodio(
    const Episodio *episodio,
    char *buffer,
    size_t capacidad
);

#endif /* UTILS_H */