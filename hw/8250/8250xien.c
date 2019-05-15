
#if !defined(TARGET_PC98)

#include <hw/8250/8250.h>

void uart_toggle_xmit_ien(struct info_8250 *uart) {
    /* apparently if the XMIT buffer is empty, you can trigger
     * another TX empty interrupt event by toggling bit 2 in
     * the IER register. */
    unsigned char c = inp(uart->port+PORT_8250_IER);
    outp(uart->port+PORT_8250_IER,c & ~(1 << 2));
    outp(uart->port+PORT_8250_IER,c);
}

#endif //!defined(TARGET_PC98)

/* vim: set tabstop=4 softtabstop=4 shiftwidth=4 expandtab */
