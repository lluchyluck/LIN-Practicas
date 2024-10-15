#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/syscall.h>

#include <string.h>

#define BUFSIZE 512

#ifndef SYS_read
#define SYS_read 0
#endif

int main(void)
{
    int read_chars = 0;
    int fd = 0;
    char buf[BUFSIZE + 1];

    /* Open /proc entry in read-only mode */
    // fd=open("/proc/cpuinfo",O_RDONLY);
    fd = syscall(SYS_open, "/proc/cpuinfo", O_RDONLY);
    if (fd < 0)
    {
        const char *error_message = "Can't open the file\n";
        syscall(SYS_write, STDERR_FILENO, error_message, strlen(error_message) - 1);
        // fprintf(stderr,"Can't open the file\n");
        exit(1);
    }

    /* Loop that reads data from the file and prints its contents to stdout */
    while ((read_chars = syscall(SYS_read, fd, buf, BUFSIZE)) > 0)
    {
        buf[read_chars] = '\0';
        //printf("%s", buf);
        syscall(SYS_write, STDOUT_FILENO, buf, read_chars);
    }

    if (read_chars < 0)
    {
        //fprintf(stderr, "Error while reading the file\n");
        const char *error_message = "Error while reading the file\n";
        syscall(SYS_write, STDERR_FILENO, error_message, strlen(error_message) - 1);
        exit(1);
    }

    /* Close the file and exit */
    syscall(SYS_close, fd);
    return 0;
}
