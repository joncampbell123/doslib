
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>
#include <hw/8251/8251.h>
#include <hw/dos/doswin.h>

int main() {
    printf("8251 test program\n");

    /* WARNING: For PC-98 only. Do not run on IBM PC hardware */

    cpu_probe();        /* ..for the DOS probe routine */
    probe_dos();        /* ..for the Windows detection code */
    detect_windows();   /* Windows virtualizes the COM ports, and we don't want probing to occur to avoid any disruption */

    if (!probe_8254()) {
        printf("8254 not found (I need this for time-sensitive portions of the driver)\n");
        return 1;
    }

    if (!probe_8259()) {
        printf("8259 not found (I need this for portions of the test involving serial interrupts)\n");
        return 1;
    }

    if (!init_8251()) {
        printf("Cannot init 8250 library\n");
        return 1;
    }

    return 0;
}

/* vim: set tabstop=4 softtabstop=4 shiftwidth=4 expandtab */
