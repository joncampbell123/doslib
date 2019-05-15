
#if !defined(TARGET_PC98)

#include <hw/8250/8250.h>

void uart_8250_set_baudrate(struct info_8250 *uart,uint16_t dlab) {
    uint8_t c;

    /* enable access to the divisor */
    c = inp(uart->port+PORT_8250_LCR);
    outp(uart->port+PORT_8250_LCR,c | 0x80);
    /* set rate */
    outp(uart->port+PORT_8250_DIV_LO,dlab);
    outp(uart->port+PORT_8250_DIV_HI,dlab >> 8);
    /* disable access to the divisor */
    outp(uart->port+PORT_8250_LCR,c & 0x7F);
}

#endif //!defined(TARGET_PC98)

/* vim: set tabstop=4 softtabstop=4 shiftwidth=4 expandtab */
