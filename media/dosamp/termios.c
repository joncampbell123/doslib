
#include <stdio.h>
#include <stdint.h>
#ifdef LINUX
#include <endian.h>
#else
#include <hw/cpu/endian.h>
#endif
#ifndef LINUX
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <direct.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <malloc.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#ifndef LINUX
#include <dos.h>
#endif

#ifndef LINUX
#include <hw/dos/dos.h>
#include <hw/cpu/cpu.h>
#include <hw/8237/8237.h>       /* 8237 DMA */
#include <hw/8254/8254.h>       /* 8254 timer */
#include <hw/8259/8259.h>       /* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/cpu/cpurdtsc.h>
#include <hw/dos/doswin.h>
#include <hw/dos/tgusmega.h>
#include <hw/dos/tgussbos.h>
#include <hw/dos/tgusumid.h>
#include <hw/isapnp/isapnp.h>
#include <hw/sndsb/sndsbpnp.h>
#endif

#include "wavefmt.h"
#include "dosamp.h"
#include "timesrc.h"
#include "dosptrnm.h"
#include "filesrc.h"
#include "resample.h"
#include "cvrdbuf.h"
#include "cvip.h"
#include "trkrbase.h"
#include "tmpbuf.h"
#include "snirq.h"
#include "sndcard.h"
#include "termios.h"

/* TODO: Move to another file eventually. */
#if defined(LINUX)
#include <termios.h>
#include <poll.h>

/* Linux does not provide getch() and kbhit().
 * We must provide our own using termios() functions. */

static struct termios termios_prev,termios_cur;
static unsigned char termios_exhook = 0;
static unsigned char termios_init = 0;

int free_termios(void);

void atexit_termios(void) {
    free_termios();
}

int init_termios(void) {
    if (!termios_init) {
        tcgetattr(0/*STDIN*/,&termios_prev);

        termios_cur = termios_prev;

        termios_cur.c_iflag &= ~(IGNBRK|IGNCR|ICRNL|INLCR);
        termios_cur.c_iflag |=  (BRKINT);

        termios_cur.c_oflag &= ~(OCRNL);
        termios_cur.c_oflag |=  (ONLCR);

        termios_cur.c_lflag &= ~(ICANON|XCASE|ECHO|ECHOE|ECHOK|ECHONL|ECHOCTL|ECHOPRT|ECHOKE);

        tcsetattr(0/*STDIN*/,TCSADRAIN,&termios_cur);

        if (!termios_exhook) {
            atexit(atexit_termios);
            termios_exhook = 1;
        }

        termios_init = 1;
    }

    return 0;
}

int free_termios(void) {
    if (termios_init) {
        tcsetattr(0/*STDIN*/,TCSADRAIN,&termios_prev);
        termios_init = 0;
    }

    return 0;
}

int getch(void) {
    char c;

    if (read(0/*STDIN*/,&c,1) != 1)
        return -1;

    return (int)((unsigned char)c);
}

int kbhit(void) {
    struct pollfd pfd = {0,0,0};

    pfd.events = POLLIN;
    pfd.fd = 0/*STDIN*/;
    if (poll(&pfd,1,0) > 0) {
        if ((pfd.revents & POLLIN) != 0)
            return 1;
    }

    return 0;
}

#endif /* LINUX */

