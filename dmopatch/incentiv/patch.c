
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>

#ifndef O_BINARY
#define O_BINARY (0)
#endif

int openrw_file(const char *path) {
    int fd;

    fd = open(path,O_RDWR|O_BINARY);
    if (fd < 0) {
        fprintf(stderr,"Unable to open for rw, file %s\n",path);
        return -1;
    }

    return fd;
}

int main() {
    int fd;

    printf("DOSLIB/DOSBox-X patch for DID INCENTIV (the party 1994)\n");
    printf("This patch fixes:\n");
    printf(" - Random I/O access while playing music on Gravis Ultrasound.\n");

    /* ======================================= INCENTIV.EXE ================================ */
    if ((fd=openrw_file("INCENTIV.EXE")) < 0) return 1;

    close(fd);
    /* ===================================================================================== */

    return 0;
}

