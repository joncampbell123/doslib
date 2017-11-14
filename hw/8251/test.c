
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <ctype.h>
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

void uart_8251_reset(struct uart_8251 *uart,unsigned char mode,unsigned char sync1,unsigned char sync2) {
    const unsigned short prt = uart_8251_portidx(uart->base_io,1/*command/mode*/);

    /* force UART into known state */
    outp(prt,0x00);
    outp(prt,0x00);
    outp(prt,0x00);

    outp(prt,0x40); /* UART reset (bit 6=1) to return to mode format */
    outp(prt,mode);
    if ((mode&3) == 0/*SYNC MODE*/) {
        /* TODO? */
    }
}

static inline void uart_8251_command(struct uart_8251 *uart,const unsigned char cmd) {
    const unsigned short prt = uart_8251_portidx(uart->base_io,1/*command/mode*/);

    /* WARNING: Assumes bit 6 == 0. If bit 6 == 1 you will reset back into mode byte */
    outp(prt,cmd);
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

void config_input(void) {
    unsigned long rate;
    unsigned char b;
    char tmp[64];
    int sbits;
    int bits;
    char p;

    if (uart->dont_touch_config) {
        printf("It is not recommended to reconfigure this UART.\n");
        printf("Hit Y to continue, anything else to cancel.\n");

        if (tolower(getch()) != 'y') return;
    }

    /* NTS: It seems to be the standard on PC-98 to use the 16X baud rate multiplier, so we do so here. */

    printf("Baud rate? "); fflush(stdout);
    tmp[0] = 0;
    fgets(tmp,sizeof(tmp),stdin);
    rate = atoi(tmp);
    if (rate < 1 || rate > 115200) return;

    printf("Stop bits? (1=1 2=1.5 3=2) "); fflush(stdout);
    tmp[0] = 0;
    fgets(tmp,sizeof(tmp),stdin);
    sbits = atoi(tmp);
    if (sbits < 1 || sbits > 3) return;

    printf("Data bits? (5 to 8) "); fflush(stdout);
    tmp[0] = 0;
    fgets(tmp,sizeof(tmp),stdin);
    bits = atoi(tmp);
    if (bits < 5 || bits > 8) return;

    printf("Parity? (E)ven (O)dd or (N)one "); fflush(stdout);
    p = tolower(getch());
    if (!(p == 'e' || p == 'o' || p == 'n')) return;

    /* construct mode byte */
    b  = (bits - 5) << 2; /* bits [3:2] = character length (0=5 .. 3=8) */
    if (p == 'e') {
        b |= 0x30;  /* bits [5:4] = 11 = even parity, enable  (bit 4 is parity enable) */
    }
    else if (p == 'o') {
        b |= 0x10;  /* bits [5:4] = 01 = odd parity, enable  (bit 4 is parity enable) */
    }
    else { /* n */
        /* bits [5:4] = 00 = parity disable */
    }

    b |= sbits << 6; /* bits [7:6] = stop bits 0=invalid 1=1-bit 2=1.5-bit 3=2-bit */
    b |= (2 << 0); /* bits [1:0] = 16X baud rate factor (baud rate is 1/16th the clock rate) */

    printf("Using mode byte 0x%02X\n",b);

    _cli();
    uart_8251_reset(uart,b,0,0);
    uart_8251_command(uart,0x15); /* error reset | receive enable | transmit enable */

    /* BAUD rate is controlled by PIT timer output 2 (COM1) */
#ifdef TARGET_PC98
    if (uart->base_io == 0x30) {
        unsigned long clk = T8254_REF_CLOCK_HZ / (rate * 16UL/*baud rate times 16*/);
	    write_8254(T8254_TIMER_RS232,clk,T8254_MODE_2_RATE_GENERATOR);
        printf("Programming PIT output 2 to set baud rate\n");
    }
#endif

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
        printf(" 1. Show raw input   2. Configure device\n");

        c = getch();
        if (c == 27) break;

        if (c == '1') {
            raw_input();
        }
        else if (c == '2') {
            config_input();
        }
    }

    return 0;
}

/* vim: set tabstop=4 softtabstop=4 shiftwidth=4 expandtab */
