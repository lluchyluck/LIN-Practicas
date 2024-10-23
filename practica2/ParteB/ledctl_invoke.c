#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define MIN_MASK 0
#define MAX_MASK 7

// Prototipo de la llamada al sistema ledctl
extern long ledctl(unsigned int leds);

int main(int argc, char *argv[]) {
    unsigned int mask;
    long result;

    // Comprobar que el usuario ha pasado un argumento
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <ledmask>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Convertir el argumento a un número
    mask = atoi(argv[1]);

    // Verificar que está en el rango válido (0-7)
    if (mask < MIN_MASK || mask > MAX_MASK) {
        fprintf(stderr, "Error: ledmask must be in the range 0-7.\n");
        exit(EXIT_FAILURE);
    }

    // Invocar la llamada al sistema
    result = ledctl(mask);

    // Manejo de errores
    if (result == -1) {
        perror("ledctl failed");
        exit(EXIT_FAILURE);
    }

    printf("LED state changed successfully.\n");
    return 0;
}
