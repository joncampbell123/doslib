
#ifdef __cplusplus
extern "C" {
#endif

#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdint.h>

struct uart_8251 {
    unsigned short      base_io;        /* if nonzero, exists */
    unsigned char       mode_byte;      /* if nonzero, mode byte the host programmed the UART with. There's no way to READ the mode byte. */
    unsigned int        dont_touch_config:1; /* if nonzero, don't touch baud rate, etc. */
    char*               description;    /* non-NULL if known */
    int8_t              irq;            /* IRQ associated with it, if known, or -1 */
};

#define MAX_UART_8251       4

extern struct uart_8251 uart_8251_list[MAX_UART_8251];
extern unsigned char uart_8251_listadd;

int init_8251();
void probe_common_8251();
void uart_8251_reset(struct uart_8251 *uart,unsigned char mode,unsigned char sync1,unsigned char sync2);

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

static inline unsigned char uart_8251_status(struct uart_8251 *uart) {
    return inp(uart_8251_portidx(uart->base_io,1/*status/mode/command*/));
}

static inline unsigned char uart_8251_rxready(struct uart_8251 *uart) {
    return (uart_8251_status(uart) & 0x02/*RxRDY*/);
}

static inline unsigned char uart_8251_read(struct uart_8251 *uart) {
    return inp(uart_8251_portidx(uart->base_io,0/*data*/));
}

static inline unsigned char uart_8251_txready(struct uart_8251 *uart) {
    return (uart_8251_status(uart) & 0x01/*TxRDY*/);
}

static inline unsigned char uart_8251_write(struct uart_8251 *uart,unsigned char c) {
    return outp(uart_8251_portidx(uart->base_io,0/*data*/),c);
}

static inline void uart_8251_command(struct uart_8251 *uart,const unsigned char cmd) {
    const unsigned short prt = uart_8251_portidx(uart->base_io,1/*command/mode*/);

    /* WARNING: Assumes bit 6 == 0. If bit 6 == 1 you will reset back into mode byte */
    outp(prt,cmd);
}

#ifdef __cplusplus
}
#endif

/* vim: set tabstop=4 softtabstop=4 shiftwidth=4 expandtab */
