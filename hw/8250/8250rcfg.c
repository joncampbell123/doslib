
#if !defined(TARGET_PC98)

#include <hw/8250/8250.h>

void uart_8250_get_config(struct info_8250 *uart,unsigned long *baud,unsigned char *bits,unsigned char *stop_bits,unsigned char *parity) {
    uint16_t dlab;
    uint8_t c = inp(uart->port+PORT_8250_LCR);
    *bits = (c & 3) + 5;
    *stop_bits = (c & 4) ? 2 : 1;
    *parity = (c >> 3) & 7;

    /* then switch on DLAB to get divisor */
    outp(uart->port+PORT_8250_LCR,c | 0x80);

    /* read back divisor */
    dlab = inp(uart->port+PORT_8250_DIV_LO);
    dlab |= (uint16_t)inp(uart->port+PORT_8250_DIV_HI) << 8;

    /* then switch off DLAB */
    outp(uart->port+PORT_8250_LCR,c & 0x7F);

    *baud = uart_8250_divisor_to_baud(uart,dlab);
}

#endif //!defined(TARGET_PC98)

/* vim: set tabstop=4 softtabstop=4 shiftwidth=4 expandtab */
