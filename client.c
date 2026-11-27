/**
 * @file client.c
 * @brief Cliente TCP del sistema de consulta de calificaciones IMDb.
 *
 * Permite consultar los datos de los episodios de la serie
 * A Knight of the Seven Kingdoms mediante una conexion TCP
 * con el servidor concurrente.
 *
 * Admite solicitudes interactivas y mediante argumentos
 * de la linea de comandos.
 *
 * Reutiliza las funciones auxiliares y las constantes
 * proporcionadas por la biblioteca estatica libmonitor.a.
 *
 * Proyecto Final de Programacion de Sistemas.
 * Curso 2026-2027. Grupo 9.
 */

#define _POSIX_C_SOURCE 200809L

#include "utils.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

/** Direccion IPv4 utilizada cuando no se especifica un servidor. */
#define IP_PREDETERMINADA "127.0.0.1"

/** Tiempo de espera por operacion de comunicacion, en segundos. */
#define TIEMPO_ESPERA 30

/** Capacidad del buffer destinado a la entrada por teclado. */
#define CAPACIDAD_ENTRADA 128

/**
 * @brief Muestra las instrucciones de utilizacion del cliente.
 *
 * Describe las modalidades de ejecucion disponibles y
 * proporciona ejemplos de consultas.
 *
 * @param programa Nombre del ejecutable.
 * @param salida Flujo donde se muestran las instrucciones.
 */
static void mostrar_ayuda(
    const char *programa,
    FILE *salida
)
{
    fprintf(
        salida,
        "Uso:\n"
        "  %s\n"
        "  %s CODIGO\n"
        "  %s IP_SERVIDOR CODIGO\n"
        "  %s --help\n\n"
        "Ejemplos:\n"
        "  %s S01E01\n"
        "  %s 127.0.0.1 S01E05\n\n"
        "Puerto TCP: %d\n"
        "Formato del codigo: SddEdd\n",
        programa,
        programa,
        programa,
        programa,
        programa,
        programa,
        PUERTO_SERVIDOR
    );
}

/**
 * @brief Solicita al usuario el codigo de un episodio.
 *
 * Lee la entrada estandar, elimina el salto de linea
 * y verifica que el codigo posea el formato requerido.
 *
 * @param codigo Buffer de al menos LONGITUD_CODIGO + 1 bytes.
 *
 * @return 0 si se obtiene un codigo valido;
 *        -1 si la entrada es incorrecta o no puede leerse.
 */
static int solicitar_codigo(
    char codigo[LONGITUD_CODIGO + 1]
)
{
    char entrada[CAPACIDAD_ENTRADA];

    printf(
        "Introduzca el codigo del episodio "
        "(ej. S01E01): "
    );

    fflush(stdout);

    /* Comprobar si la lectura de la entrada es correcta. */
    if (fgets(
            entrada,
            sizeof entrada,
            stdin
        ) == NULL) {

        fputs(
            "ERROR: no se pudo leer el codigo.\n",
            stderr
        );

        return -1;
    }

    /*
     * Eliminar el salto de linea introducido al
     * presionar Enter.
     */
    char *salto = strchr(entrada, '\n');

    if (salto != NULL) {
        *salto = '\0';
    }

    /* Rechazar codigos que no cumplan el formato. */
    if (!codigo_valido(entrada)) {

        fputs(
            "ERROR: formato de codigo invalido. "
            "Debe utilizar seis caracteres: SddEdd.\n",
            stderr
        );

        return -1;
    }

    /*
     * Copiar los seis caracteres y el terminador nulo.
     * La longitud fue comprobada previamente.
     */
    memcpy(
        codigo,
        entrada,
        LONGITUD_CODIGO + 1
    );

    return 0;
}

/**
 * @brief Configura los tiempos de espera de comunicacion.
 *
 * Establece limites para las operaciones bloqueantes
 * de recepcion y envio de datos mediante el socket.
 *
 * @param fd Descriptor del socket.
 *
 * @return 0 si la configuracion es correcta;
 *        -1 si ocurre un error, con errno establecido.
 */
static int configurar_tiempos(int fd)
{
    struct timeval tiempo = {
        .tv_sec = TIEMPO_ESPERA,
        .tv_usec = 0
    };

    if (setsockopt(
            fd,
            SOL_SOCKET,
            SO_RCVTIMEO,
            &tiempo,
            sizeof tiempo
        ) == -1) {

        return -1;
    }

    return setsockopt(
        fd,
        SOL_SOCKET,
        SO_SNDTIMEO,
        &tiempo,
        sizeof tiempo
    );
}

/**
 * @brief Establece una conexion TCP con el servidor.
 *
 * Convierte la direccion IPv4 textual a su representacion
 * binaria, crea el socket y establece la conexion con
 * el puerto definido en la biblioteca.
 *
 * @param ip Direccion IPv4 del servidor.
 *
 * @return Descriptor del socket conectado si tiene exito;
 *        -1 si ocurre un error, con errno establecido.
 */
static int conectar_servidor(const char *ip)
{
    struct sockaddr_in direccion = {0};

    direccion.sin_family = AF_INET;
    direccion.sin_port = htons(PUERTO_SERVIDOR);

    /* Convertir y validar la direccion IPv4 proporcionada. */
    int resultado = inet_pton(
        AF_INET,
        ip,
        &direccion.sin_addr
    );

    if (resultado != 1) {

        if (resultado == 0) {
            errno = EINVAL;
        }

        return -1;
    }

    /* Crear un socket IPv4 orientado a conexion. */
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == -1) {
        return -1;
    }

    /*
     * Configurar el socket antes de establecer la conexion.
     * Si alguna operacion falla, liberar el descriptor.
     */
    if (configurar_tiempos(fd) == -1 ||
        connect(
            fd,
            (struct sockaddr *)&direccion,
            sizeof direccion
        ) == -1) {

        int error = errno;

        close(fd);

        errno = error;

        return -1;
    }

    return fd;
}

/**
 * @brief Recibe y muestra la respuesta completa del servidor.
 *
 * Lee bloques de bytes hasta detectar el cierre del flujo
 * TCP. No supone que la respuesta pueda recibirse mediante
 * una unica llamada a recv().
 *
 * @param fd Descriptor del socket conectado.
 *
 * @return 0 si se recibe y muestra una respuesta no vacia;
 *        -1 si ocurre un error de comunicacion o de salida.
 */
static int recibir_respuesta(int fd)
{
    char buffer[CAPACIDAD_RESPUESTA];

    bool datos_recibidos = false;

    for (;;) {

        ssize_t n = recv(
            fd,
            buffer,
            sizeof buffer,
            0
        );

        /* Detectar el final del flujo TCP. */
        if (n == 0) {

            /*
             * Una conexion cerrada sin respuesta no se
             * considera una consulta satisfactoria.
             */
            if (!datos_recibidos) {
                errno = EPROTO;
                return -1;
            }

            return fflush(stdout) == EOF ? -1 : 0;
        }

        /* Gestionar errores e interrupciones por señales. */
        if (n < 0) {

            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        datos_recibidos = true;

        /*
         * Mostrar exactamente los bytes recibidos.
         * No se presupone que el bloque recibido
         * contenga un terminador nulo.
         */
        if (fwrite(
                buffer,
                1,
                (size_t)n,
                stdout
            ) != (size_t)n) {

            return -1;
        }
    }
}

/**
 * @brief Ejecuta una consulta al servidor TCP.
 *
 * Establece la conexion, transmite exactamente los seis
 * caracteres del codigo y recibe la respuesta completa.
 *
 * Garantiza la liberacion del socket al finalizar,
 * incluso cuando se producen errores de comunicacion.
 *
 * @param ip Direccion IPv4 del servidor.
 * @param codigo Codigo valido del episodio solicitado.
 *
 * @return EXIT_SUCCESS si se completa la comunicacion;
 *         EXIT_FAILURE si ocurre un error.
 *
 * @note Una respuesta de error emitida por el servidor
 *       tambien constituye una comunicacion completada.
 */
static int consultar_servidor(
    const char *ip,
    const char *codigo
)
{
    int fd = conectar_servidor(ip);

    if (fd == -1) {
        perror("Conexion con el servidor");
        return EXIT_FAILURE;
    }

    int estado = EXIT_FAILURE;

    /*
     * Transmitir unicamente los seis caracteres del
     * identificador, sin salto de linea ni terminador nulo.
     */
    if (enviar_completo(
            fd,
            codigo,
            LONGITUD_CODIGO
        ) == -1) {

        perror("Envio de la solicitud");

        goto fin;
    }

    /*
     * Recibir la respuesta hasta que el servidor
     * cierre el extremo correspondiente de la conexion.
     */
    if (recibir_respuesta(fd) == -1) {

        perror("Recepcion de la respuesta");

        goto fin;
    }

    estado = EXIT_SUCCESS;

fin:

    /* Liberar el descriptor en todos los recorridos. */
    if (close(fd) == -1) {

        perror("Cierre del socket");

        estado = EXIT_FAILURE;
    }

    return estado;
}

/**
 * @brief Punto de entrada del cliente TCP.
 *
 * Permite especificar el codigo mediante argumentos o
 * introducirlo de manera interactiva.
 *
 * Cuando no se especifica una direccion IPv4, se utiliza
 * la direccion local 127.0.0.1.
 *
 * @param argc Cantidad de argumentos recibidos.
 * @param argv Arreglo de argumentos de la linea de comandos.
 *
 * @return EXIT_SUCCESS o EXIT_FAILURE segun el resultado.
 */
int main(int argc, char *argv[])
{
    const char *ip = IP_PREDETERMINADA;

    char codigo[LONGITUD_CODIGO + 1];

    /* Mostrar la ayuda sin establecer ninguna conexion. */
    if (argc == 2 &&
        strcmp(argv[1], "--help") == 0) {

        mostrar_ayuda(argv[0], stdout);

        return EXIT_SUCCESS;
    }

    /* Modo interactivo. */
    if (argc == 1) {

        if (solicitar_codigo(codigo) == -1) {
            return EXIT_FAILURE;
        }

    } else if (argc == 2 || argc == 3) {

        /* Obtener la direccion y el codigo por argumentos. */
        const char *entrada = argv[argc - 1];

        if (argc == 3) {
            ip = argv[1];
        }

        /* Validar antes de copiar y transmitir el codigo. */
        if (!codigo_valido(entrada)) {

            fputs(
                "ERROR: formato de codigo invalido. "
                "Debe utilizar seis caracteres: SddEdd.\n",
                stderr
            );

            return EXIT_FAILURE;
        }

        memcpy(
            codigo,
            entrada,
            LONGITUD_CODIGO + 1
        );

    } else {

        /* Rechazar cantidades incorrectas de argumentos. */
        mostrar_ayuda(argv[0], stderr);

        return EXIT_FAILURE;
    }

    /* Establecer la conexion y ejecutar la consulta. */
    return consultar_servidor(ip, codigo);
}