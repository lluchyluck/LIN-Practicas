#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

#define SYS_ledctl 452  // Este número debe coincidir con el número que agregaste en syscall_64.tbl

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <mask>\n", argv[0]);
        return 1;
    }

    unsigned int mask = atoi(argv[1]);
   
    

    // Llamar a la syscall ledctl usando su número (SYS_ledctl)
    long result = syscall(SYS_ledctl, mask);
    if (result == -1) {
        perror("Error al invocar ledctl");
        return 1;
    }

    return 0;
}
