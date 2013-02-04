/* WARNING: As usual for performance reasons this library generally does not
 *          enable/disable interrupts (cli/sti). To avoid contention with
 *          interrupt handlers the calling program should do that. */

#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */

#include <hw/cpu/cpu.h>
#include <stdint.h>

/* parallel port types */
enum {
	PARPORT_STANDARD=0,
	PARPORT_BIDIRECTIONAL,
	PARPORT_ECP,
	PARPORT_EPP,
	PARPORT_ECP_AND_EPP,
	PARPORT_MAX
};

/* modes modes */
enum {
	PARPORT_MODE_STANDARD_STROBE_ACK=0,	/* not on busy, pulse, wait for ack */
	PARPORT_MODE_STANDARD_STROBE,		/* not on busy, pulse, don't wait for ack */
	PARPORT_MODE_FIFO,			/* switch into FIFO mode, let h/w do handshaking */
	PARPORT_MODE_ECP,			/* switch to ECP mode */
	PARPORT_MODE_EPP			/* switch to EPP mode */
};

#define MAX_PARPORTS		4

#define PARPORT_SUPPORTS_EPP(x)		((x) == PARPORT_EPP || (x) == PARPORT_ECP_AND_EPP)
#define PARPORT_SUPPORTS_ECP(x)		((x) == PARPORT_ECP || (x) == PARPORT_ECP_AND_EPP)

#define PARPORT_IO_DATA			0
#define PARPORT_IO_STATUS		1
#define PARPORT_IO_CONTROL		2
#define PARPORT_IO_EPP_ADDRESS		3
#define PARPORT_IO_EPP_DATA		4

#define PARPORT_IO_ECP_REG_A		0x400
#define PARPORT_IO_ECP_REG_B		0x401
#define PARPORT_IO_ECP_CONTROL		0x402

#define PARPORT_STATUS_nBUSY		(1 << 7)
#define PARPORT_STATUS_nACK		(1 << 6)
#define PARPORT_STATUS_PAPER_OUT	(1 << 5)
#define PARPORT_STATUS_SELECT		(1 << 4)
#define PARPORT_STATUS_nERROR		(1 << 3)

#define PARPORT_CTL_ENABLE_BIDIR	(1 << 5)
#define PARPORT_CTL_ENABLE_IRQ		(1 << 4)
#define PARPORT_CTL_SELECT_PRINTER	(1 << 3)
#define PARPORT_CTL_nINIT		(1 << 2)
#define PARPORT_CTL_LINEFEED		(1 << 1)
#define PARPORT_CTL_STROBE		(1 << 0)

#define STANDARD_PARPORT_PORTS		3

struct info_parport {
	uint16_t		port;
	/* 8 bits { */
	uint8_t			type:4;
	uint8_t			bit10:1;
	uint8_t			max_xfer_size:3;	/* in bytes */
	/* } */
	int8_t			irq;
	int8_t			dma;
	/* mode */
	uint8_t			output_mode:3;
	uint8_t			reserved:5;
};

extern const char *			parport_type_str[PARPORT_MAX];
extern int				init_parports;
extern int				bios_parports;
extern uint16_t				standard_parport_ports[STANDARD_PARPORT_PORTS];
extern struct info_parport		info_parport[MAX_PARPORTS];
extern int				info_parports;

uint16_t get_bios_parport(unsigned int index);
int already_got_parport(uint16_t port);
int probe_parport(uint16_t port);
int add_pnp_parport(uint16_t port,int irq,int dma,int type);
int init_parport();

