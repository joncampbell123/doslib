
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/doswin.h>

int main(int argc,char **argv) {
    uint32_t res;
    uint16_t port;
    char sz = 'b';

	cpu_probe();

    if (argc < 2) {
        fprintf(stderr,"INP <port>[bwl]\n");
        return 1;
    }

    {
        char *s = argv[1];
        port = (uint16_t)strtoul(s,&s,0);
        if (*s == 'b' || *s == 'w' || *s == 'l') sz = *s++;
    }

    if (sz == 'l') {
        res = (uint32_t)inpd(port);
        printf("%08lx\n",(unsigned long)res);
    }
    else if (sz == 'w') {
        res = (uint32_t)inpw(port);
        printf("%04lx\n",(unsigned long)res);
    }
    else { /* 'b' */
        res = (uint32_t)inp(port);
        printf("%02lx\n",(unsigned long)res);
    }

	return 0;
}

