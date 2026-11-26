/**
 * @file monitor.c
 * @brief Implementacion del modulo de consulta de episodios.
 *
 * Contiene la base de datos establecida en la orientacion
 * oficial del proyecto y las operaciones de validacion,
 * busqueda y conteo de episodios.
 *
 * Proyecto Final de Programacion de Sistemas.
 * Curso 2026-2027. Grupo 9.
 */

#include "monitor.h"

#include <string.h>

/**
 * @brief Base de datos de episodios de la serie.
 *
 * Los valores corresponden exactamente a los proporcionados
 * en la orientacion oficial de la tarea integradora.
 *
 * La tabla posee enlace interno mediante static y se declara
 * constante para impedir modificaciones accidentales.
 */
static const Episodio bd_episodios[] = {
    {"S01E01", "The Hedge Knight",         9.2, 45120},
    {"S01E02", "Hard Salt Beef",           8.8, 38900},
    {"S01E03", "The Squire",               9.5, 52400},
    {"S01E04", "Seven",                    9.1, 41200},
    {"S01E05", "In the Name of the Mother", 9.7, 68300},
    {"S01E06", "The Morrow",               9.4, 49800}
};

/**
 * @brief Determina si un caracter es un digito decimal.
 *
 * Funcion auxiliar privada utilizada durante la validacion
 * de los codigos identificadores.
 *
 * @param c Caracter que se desea comprobar.
 * @return true si pertenece al intervalo '0'-'9';
 *         false en caso contrario.
 */
static bool es_digito(char c)
{
    return c >= '0' && c <= '9';
}

/**
 * Valida la longitud y la estructura del codigo recibido.
 */
bool codigo_valido(const char *codigo)
{
    /* Comprobar el puntero antes de acceder a la cadena. */
    if (codigo == NULL || strlen(codigo) != LONGITUD_CODIGO) {
        return false;
    }

    /* Verificar el formato SddEdd. */
    return codigo[0] == 'S' &&
           es_digito(codigo[1]) &&
           es_digito(codigo[2]) &&
           codigo[3] == 'E' &&
           es_digito(codigo[4]) &&
           es_digito(codigo[5]);
}

/**
 * Calcula la cantidad de registros a partir del arreglo,
 * evitando mantener manualmente una constante adicional.
 */ 
size_t cantidad_episodios(void)
{
    return sizeof bd_episodios / sizeof bd_episodios[0];
}

/**
 * Busca secuencialmente un episodio a partir de su codigo.
 */
const Episodio *buscar_episodio(const char *codigo)
{
    /* Descartar solicitudes con formato incorrecto. */
    if (!codigo_valido(codigo)) {
        return NULL;
    }

    /* Recorrer la base de datos hasta encontrar una coincidencia. */
    for (size_t i = 0; i < cantidad_episodios(); ++i) {
        if (strcmp(bd_episodios[i].codigo, codigo) == 0) {
            return &bd_episodios[i];
        }
    }

    /* El codigo es valido, pero no existe en la base de datos. */
    return NULL;
}