
#include <string.h>

#include <hw/8251/8251.h>

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

