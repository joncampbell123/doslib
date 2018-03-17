
#include <string.h>

#include <hw/8251/8251.h>

unsigned char inited_8251 = 0;
unsigned char uart_8251_listadd;
struct uart_8251 uart_8251_list[MAX_UART_8251];

void check_8251(unsigned short baseio) {
    unsigned char a,b;

    a = inp(baseio);
#ifdef TARGET_PC98
    b = inp(baseio+2);
#else
    b = inp(baseio+1);
#endif

    /* if both are 0xFF or both are 0x00 then nothing is there */
    if ((a&b) == 0xFF || (a|b) == 0x00)
        return;

    if (uart_8251_listadd >= MAX_UART_8251)
        return;

    uart_8251_list[uart_8251_listadd].base_io = baseio;
    uart_8251_list[uart_8251_listadd].mode_byte = 0x00; // unknown
    uart_8251_list[uart_8251_listadd].irq = -1; // unknown

#ifdef TARGET_PC98
    if (baseio == 0x41/*keyboard*/) {
        uart_8251_list[uart_8251_listadd].dont_touch_config = 1; /* probably not wise to play with 8251 mode of the keyboard */
        uart_8251_list[uart_8251_listadd].description = "PC-98 keyboard UART";
        uart_8251_list[uart_8251_listadd].irq = 1;
    }
    else if (baseio == 0x30/*RS-232 COM1*/) {
        uart_8251_list[uart_8251_listadd].description = "COM1 RS-232C UART";
        uart_8251_list[uart_8251_listadd].irq = 4;
    }
#endif

    uart_8251_listadd++;
}

int init_8251() {
    if (!inited_8251) {
        memset(&uart_8251_list,0,sizeof(uart_8251_list));
        uart_8251_listadd = 0;
        inited_8251 = 1;
    }

    return 1;
}

void probe_common_8251() {
#ifdef TARGET_PC98
    check_8251(0x41);       /* 0x41-0x43 NEC PC-98 keyboard */
    check_8251(0x30);       /* 0x30-0x32 NEC PC-98 COM1 RS-232C port */
#endif
}

/* vim: set tabstop=4 softtabstop=4 shiftwidth=4 expandtab */
