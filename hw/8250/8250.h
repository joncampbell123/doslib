/* 8250.h
 *
 * 8250/16450/16550/16750 serial port UART library.
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box] */
 
/* WARNING: As usual for performance reasons this library generally does not
 *          enable/disable interrupts (cli/sti). To avoid contention with
 *          interrupt handlers the calling program should do that. */

#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */

#include <hw/cpu/cpu.h>
#include <stdint.h>

#define MAX_8250_PORTS				8

#define STANDARD_8250_PORT_COUNT		4

#define PORT_8250_IO				0
#define PORT_8250_IER				1
#define PORT_8250_IIR				2
#define PORT_8250_FCR				2
#define PORT_8250_LCR				3
#define PORT_8250_MCR				4
#define PORT_8250_LSR				5
#define PORT_8250_MSR				6
#define PORT_8250_SCRATCH			7
#define PORT_8250_DIV_LO			0
#define PORT_8250_DIV_HI			1

/* Line control register bits */
#define UART_8250_LCR_8BIT			(3 << 0)
#define UART_8250_LCR_PARITY			(1 << 3)
#define UART_8250_LCR_PARITY_EVEN		(1 << 4)
/* set this bit to make register offsets 0&1 the divisor latch */
#define UART_8250_LCR_DLAB			(1 << 7)

/* FIFO control register bits */
#define UART_8250_FCR_FIFO_ENABLE		(1 << 0)
#define UART_8250_FCR_RCV_FIFO_RESET		(1 << 1)
#define UART_8250_FCR_XMIT_FIFO_RESET		(1 << 2)
#define UART_8250_FCR_DMA_MODE_SELECT		(1 << 3)

/* 16750 only */
#define UART_8250_FCR_64BYTE_FIFO		(1 << 5)

#define UART_8250_FCR_RCV_THRESHHOLD_MASK	(3 << 6)
#define UART_8250_FCR_RCV_THRESHHOLD_SHIFT	(6)
/* known threshholds (non-64 byte mode)
    0 = 1 byte
    1 = 4 bytes
    2 = 8 bytes
    3 = 14 bytes
   known threshholds (64-byte mode)
    0 = 1 byte
    1 = 16 bytes
    2 = 32 bytes
    3 = 56 bytes */

enum {
	TYPE_8250_IS_8250=0,
	TYPE_8250_IS_16450,
	TYPE_8250_IS_16550,
	TYPE_8250_IS_16550A,
	TYPE_8250_IS_16750,
	TYPE_8250_MAX
};

struct info_8250 {
	uint8_t		type;
	uint16_t	port;
	int8_t		irq;
};

extern const char*		type_8250_strs[TYPE_8250_MAX];
extern const uint16_t		standard_8250_ports[STANDARD_8250_PORT_COUNT];
extern uint16_t			base_8250_port[MAX_8250_PORTS];
extern struct info_8250		info_8250_port[MAX_8250_PORTS];
extern unsigned int		base_8250_ports;
extern unsigned char		bios_8250_ports;
extern char			use_8250_int;
extern char			inited_8250;

#define type_8250_to_str(x)	type_8250_strs[x]
#define base_8250_full()	(base_8250_ports >= MAX_8250_PORTS)

static inline void uart_8250_set_MCR(struct info_8250 *uart,uint8_t mcr) {
	outp(uart->port+PORT_8250_MCR,mcr);
}

static inline uint8_t uart_8250_read_MCR(struct info_8250 *uart) {
	return inp(uart->port+PORT_8250_MCR);
}

static inline uint8_t uart_8250_read_MSR(struct info_8250 *uart) {
	return inp(uart->port+PORT_8250_MSR);
}

static inline void uart_8250_set_line_control(struct info_8250 *uart,uint8_t lcr) {
	outp(uart->port+PORT_8250_LCR,lcr);
}

/* NTS: This function is only slightly affected by the FIFO functions, FCR bit 3 (so called "DMA" mode).
 *      When "mode 0" FCR bit 3 == 0, this will signal (1) when any amount of data is waiting in the FIFO.
 *      When "mode 1" FCR bit 3 == 1, this will signal (1) when the number of bytes in the FIFO exceeds a threshhold, or a timeout has occured */
static inline int uart_8250_can_read(struct info_8250 *uart) {
	return (inp(uart->port+PORT_8250_LSR) & (1 << 0)) ? 1 : 0; /*if LSR says receive buffer is ready, then yes we can read */
}

/* WARNING: The caller is expected to call uart_8250_can_read() first before calling this function */
static inline uint8_t uart_8250_read(struct info_8250 *uart) {
	return inp(uart->port+PORT_8250_IO);
}

/* NTS: This function is only slightly affected by FIFO FCR bit 3 (so called "DMA" mode).
 *      When "mode 0" FCR bit 3 == 0, this will signal (1) when the FIFO is empty or (on non-FIFO UARTs) the "holding register" is ready to accept another byte.
 *      That does mean though that following the traditional transmission model you will only maintain at most 1 byte in the XMIT FIFO, which is kind of wasteful.
 *      When "mode 1" FCR bit 3 == 1, this will signal (1) when there is room in the FIFO to write another data byte. This function returns 0 when the XMIT FIFO is full */
static inline int uart_8250_can_write(struct info_8250 *uart) {
	return (inp(uart->port+PORT_8250_LSR) & (1 << 5)) ? 1 : 0; /*if LSR says transmit buffer is empty, then yes we can write */
}

/* WARNING: The caller is expected to call uart_8250_can_write() first before calling this function */
static inline void uart_8250_write(struct info_8250 *uart,uint8_t c) {
	outp(uart->port+PORT_8250_IO,c);
}

int init_8250();
int probe_8250(uint16_t port);
int add_pnp_8250(uint16_t port,int irq);
int already_got_8250_port(uint16_t port);
uint16_t get_8250_bios_port(unsigned int index);
void uart_toggle_xmit_ien(struct info_8250 *uart);
const char *type_8250_parity(unsigned char parity);
void uart_8250_disable_FIFO(struct info_8250 *uart);
void uart_8250_set_FIFO(struct info_8250 *uart,uint8_t flags);
void uart_8250_set_baudrate(struct info_8250 *uart,uint16_t dlab);
void uart_8250_enable_interrupt(struct info_8250 *uart,uint8_t mask);
uint16_t uart_8250_baud_to_divisor(struct info_8250 *uart,unsigned long rate);
unsigned long uart_8250_divisor_to_baud(struct info_8250 *uart,uint16_t rate);
void uart_8250_get_config(struct info_8250 *uart,unsigned long *baud,unsigned char *bits,unsigned char *stop_bits,unsigned char *parity);

