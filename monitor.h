/**
 * @file monitor.h
 * @brief Interfaz publica del modulo de consulta de episodios.
 *
 * Define la estructura de datos, las constantes y las operaciones
 * necesarias para consultar las calificaciones IMDb de la serie
 * A Knight of the Seven Kingdoms.
 *
 * Proyecto Final de Programacion de Sistemas.
 * Curso 2026-2027. Grupo 9.
 */

#ifndef MONITOR_H
#define MONITOR_H

#include <stdbool.h>
#include <stddef.h>

/** Longitud exacta del codigo identificador de un episodio. */
#define LONGITUD_CODIGO 6

/** Capacidad del campo titulo, incluido el terminador nulo. */
#define CAPACIDAD_TITULO 64

/**
 * @struct Episodio
 * @brief Representa la informacion registrada de un episodio.
 */
typedef struct {
    char codigo[LONGITUD_CODIGO + 1]; /**< Codigo identificador. */
    char titulo[CAPACIDAD_TITULO];    /**< Titulo del episodio. */
    double puntuacion;              /**< Calificacion IMDb. */
    int votos;                      /**< Cantidad de votos. */
} Episodio;

/**
 * @brief Comprueba el formato del codigo de un episodio.
 *
 * El codigo debe contener exactamente seis caracteres y
 * ajustarse al formato SddEdd, donde d representa un digito.
 * La validacion distingue mayusculas de minusculas.
 *
 * Esta funcion verifica el formato, no la existencia del episodio.
 *
 * @param codigo Cadena terminada en nulo que se desea validar.
 * @return true si el formato es correcto; false en caso contrario,
 *         incluido cuando codigo es NULL.
 */
bool codigo_valido(const char *codigo);

/**
 * @brief Busca un episodio mediante su codigo identificador.
 *
 * Realiza una busqueda secuencial sobre la base de datos.
 * El registro devuelto es de solo lectura y permanece disponible
 * durante toda la ejecucion del proceso.
 *
 * @param codigo Cadena terminada en nulo con el codigo del episodio.
 * @return Puntero constante al episodio encontrado, o NULL si
 *         el codigo es invalido o el episodio no existe.
 *
 * @note El llamador no debe modificar ni liberar el registro.
 */
const Episodio *buscar_episodio(const char *codigo);

/**
 * @brief Obtiene la cantidad de episodios almacenados.
 *
 * @return Numero total de registros de la base de datos.
 */
size_t cantidad_episodios(void);

#endif /* MONITOR_H */