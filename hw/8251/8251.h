
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdint.h>

struct uart_8251 {
    unsigned short      base_io;        /* if nonzero, exists */
    unsigned char       mode_byte;      /* if nonzero, mode byte the host programmed the UART with. There's no way to READ the mode byte. */
    unsigned int        dont_touch_config:1; /* if nonzero, don't touch baud rate, etc. */
    char*               description;    /* non-NULL if known */
};

#define MAX_UART_8251       4

extern struct uart_8251 uart_8251_list[MAX_UART_8251];
extern unsigned char uart_8251_listadd;

int init_8251();
void probe_common_8251();

/* base_io + idx */
static inline unsigned short uart_8251_portidx(unsigned short base,unsigned char idx) {
#ifdef TARGET_PC98
    return base + (idx * 2U); /* all I/O ports are even or odd, 2 apart */
#else
    return base + idx; /* in the event that an 8251 is on an IBM PC, they would probably be 1 apart */
#endif
}

static inline unsigned char uart_8251_total(void) {
    return uart_8251_listadd;
}

static inline struct uart_8251 *uart_8251_get(unsigned char idx) {
    if (idx >= uart_8251_listadd)
        return NULL;
    if (uart_8251_list[idx].base_io == 0)
        return NULL;

    return &uart_8251_list[idx];
}

/* vim: set tabstop=4 softtabstop=4 shiftwidth=4 expandtab */
