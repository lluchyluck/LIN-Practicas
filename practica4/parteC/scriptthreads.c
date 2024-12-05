#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define NUM_THREADS 2

void *write_to_device(void *arg) {
    const char characters[] = "0123456789abcdefABCDEF";
    const size_t num_chars = sizeof(characters) - 1; // Exclude null terminator
    const char *device_path = "/dev/display7s";
    char buffer[3]; // Espacio para carácter, '\n' y '\0'
    int thread_id = *(int *)arg;

    printf("Hilo %d: Iniciando escritura en %s\n", thread_id, device_path);

    // Abre el dispositivo para escritura
    int fd = open(device_path, O_WRONLY);
    if (fd == -1) {
        perror("Hilo %d: Error al abrir el dispositivo");
        pthread_exit(NULL);
    }

    printf("Hilo %d: Dispositivo abierto correctamente\n", thread_id);

    while (1) { // Bucle infinito
        for (size_t i = 0; i < num_chars; i++) {
            // Prepara el buffer con el carácter seguido de '\n'
            snprintf(buffer, sizeof(buffer), "%c\n", characters[i]);

            // Escribe el carácter en el dispositivo
            if (write(fd, buffer, strlen(buffer)) == -1) {
                perror("Hilo %d: Error al escribir en el dispositivo");
                close(fd);
                pthread_exit(NULL);
            }

            printf("Hilo %d: Escrito '%c' en el dispositivo\n", thread_id, characters[i]);
            usleep(400000); // Pausa de 0.4 segundos
        }
    }

    // Cierra el dispositivo (nunca se alcanza en este código)
    close(fd);
    printf("Hilo %d: Cerrando dispositivo\n", thread_id);
    pthread_exit(NULL);
}

int main() {
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];

    printf("Creando %d hilos...\n", NUM_THREADS);

    // Crear hilos
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i + 1; // Asignar ID de hilo para depuración
        if (pthread_create(&threads[i], NULL, write_to_device, &thread_ids[i]) != 0) {
            perror("Error al crear el hilo");
            return EXIT_FAILURE;
        }
        printf("Hilo %d creado\n", thread_ids[i]);
    }

    // Esperar a que los hilos terminen (aunque en este caso no terminan)
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        printf("Hilo %d finalizado\n", thread_ids[i]);
    }

    return EXIT_SUCCESS;
}
