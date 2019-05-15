
#if !defined(TARGET_PC98)

#include <hw/8250/8250.h>

uint16_t uart_8250_baud_to_divisor(struct info_8250 *uart,unsigned long rate) {
    if (rate == 0) return 0;
    return (uint16_t)(115200UL / rate);
}

unsigned long uart_8250_divisor_to_baud(struct info_8250 *uart,uint16_t rate) {
    if (rate == 0) return 1;
    return (unsigned long)(115200UL / (unsigned long)rate);
}

#endif //!defined(TARGET_PC98)

/* vim: set tabstop=4 softtabstop=4 shiftwidth=4 expandtab */
