#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

int main() {
    int fd;
    char c;
    
    // Abrir el dispositivo /dev/display7s
    fd = open("/dev/display7s", O_WRONLY);
    if (fd == -1) {
        perror("Error al abrir el dispositivo");
        exit(EXIT_FAILURE);
    }

    // Bucle infinito
    while (1) {
        // Enviar los caracteres del 0 al 9, a-f, A-F
        for (c = '0'; c <= '9'; c++) {
            write(fd, &c, 1);
            usleep(400000);  // 0.4 segundos
        }
        
        for (c = 'a'; c <= 'f'; c++) {
            write(fd, &c, 1);
            usleep(400000);  // 0.4 segundos
        }
        
        for (c = 'A'; c <= 'F'; c++) {
            write(fd, &c, 1);
            usleep(400000);  // 0.4 segundos
        }
    }

    // Cerrar el dispositivo
    close(fd);
    return 0;
}
