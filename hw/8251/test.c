
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

struct uart_8251 *uart = NULL;

// TODO MOVE
static inline unsigned char uart_8251_status(struct uart_8251 *uart) {
    return inp(uart_8251_portidx(uart->base_io,1/*status/mode/command*/));
}

static inline unsigned char uart_8251_rxready(struct uart_8251 *uart) {
    return (uart_8251_status(uart) & 0x02/*RxRDY*/);
}

static inline unsigned char uart_8251_read(struct uart_8251 *uart) {
    return inp(uart_8251_portidx(uart->base_io,0/*data*/));
}
// END

void raw_input(void) {
    unsigned long countdown_init = T8254_REF_CLOCK_HZ * 2UL;
    unsigned long countdown = countdown_init;
    unsigned short c,pc;

    c = pc = read_8254(T8254_TIMER_INTERRUPT_TICK);

    _cli();
    while (1) {
        if (uart->irq >= 0)
    		p8259_OCW2(uart->irq,P8259_OCW2_SPECIFIC_EOI | (uart->irq & 7));

        if (uart_8251_rxready(uart)) {
            unsigned char c = uart_8251_read(uart);

            countdown = countdown_init;

            printf("0x%02X ",c);
            fflush(stdout);
        }
        else {
            pc = c;
            c = read_8254(T8254_TIMER_INTERRUPT_TICK);

            countdown -= ((pc - c) & 0xFFFF); /* counts DOWN */
            if ((signed long)countdown <= 0L) break; /* timeout */
        }
    }
    _sti();
}

int main() {
    unsigned int i;
    int c;

    printf("8251 test program\n");

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

    probe_common_8251();

    if (uart_8251_total() == 0) {
        printf("No 8251 chips found\n");
        return 1;
    }

    printf("Pick a UART to play with.\n");
    for (i=0;i < uart_8251_total();i++) {
        struct uart_8251 *c_uart = uart_8251_get(i);
        if (c_uart == NULL) continue;

        printf("  %u: UART at 0x%02X,0x%02X '%s'\n",
            i,uart_8251_portidx(c_uart->base_io,0),
            uart_8251_portidx(c_uart->base_io,1),
            c_uart->description ? c_uart->description : "");
    }

    /* countdown time code relies on it */
    write_8254_system_timer(T8254_TIMER_INTERRUPT_TICK);

    printf("Choice? "); fflush(stdout);
    c = getch() - '0';
    printf("\n");
    if (c < 0 || c >= uart_8251_total()) return 0;

    uart = uart_8251_get(c);
    printf("You chose '%s' at 0x%02X\n",uart->description ? uart->description : "",uart->base_io);

    while (1) {
        printf("What do you want to do?\n");
        printf(" 1. Show raw input\n");

        c = getch();
        if (c == 27) break;

        if (c == '1') {
            raw_input();
        }
    }

    return 0;
}

/* vim: set tabstop=4 softtabstop=4 shiftwidth=4 expandtab */
