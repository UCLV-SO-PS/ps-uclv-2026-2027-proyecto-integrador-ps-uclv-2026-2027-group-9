"""
Pruebas de integracion del servidor concurrente TCP.

Verifica los episodios oficiales, las solicitudes invalidas,
las transmisiones fragmentadas y la atencion concurrente
de conexiones independientes.

Requiere que el servidor este ejecutandose en el puerto 6666.

Proyecto Final de Programacion de Sistemas.
Curso 2026-2027. Grupo 9.
"""

import socket

HOST = "127.0.0.1"
PUERTO = 6666
TIEMPO_ESPERA = 5


def conectar():
    """Establece una conexion TCP con el servidor."""
    return socket.create_connection(
        (HOST, PUERTO),
        timeout=TIEMPO_ESPERA
    )


def recibir_respuesta(conexion):
    """Recibe la respuesta completa hasta el cierre del flujo."""
    datos = bytearray()

    while True:
        bloque = conexion.recv(512)

        if not bloque:
            break

        datos.extend(bloque)

    return datos.decode("utf-8")


def consultar(codigo):
    """Envia una solicitud y devuelve la respuesta textual."""
    with conectar() as conexion:
        conexion.sendall(codigo)
        conexion.shutdown(socket.SHUT_WR)

        return recibir_respuesta(conexion)


def probar_episodios():
    """Comprueba los datos oficiales de los seis episodios."""
    episodios = [
        ("S01E01", "The Hedge Knight", "9.2", "45120"),
        ("S01E02", "Hard Salt Beef", "8.8", "38900"),
        ("S01E03", "The Squire", "9.5", "52400"),
        ("S01E04", "Seven", "9.1", "41200"),
        ("S01E05", "In the Name of the Mother", "9.7", "68300"),
        ("S01E06", "The Morrow", "9.4", "49800")
    ]

    for codigo, titulo, puntuacion, votos in episodios:
        respuesta = consultar(codigo.encode("ascii"))

        assert f"Codigo:     {codigo}\n" in respuesta
        assert f"Titulo:     {titulo}\n" in respuesta
        assert f"Puntuacion: {puntuacion}/10\n" in respuesta
        assert f"Votos:      {votos}\n" in respuesta

        print(f"OK: consulta de {codigo}")


def probar_errores():
    """Comprueba las respuestas ante solicitudes incorrectas."""
    respuesta = consultar(b"S01E99")
    assert "ERROR: episodio no encontrado." in respuesta

    respuesta = consultar(b"s01e01")
    assert "ERROR: formato de codigo invalido." in respuesta

    respuesta = consultar(b"S01E0X")
    assert "ERROR: formato de codigo invalido." in respuesta

    respuesta = consultar(b"S01")
    assert "ERROR: codigo incompleto." in respuesta

    print("OK: tratamiento de solicitudes incorrectas")


def probar_concurrencia():
    """
    Mantiene una solicitud incompleta mientras otro cliente
    realiza una consulta independiente.

    Comprueba que el servidor no atiende las conexiones
    de manera estrictamente secuencial.
    """
    with conectar() as cliente_a:

        # El primer cliente envia solamente dos bytes.
        cliente_a.sendall(b"S0")

        # Un segundo cliente debe recibir su respuesta.
        with conectar() as cliente_b:
            cliente_b.sendall(b"S01E05")

            respuesta_b = recibir_respuesta(cliente_b)

            assert "In the Name of the Mother" in respuesta_b
            assert "9.7/10" in respuesta_b

        print("OK: segundo cliente atendido concurrentemente")

        # Completar la solicitud del primer cliente.
        cliente_a.sendall(b"1E01")

        respuesta_a = recibir_respuesta(cliente_a)

        assert "The Hedge Knight" in respuesta_a
        assert "9.2/10" in respuesta_a

        print("OK: solicitud fragmentada del primer cliente")


def main():
    """Ejecuta las pruebas de integracion disponibles."""
    probar_episodios()
    probar_errores()
    probar_concurrencia()

    print("Todas las pruebas de integracion superadas.")


if __name__ == "__main__":
    main()