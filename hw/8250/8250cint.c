 
#include <hw/8250/8250.h>

void uart_8250_enable_interrupt(struct info_8250 *uart,uint8_t mask) {
    uint8_t c;

    outp(uart->port+PORT_8250_LCR,inp(uart->port+PORT_8250_LCR) & 0x7F);

    /* the mask is written as-is to the IER. we assume the DLAB latch == 0 */
    outp(uart->port+PORT_8250_IER,mask);

    /* on PC platforms, we also have to diddle with the AUX 2 line (FIXME: why?) */
    c = inp(uart->port+PORT_8250_MCR);
    if (mask != 0) c |= 8;  /* AUX 2 output line */
    else           c &= ~8;
    outp(uart->port+PORT_8250_MCR,c);
}

/* vim: set tabstop=4 softtabstop=4 shiftwidth=4 expandtab */
