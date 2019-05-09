
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
    uint32_t data;
    uint16_t port;
    char sz = 'b';

	cpu_probe();

    if (argc < 3) {
        fprintf(stderr,"OUTP <port>[bwl] <data>\n");
        return 1;
    }

    data = (uint32_t)strtoul(argv[2],NULL,0);

    {
        char *s = argv[1];
        port = (uint16_t)strtoul(s,&s,0);
        if (*s == 'b' || *s == 'w' || *s == 'l') sz = *s++;
    }

    if (sz == 'l') {
        outpd(port,data);
    }
    else if (sz == 'w') {
        outpw(port,data);
    }
    else { /* 'b' */
        outp(port,data);
    }

	return 0;
}

