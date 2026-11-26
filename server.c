/**
 * @file server.c
 * @brief Servidor concurrente TCP de calificaciones IMDb.
 *
 * Implementa un servidor IPv4 basado en procesos, capaz de
 * atender simultaneamente multiples clientes mediante fork().
 *
 * Utiliza tuberias sin nombre para comunicar la direccion IP
 * de cada cliente al proceso padre y señales para controlar
 * la finalizacion de los procesos hijos.
 *
 * Las consultas y operaciones auxiliares se realizan mediante
 * las funciones proporcionadas por la biblioteca libmonitor.a.
 *
 * Proyecto Final de Programacion de Sistemas.
 * Curso 2026-2027. Grupo 9.
 */

#define _POSIX_C_SOURCE 200809L

#include "utils.h"

#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

/** Capacidad solicitada para la cola de conexiones pendientes. */
#define COLA_CONEXIONES 32

/** Tiempo de espera por operacion de comunicacion, en segundos. */
#define TIEMPO_ESPERA 30

/**
 * @brief Recolecta los procesos hijos que han finalizado.
 *
 * Manejador asincrono de SIGCHLD. Utiliza waitpid() en modo
 * no bloqueante para evitar la acumulacion de procesos zombis.
 *
 * @param senal Numero de la señal recibida.
 *
 * @note Preserva errno y no utiliza funciones que no sean
 *       seguras dentro de un manejador asincrono de señales.
 */
static void recolectar_hijos(int senal)
{
    int errno_anterior = errno;

    /* El parametro es obligatorio en la interfaz del manejador. */
    (void)senal;

    /*
     * Una sola señal puede corresponder a varios hijos
     * finalizados. Por ello se repite la recoleccion.
     */
    for (;;) {
        pid_t pid = waitpid(-1, NULL, WNOHANG);

        if (pid > 0) {
            continue;
        }

        if (pid == -1 && errno == EINTR) {
            continue;
        }

        /* No quedan hijos finalizados pendientes de recoger. */
        break;
    }

    errno = errno_anterior;
}

/**
 * @brief Configura el tratamiento de las señales del servidor.
 *
 * SIGINT se ignora por exigencia de la orientacion.
 * SIGPIPE se ignora para gestionar errores de escritura.
 *
 * En ejecucion normal, SIGCHLD activa el manejador encargado
 * de recolectar los hijos finalizados.
 *
 * Durante la prueba automatica, los hijos se recolectan
 * explicitamente mediante waitpid().
 *
 * @param prueba Indica si se ejecuta el modo --test-run.
 * @return 0 si la configuracion es correcta;
 *        -1 si ocurre un error.
 */
static int configurar_senales(int prueba)
{
    struct sigaction accion = {0};

    if (sigemptyset(&accion.sa_mask) == -1) {
        return -1;
    }

    /* Ignorar CTRL+C y errores de tuberias sin lectores. */
    accion.sa_handler = SIG_IGN;

    if (sigaction(SIGINT, &accion, NULL) == -1) {
        return -1;
    }

    if (sigaction(SIGPIPE, &accion, NULL) == -1) {
        return -1;
    }

    /*
     * En el smoke test se utiliza waitpid() explicitamente
     * para comprobar el estado de finalizacion del hijo.
     */
    if (prueba) {
        accion.sa_handler = SIG_DFL;
        accion.sa_flags = 0;
    } else {
        accion.sa_handler = recolectar_hijos;
        accion.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    }

    return sigaction(SIGCHLD, &accion, NULL);
}

/**
 * @brief Crea y configura un socket TCP de escucha.
 *
 * Permite reutilizar la direccion local y establece la cola
 * de conexiones pendientes.
 *
 * @param ip Direccion IPv4 en orden de bytes del anfitrion.
 * @param puerto Puerto TCP en orden de bytes del anfitrion.
 *
 * @return Descriptor del socket de escucha si tiene exito;
 *        -1 si ocurre un error, con errno establecido.
 */
static int crear_escucha(uint32_t ip, uint16_t puerto)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == -1) {
        return -1;
    }

    int activar = 1;

    struct sockaddr_in direccion = {0};

    direccion.sin_family = AF_INET;
    direccion.sin_addr.s_addr = htonl(ip);
    direccion.sin_port = htons(puerto);

    /*
     * SO_REUSEADDR facilita reiniciar el servidor despues
     * de su finalizacion.
     */
    if (setsockopt(
            fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &activar,
            sizeof activar
        ) == -1 ||
        bind(
            fd,
            (struct sockaddr *)&direccion,
            sizeof direccion
        ) == -1 ||
        listen(fd, COLA_CONEXIONES) == -1) {

        /* Conservar la causa original del error. */
        int error = errno;

        close(fd);

        errno = error;

        return -1;
    }

    return fd;
}

/**
 * @brief Configura los tiempos de espera de un socket.
 *
 * Evita que una operacion individual de lectura o escritura
 * permanezca bloqueada indefinidamente ante clientes inactivos.
 *
 * @param fd Descriptor del socket conectado.
 * @return 0 si ambas opciones se configuran correctamente;
 *        -1 si ocurre un error.
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
 * @brief Acepta una conexion entrante.
 *
 * Reintenta accept() cuando la operacion resulta interrumpida
 * por una señal.
 *
 * @param escucha Descriptor del socket de escucha.
 * @param direccion Estructura donde se almacena la direccion
 *                  del cliente conectado.
 *
 * @return Descriptor del socket conectado;
 *        -1 si ocurre un error.
 */
static int aceptar_cliente(
    int escucha,
    struct sockaddr_in *direccion
)
{
    int fd;

    do {
        socklen_t longitud = sizeof *direccion;

        fd = accept(
            escucha,
            (struct sockaddr *)direccion,
            &longitud
        );

    } while (fd == -1 && errno == EINTR);

    return fd;
}

/**
 * @brief Envia un mensaje de error textual al cliente.
 *
 * @param fd Descriptor del socket conectado.
 * @param mensaje Cadena terminada en nulo con el error.
 *
 * @return 0 si se envia completamente;
 *        -1 si ocurre un error de comunicacion.
 */
static int enviar_error(int fd, const char *mensaje)
{
    return enviar_completo(fd, mensaje, strlen(mensaje));
}

/**
 * @brief Procesa una solicitud de consulta de episodios.
 *
 * Recibe el codigo del episodio, comprueba su formato,
 * consulta la base de datos y envia una respuesta textual.
 *
 * Distingue entre solicitudes incompletas, codigos invalidos
 * y episodios inexistentes.
 *
 * @param fd Descriptor del socket conectado al cliente.
 *
 * @return 0 si se procesa y responde la solicitud;
 *        -1 si ocurre un error interno o de comunicacion.
 */
static int atender_solicitud(int fd)
{
    char codigo[LONGITUD_CODIGO + 1];
    char respuesta[CAPACIDAD_RESPUESTA];

    /* Recibir los seis bytes del codigo identificador. */
    int resultado = recibir_codigo(fd, codigo);

    if (resultado == -1) {
        return -1;
    }

    /* Detectar el cierre prematuro del flujo del cliente. */
    if (resultado == 1) {
        return enviar_error(
            fd,
            "ERROR: codigo incompleto.\n"
        );
    }

    /* Comprobar la estructura sintactica del codigo. */
    if (!codigo_valido(codigo)) {
        return enviar_error(
            fd,
            "ERROR: formato de codigo invalido.\n"
        );
    }

    /* Consultar la base de datos de la biblioteca estatica. */
    const Episodio *episodio = buscar_episodio(codigo);

    if (episodio == NULL) {
        return enviar_error(
            fd,
            "ERROR: episodio no encontrado.\n"
        );
    }

    /* Construir la respuesta dentro de los limites del buffer. */
    int n = formatear_episodio(
        episodio,
        respuesta,
        sizeof respuesta
    );

    if (n == -1) {
        return -1;
    }

    /* Transmitir la respuesta sin incluir el terminador nulo. */
    return enviar_completo(fd, respuesta, (size_t)n);
}

/**
 * @brief Crea un proceso hijo para atender una conexion.
 *
 * Establece una tuberia sin nombre antes de ejecutar fork().
 * El hijo comunica la IP del cliente al padre antes de
 * procesar la solicitud.
 *
 * El padre recibe la direccion IP, la imprime junto al PID
 * correspondiente y queda disponible para aceptar conexiones.
 *
 * @param escucha Descriptor del socket de escucha.
 * @param cliente Descriptor del socket conectado.
 * @param direccion Direccion IPv4 del cliente.
 * @param fd_extra Descriptor adicional heredado que debe
 *                 cerrar el hijo durante el smoke test.
 *                 Se utiliza -1 en la ejecucion normal.
 *
 * @return PID del hijo creado;
 *        -1 si no se puede crear la tuberia o el proceso.
 *
 * @note La funcion asume la responsabilidad de cerrar el
 *       descriptor cliente en el proceso padre.
 */
static pid_t despachar(
    int escucha,
    int cliente,
    const struct sockaddr_in *direccion,
    int fd_extra
)
{
    int p[2];

    /* Crear la tuberia antes de generar el nuevo proceso. */
    if (pipe(p) == -1) {
        perror("pipe");
        close(cliente);
        return -1;
    }

    pid_t pid = fork();

    /* Error durante la creacion del proceso hijo. */
    if (pid == -1) {
        perror("fork");

        close(p[0]);
        close(p[1]);
        close(cliente);

        return -1;
    }

    /* ================= PROCESO HIJO ================= */
    if (pid == 0) {

        /* El hijo no acepta conexiones ni lee de la tuberia. */
        close(escucha);
        close(p[0]);

        /*
         * En la prueba automatica se hereda un descriptor
         * adicional que pertenece al cliente de prueba.
         */
        if (fd_extra >= 0) {
            close(fd_extra);
        }

        /*
         * INET_ADDRSTRLEN proporciona capacidad suficiente
         * para representar una direccion IPv4 como texto.
         */
        char ip[INET_ADDRSTRLEN] = {0};

        /*
         * Convertir la direccion binaria y comunicarla
         * al padre antes de atender la solicitud.
         */
        if (inet_ntop(
                AF_INET,
                &direccion->sin_addr,
                ip,
                sizeof ip
            ) == NULL ||
            escribir_completo(
                p[1],
                ip,
                sizeof ip
            ) == -1) {

            perror("Notificacion IPC");

            close(p[1]);
            close(cliente);

            _exit(EXIT_FAILURE);
        }

        /* La notificacion al padre ha finalizado. */
        close(p[1]);

        /* Configurar y atender la conexion del cliente. */
        int resultado = configurar_tiempos(cliente);

        if (resultado == 0) {
            resultado = atender_solicitud(cliente);
        }

        if (resultado == -1) {
            perror("Atencion del cliente");
        }

        /* Liberar el socket conectado antes de finalizar. */
        close(cliente);

        /*
         * _exit evita volver a ejecutar rutinas de
         * finalizacion y vaciar buffers heredados del padre.
         */
        _exit(
            resultado == 0
            ? EXIT_SUCCESS
            : EXIT_FAILURE
        );
    }

    /* ================= PROCESO PADRE ================= */

    /*
     * El padre no atiende directamente al cliente
     * ni escribe en la tuberia.
     */
    close(cliente);
    close(p[1]);

    char ip[INET_ADDRSTRLEN] = {0};

    /* Recibir la direccion IP enviada por el hijo. */
    int resultado = leer_completo(
        p[0],
        ip,
        sizeof ip
    );

    close(p[0]);

    if (resultado == 0) {

        /* Garantizar la terminacion de la cadena recibida. */
        ip[sizeof ip - 1] = '\0';

        printf(
            "Cliente IP: %s | PID hijo: %ld\n",
            ip,
            (long)pid
        );

        fflush(stdout);

    } else {

        fprintf(
            stderr,
            "No se recibio la IP del hijo %ld.\n",
            (long)pid
        );
    }

    /*
     * El padre no espera aqui la finalizacion del hijo.
     * En ejecucion normal, SIGCHLD permite recolectarlo
     * posteriormente sin bloquear la aceptacion de clientes.
     */
    return pid;
}

/**
 * @brief Ejecuta una prueba automatica del servidor.
 *
 * Comprueba la disponibilidad de la base de datos, crea
 * un socket TCP local, establece una conexion, genera
 * un proceso hijo y verifica una respuesta completa.
 *
 * Utiliza un puerto temporal asignado por el sistema
 * operativo para evitar conflictos con el puerto 6666.
 *
 * @return EXIT_SUCCESS si todas las comprobaciones finalizan
 *         correctamente; EXIT_FAILURE en caso contrario.
 */
static int prueba_automatica(void)
{
    int estado = EXIT_FAILURE;

    int escucha = -1;
    int cliente = -1;

    pid_t hijo = -1;

    struct sockaddr_in direccion = {0};
    struct sockaddr_in remoto = {0};

    socklen_t longitud = sizeof direccion;

    char esperado[CAPACIDAD_RESPUESTA];
    char recibido[CAPACIDAD_RESPUESTA];
    char extra;

    /*
     * Comprobar que la base de datos enlazada contiene
     * la cantidad de registros esperada y un episodio conocido.
     */
    const Episodio *episodio = buscar_episodio("S01E05");

    if (cantidad_episodios() != 6 ||
        episodio == NULL ||
        episodio->puntuacion != 9.7 ||
        episodio->votos != 68300 ||
        strcmp(
            episodio->titulo,
            "In the Name of the Mother"
        ) != 0) {

        goto fin;
    }

    /* Obtener la respuesta esperada. */
    int n = formatear_episodio(
        episodio,
        esperado,
        sizeof esperado
    );

    if (n <= 0) {
        goto fin;
    }

    /*
     * El puerto cero permite que el sistema operativo
     * seleccione un puerto disponible automaticamente.
     */
    escucha = crear_escucha(INADDR_LOOPBACK, 0);

    if (escucha == -1 ||
        getsockname(
            escucha,
            (struct sockaddr *)&direccion,
            &longitud
        ) == -1) {

        goto fin;
    }

    /* Crear y conectar el socket del cliente de prueba. */
    cliente = socket(AF_INET, SOCK_STREAM, 0);

    if (cliente == -1 ||
        configurar_tiempos(cliente) == -1 ||
        connect(
            cliente,
            (struct sockaddr *)&direccion,
            longitud
        ) == -1) {

        goto fin;
    }

    /* Aceptar la conexion de prueba. */
    int conectado = aceptar_cliente(escucha, &remoto);

    if (conectado == -1) {
        goto fin;
    }

    /*
     * Utilizar el mismo mecanismo de despacho empleado
     * durante la ejecucion normal del servidor.
     */
    hijo = despachar(
        escucha,
        conectado,
        &remoto,
        cliente
    );

    /*
     * Enviar una solicitud real y verificar que todos
     * los bytes de la respuesta coinciden con lo esperado.
     */
    if (hijo == -1 ||
        enviar_completo(
            cliente,
            "S01E05",
            LONGITUD_CODIGO
        ) == -1 ||
        leer_completo(
            cliente,
            recibido,
            (size_t)n
        ) != 0 ||
        memcmp(recibido, esperado, (size_t)n) != 0 ||
        leer_completo(cliente, &extra, 1) != 1) {

        goto fin;
    }

    estado = EXIT_SUCCESS;

fin:

    /* Centralizar la liberacion de recursos de la prueba. */
    if (cliente >= 0) {
        close(cliente);
    }

    if (escucha >= 0) {
        close(escucha);
    }

    /*
     * Comprobar que el proceso hijo finaliza correctamente
     * y recolectarlo explicitamente.
     */
    if (hijo > 0) {

        int estado_hijo;
        pid_t resultado;

        do {
            resultado = waitpid(
                hijo,
                &estado_hijo,
                0
            );

        } while (resultado == -1 && errno == EINTR);

        if (resultado == -1 ||
            !WIFEXITED(estado_hijo) ||
            WEXITSTATUS(estado_hijo) != EXIT_SUCCESS) {

            estado = EXIT_FAILURE;
        }
    }

    if (estado == EXIT_SUCCESS) {
        puts("Smoke test: OK");
    } else {
        fputs("Smoke test: ERROR\n", stderr);
    }

    return estado;
}

/**
 * @brief Punto de entrada del servidor concurrente.
 *
 * Admite dos modos de ejecucion:
 *
 *   ./server
 *   ./server --test-run
 *
 * En modo normal escucha conexiones TCP en el puerto 6666.
 * En modo de prueba verifica el funcionamiento del sistema
 * y finaliza automaticamente.
 *
 * @param argc Cantidad de argumentos recibidos.
 * @param argv Arreglo de argumentos de la linea de comandos.
 *
 * @return EXIT_SUCCESS o EXIT_FAILURE segun el resultado
 *         de la ejecucion.
 */
int main(int argc, char *argv[])
{
    int prueba =
        argc == 2 &&
        strcmp(argv[1], "--test-run") == 0;

    /* Rechazar argumentos que no formen parte de la interfaz. */
    if (argc != 1 && !prueba) {

        fprintf(
            stderr,
            "Uso: %s [--test-run]\n",
            argv[0]
        );

        return EXIT_FAILURE;
    }

    /* Configurar las señales antes de crear procesos hijos. */
    if (configurar_senales(prueba) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    /* Ejecutar la prueba automatica si fue solicitada. */
    if (prueba) {
        return prueba_automatica();
    }

    /* Inicializar el socket de escucha del servidor. */
    int escucha = crear_escucha(
        INADDR_ANY,
        PUERTO_SERVIDOR
    );

    if (escucha == -1) {
        perror("Inicializacion del servidor");
        return EXIT_FAILURE;
    }

    printf(
        "Servidor TCP escuchando en el puerto %d. PID: %ld\n",
        PUERTO_SERVIDOR,
        (long)getpid()
    );

    fflush(stdout);

    /* Ciclo principal de aceptacion de conexiones. */
    for (;;) {

        struct sockaddr_in direccion = {0};

        int cliente = aceptar_cliente(
            escucha,
            &direccion
        );

        if (cliente == -1) {

            /*
             * Una conexion abortada no debe provocar
             * la finalizacion del servidor.
             */
            if (errno == ECONNABORTED) {
                continue;
            }

            perror("accept");

            close(escucha);

            return EXIT_FAILURE;
        }

        /*
         * Crear un hijo independiente para cada conexion.
         * Ante fallos de recursos se evita repetir la
         * operacion inmediatamente de forma continua.
         */
        if (despachar(
                escucha,
                cliente,
                &direccion,
                -1
            ) == -1) {

            sleep(1);
        }
    }
}