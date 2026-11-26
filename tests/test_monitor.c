/**
 * @file test_monitor.c
 * @brief Pruebas unitarias del modulo monitor.
 *
 * Verifica la integridad de los datos oficiales, la validacion
 * de codigos y la busqueda de episodios existentes e inexistentes.
 *
 * Proyecto Final de Programacion de Sistemas.
 * Curso 2026-2027. Grupo 9.
 */

#include "monitor.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Ejecuta las pruebas unitarias del modulo monitor.
 *
 * Las aserciones comprueban los resultados esperados.
 * Si alguna condicion no se cumple, la prueba se interrumpe.
 *
 * @return 0 si todas las pruebas finalizan correctamente.
 */
int main(void)
{
    /* Datos esperados segun la orientacion oficial. */
    const Episodio esperados[] = {
        {"S01E01", "The Hedge Knight",         9.2, 45120},
        {"S01E02", "Hard Salt Beef",           8.8, 38900},
        {"S01E03", "The Squire",               9.5, 52400},
        {"S01E04", "Seven",                    9.1, 41200},
        {"S01E05", "In the Name of the Mother", 9.7, 68300},
        {"S01E06", "The Morrow",               9.4, 49800}
    };

    /* Verificar la cantidad total de episodios. */
    assert(cantidad_episodios() ==
           sizeof esperados / sizeof esperados[0]);

    /* Comprobar individualmente todos los registros. */
    for (size_t i = 0; i < cantidad_episodios(); ++i) {
        assert(codigo_valido(esperados[i].codigo));

        const Episodio *e =
            buscar_episodio(esperados[i].codigo);

        assert(e != NULL);
        assert(strcmp(e->codigo, esperados[i].codigo) == 0);
        assert(strcmp(e->titulo, esperados[i].titulo) == 0);
        assert(e->puntuacion == esperados[i].puntuacion);
        assert(e->votos == esperados[i].votos);

        printf("OK: %s\n", e->codigo);
    }

    /* Casos de entrada con formato incorrecto. */
    const char *invalidos[] = {
        NULL,
        "",
        "S01E0",
        "S01E001",
        "s01e01",
        "S0AE01",
        "S01E0X",
        "S01E01\n",
        "S01E 1"
    };

    /* Verificar el rechazo de todos los codigos invalidos. */
    for (size_t i = 0;
         i < sizeof invalidos / sizeof invalidos[0];
         ++i) {
        assert(!codigo_valido(invalidos[i]));
        assert(buscar_episodio(invalidos[i]) == NULL);
    }

    /* Codigo sintacticamente valido, pero inexistente. */
    assert(codigo_valido("S01E99"));
    assert(buscar_episodio("S01E99") == NULL);

    puts("OK: codigos invalidos e inexistentes");
    puts("Todas las pruebas superadas.");

    return 0;
}