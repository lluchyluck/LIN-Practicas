#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

int main() {
    const char characters[] = "0123456789abcdefABCDEF";
    const size_t num_chars = sizeof(characters) - 1; // Exclude null terminator
    const char *device_path = "/dev/display7s";
    char buffer[3]; // Espacio para carácter, '\n' y '\0'

    // Abre el dispositivo para escritura
    int fd = open(device_path, O_WRONLY);
    if (fd == -1) {
        perror("Error al abrir el dispositivo");
        return EXIT_FAILURE;
    }

    while (1) { // Bucle infinito
        for (size_t i = 0; i < num_chars; i++) {
            // Prepara el buffer con el carácter seguido de '\n'
            snprintf(buffer, sizeof(buffer), "%c\n", characters[i]);

            // Escribe el carácter en el dispositivo
            if (write(fd, buffer, strlen(buffer)) == -1) {
                perror("Error al escribir en el dispositivo");
                close(fd);
                return EXIT_FAILURE;
            }
            usleep(400000); // Pausa de 0.4 segundos
        }
    }

    // Cierra el dispositivo (nunca se alcanza en este código)
    close(fd);
    return EXIT_SUCCESS;
}
