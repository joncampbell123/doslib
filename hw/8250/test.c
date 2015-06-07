/* test.c
 *
 * 8250/16450/16550/16750 serial port UART test program.
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box] */

/* TODO: - Can you transform the IRQ buffering code into some kind of C object that the program can allocate as needed (including what size buffer) so that
 *         such IRQ enabled code is easily usable in other programming projects?
 *       - See how well this works on that Saber laptop where the trackball is a serial device on COM2.
 *       - Add code to show you the state of the line and modem status registers, and twiddle them too.
 *       - Does this program actually work on... say... the IBM PC/XT you have sitting in the corner?
 *       - How about that ancient 286 laptop you have? The 386 one? The Compaq elite? */

/* Warnings regarding this code: The order of operations involved in enabling interrupts is
 * very important! The 8250/16450/16550A/etc UARTs in PC hardware are just so goddamn finicky
 * about getting interrupts to fire like they're supposed to that if you change the order of
 * I/O port operations (even if to a more intuitive order) you will probably break UART
 * interrupt support on many configurations.
 *
 * I must explain why the code enables interrupts the way it does in the specific order:
 *   1. At the very start, the IRQ is unmasked ahead of time (prior to main loop)
 *   2. The user hits '6' to enable interrupts
 *   3. The code sends a specific EOI to the programmable interrupt controller. If we don't do this,
 *      the Programmable Interrupt Controller may fail to send interrupts in the event the UART
 *      was already holding the IRQ line high and interrupts will not happen
 *   4. We set all lower 4 bits of the Interrupt Enable register of the UART so that we're notified
 *      when (1) data is available (2) the trasmitter buffer is empty (and waiting for more)
 *      (3 & 4) any change in the line or modem status registers
 *   5. Experience tells me that if for any reason the UART had events pending, but was interrupted
 *      for any reason, it might not fire any futher interrupts until someone reads the IIR and
 *      services each register appropriately. So the UART is never serviced because the IRQ is never
 *      fired, and the IRQ is never fired because the UART is never serviced. Forcibly read and discard
 *      all "events" from the UART to clear out that queue and get the UART firing off interrupts once
 *      again
 *
 *   Again: if you change the order of operations there, you will produce code that for one reason or
 *   another fail to enable UART interrupts on some computers, and will happen to work on other
 *   computers. Follow the above order strictly, and interrupts will fire like they are supposed to. */

/* Test scenarios.
 *
 *     DB-9 port = Device is a UART with an externally visible 9-pin D-shell connector
 *     DB-25 port= Device is a UART with an externally visible 25-pin D-shell connector
 *     Int modem = Device is an internal modem emulating a UART
 *     IRDA      = Device is an infared port pretending to be a UART
 *     Emulated  = If platform involves an emulator, the UART is emulated by the software
 *                 If the platform is actual hardware, then the BIOS or host OS traps I/O to emulate a UART
 *
 * Scenario                          I/O works       Bi-dir    Up to    UART loop     Type      Notes
 *                                 Poll  Interrupt  ectional   baud   Poll Interrupt
 * --------------------------------------------------------------------------------------------------
 * Microsoft Virtual PC 2007
 *     - real & protected mode       Y     Y           Y      115200   N      N       Emulated  Everything works, except loopback is improperly
 *                                                                                              emulated. What is actually received is the last byte
 *                                                                                              received, not the one we sent out.
 *            * Actual transfer speed is limited by the mechanism used to emulate. If using NT pipes like \\.\pipe\dostest the fake UART
 *              will stall until that pipe can be written by Virtual PC.
 *            * Works with both traditional and ISA Plug & Play versions of the test program.
 *            * Prior versions of these comments complained about the Real-mode copy running erratically and crashing.
 *              It was discovered the CPU detection code was calling a far routine as if near and the incorrect stack
 *              return caused the erratic behavior (as well as solid hangs on other configurations). Since the fix was
 *              applied VirtualBox now runs our code flawlessly.
 *
 * Oracle VirtualBox 4.0.4
 *     - real & protected ver        Y     Y           Y      115200   Y      Y       Emulated  Everything works fine. VirtualBox's UART emulation
 *                                                                                              is very tolerant of errors, perhaps even deceptively
 *                                                                                              tolerant compared to actual hardware.
 *            * Works with traditional program. VirtualBox does not emulate ISA Plug & Play.
 *
 * Toshiba Satellite Pro 465CDX laptop
 *     - With COM1 (0x3F8)           Y     Y           Y      115200   Y      ?      DB-9 port  Works fine.
 *     - With COM2 (0x2F8)           Y     Y           ?      ?        Y      ?      Int modem  Communication works at any baud, Hayes compatible
 *     - With COM3 (0x3E8)           Y     Y           N      115200   Y      ?      IRDA       Toshiba's BIOS puts this on IRQ 9. The port and IRQ
 *                                                                                              will only be detected properly by the ISA PnP version.
 *                                                                                              The traditional version will mis-detect the port IRQ
 *                                                                                              as 4.
 *
 *                                                                                              Bidirectional communications are impossible over IRDA.
 *                                                                                              If both ends transmit one byte simultaneously the
 *                                                                                              bits collide and each end receives gibberish.
 *                                                                                              Perfect transmission is only possible if each end
 *                                                                                              takes turns transmitting and receiving.
 *             * DOS extender problem (dos32a): When we enable and hook the UART IRQ the DOS extender
 *               crashes back to DOS with "Invalid TSS" exception (INT 0x0A). This makes the 32-bit
 *               protected mode version unusable on Toshiba hardware unless polling is involved.
 *               For interrupt-enabled testing, you must use the real-mode version.
 *
 * Toshiba Libretto laptop
 *     - With COM1 (0x3F8)           Y     Y           Y      115200   Y      ?      DB-9 port  Actual communication not verified. The port is only
 *                                                                                              available through the docking station, which I do not
 *                                                                                              have.
 *     - With COM2 (0x2F8)           Y     Y           N      115200   Y      ?      IRDA       Bidirectional communications are not possible. Each end
 *                                                                                              must take turns transmitting and receiving, or else
 *                                                                                              data becomes corrupt in transmission.
 *             * DOS extender problem (dos32a): When we enable and hook the UART IRQ the DOS extender
 *               crashes back to DOS with "Invalid TSS" exception (INT 0x0A). This makes the 32-bit
 *               protected mode version unusable on Toshiba hardware unless polling is involved.
 *               For interrupt-enabled testing, you must use the real-mode version.
 *
 * Compaq Elite laptop
 *     - With COM1 (0x3F8)           Y     Y           Y      115200   Y      ?      DB-9 port
 */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>
#include <hw/8250/8250.h>
#include <hw/dos/doswin.h>

#if defined(ISAPNP)
#include <hw/8250/8250pnp.h>
#include <hw/isapnp/isapnp.h>
#endif

/* global variable: the uart object */
static struct info_8250 *uart = NULL;

/* IRQ transfer buffers. Used by interrupt handler to store incoming data. Non-interrupt code should disable interrupts before accessing */
#define IRQ_BUFFER_SIZE	512

static unsigned char irq_buffer[IRQ_BUFFER_SIZE];
static unsigned int irq_buffer_i=0,irq_buffer_o=0;

static unsigned char irq_bufferout[IRQ_BUFFER_SIZE];
static unsigned int irq_bufferout_i=0,irq_bufferout_o=0;

static void irq_buffer_reset() {
	_cli();
	irq_buffer_i = irq_buffer_o = 0;
	irq_bufferout_i = irq_bufferout_o = 0;
	_sti();
}

/* take any IRQ buffered output and send it.
 * must be called with interrupts disabled.
 * may be called from the IRQ handler. */
static void irq_uart_xmit_bufferout(struct info_8250 *uart) {
	unsigned char c;

	if (irq_bufferout_o != irq_bufferout_i) {
		/* then send it */
		while (uart_8250_can_write(uart) && irq_bufferout_o != irq_bufferout_i) {
			c = irq_bufferout[irq_bufferout_o++];
			if (irq_bufferout_o >= IRQ_BUFFER_SIZE) irq_bufferout_o = 0;
			outp(uart->port+PORT_8250_IO,c);
		}
	}
}

static void irq_uart_recv_buffer(struct info_8250 *uart) {
	unsigned char c;

	while (uart_8250_can_read(uart)) {
		c = inp(uart->port+PORT_8250_IO);
		if ((irq_buffer_i+1)%IRQ_BUFFER_SIZE != irq_buffer_o) {
			irq_buffer[irq_buffer_i++] = c;
			if (irq_buffer_i >= IRQ_BUFFER_SIZE) irq_buffer_i = 0;
		}
	}
}

static int irq_bufferout_count() {
	int x = irq_bufferout_i - irq_bufferout_o;
	if (x < 0) x += IRQ_BUFFER_SIZE;
	return x;
}

/* NOTE: You're supposed to call this function with interrupts disabled,
 *       or from within an interrupt handler in response to the UART's IRQ signal. */
static void irq_uart_handle_iir(struct info_8250 *uart) {
	unsigned char reason,c,patience = 8;

#if TARGET_MSDOS == 32
	(*((unsigned short*)0xB8010))++;
#else
	(*((unsigned short far*)MK_FP(0xB800,0x0010)))++;
#endif

	/* why the interrupt? */
	/* NOTE: we loop a maximum of 8 times in case the UART we're talking to happens
	 *       to be some cheap knockoff chipset that never clears the IIR register
	 *       when all events are read */
	/* if there was actually an interrupt, then handle it. loop until all interrupt conditions are depleted */
	while (((reason=inp(uart->port+PORT_8250_IIR)&7)&1) == 0) {
		reason >>= 1;
#if TARGET_MSDOS == 32
		(*((unsigned short*)0xB8000))++;
#else
		(*((unsigned short far*)MK_FP(0xB800,0x0000)))++;
#endif

		if (reason == 3) { /* line status */
#if TARGET_MSDOS == 32
			(*((unsigned short*)0xB8008))++;
#else
			(*((unsigned short far*)MK_FP(0xB800,0x0008)))++;
#endif

			c = inp(uart->port+PORT_8250_LSR);
			/* do what you will with this info */
		}
		else if (reason == 0) { /* modem status */
#if TARGET_MSDOS == 32
			(*((unsigned short*)0xB8006))++;
#else
			(*((unsigned short far*)MK_FP(0xB800,0x0006)))++;
#endif

			c = inp(uart->port+PORT_8250_MSR);
			/* do what you will with this info */
		}
		else if (reason == 2) { /* data avail */
#if TARGET_MSDOS == 32
			(*((unsigned short*)0xB8002))++;
#else
			(*((unsigned short far*)MK_FP(0xB800,0x0002)))++;
#endif

			irq_uart_recv_buffer(uart);
		}
		else if (reason == 1) { /* transmit empty */
#if TARGET_MSDOS == 32
			(*((unsigned short*)0xB8004))++;
#else
			(*((unsigned short far*)MK_FP(0xB800,0x0004)))++;
#endif

			irq_uart_xmit_bufferout(uart);
		}

		if (--patience == 0) {
#if TARGET_MSDOS == 32
			(*((unsigned short*)0xB800A))++;
#else
			(*((unsigned short far*)MK_FP(0xB800,0x000A)))++;
#endif
			break;
		}
	}
}

static void (interrupt *old_irq)() = NULL;
static void interrupt uart_irq() {
	/* clear interrupts, just in case. NTS: the nature of interrupt handlers
	 * on the x86 platform (IF in EFLAGS) ensures interrupts will be reenabled on exit */
	_cli();
	irq_uart_handle_iir(uart);

	/* ack PIC */
	if (uart->irq >= 8) p8259_OCW2(8,P8259_OCW2_NON_SPECIFIC_EOI);
	p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
}

static void change_config(struct info_8250 *uart) {
	int done=0,c;
	uint8_t by,t2;

	while (!done) {
		{
			unsigned long baud=0;
			unsigned char bits=0,stop_bits=0,parity=0;
			uart_8250_get_config(uart,&baud,&bits,&stop_bits,&parity);
			printf("State: %lu baud %u-bit %u stop bits %s\n",baud,bits,stop_bits,type_8250_parity(parity));
			printf("1/!. Baud rate inc/dec  2/@. Bits inc/dec  3. Stop bits  4. Parity\n");
			printf("? "); fflush(stdout);
		}

		c = getch();
		printf("\n");
		if (c == 27) {
			done=1;
			break;
		}
		else if (c == '1' || c == '!') {
			unsigned long rate = 9600;
			printf("new rate? "); fflush(stdout);
			scanf("%lu",&rate);
			uart_8250_set_baudrate(uart,uart_8250_baud_to_divisor(uart,rate));
		}
		else if (c == '2' || c == '@') {
			by = inp(uart->port+PORT_8250_LCR);
			if (c == '2')	by = (by & (~3)) | (((by & 3) + 3) & 3);
			else		by = (by & (~3)) | (((by & 3) + 1) & 3);
			outp(uart->port+PORT_8250_LCR,by);
		}
		else if (c == '3' || c == '#') {
			by = inp(uart->port+PORT_8250_LCR);
			by ^= 4;
			outp(uart->port+PORT_8250_LCR,by);
		}
		else if (c == '4' || c == '$') {
			by = inp(uart->port+PORT_8250_LCR);
			t2 = (by >> 3) & 7;
			if (t2 == 0)		t2 = 1;
			else if (t2 == 1)	t2 = 3;
			else if (t2 >= 3)	t2 += 2;
			if (t2 >= 8) t2 = 0;
			by &= ~(7 << 3);
			by |= t2 << 3;
			outp(uart->port+PORT_8250_LCR,by);
		}
	}
}

static void config_fifo(struct info_8250 *uart) {
	uint8_t fcr=0;
	int done=0,c;

	if (uart->type <= TYPE_8250_IS_16550) {
		printf("Your UART (as far as I know) does not have a FIFO\n");
		return;
	}

	while (!done) {
		printf("FCR: Enable=%u mode=%u 64byte=%u recv_trigger_level=%u\n",
			(fcr & 1) ? 1 : 0,
			(fcr & 8) ? 1 : 0,
			(fcr & 32) ? 1 : 0,
			fcr >> 6);
		printf("1. Flush & enable/disable     2. Mode       3. 64byte    4. Level\n");
		printf("? "); fflush(stdout);

		c = getch();
		printf("\n");
		if (c == 27) {
			done=1;
			break;
		}
		else if (c == '1') {
			fcr ^= 1;
			uart_8250_set_FIFO(uart,fcr);
		}
		else if (c == '2') {
			fcr ^= 1 << 3;
			uart_8250_set_FIFO(uart,fcr);
		}
		else if (c == '3') {
			fcr ^= 1 << 5;
			uart_8250_set_FIFO(uart,fcr);
		}
		else if (c == '4') {
			fcr += 1 << 6;
			uart_8250_set_FIFO(uart,fcr);
		}
	}
}

void irq_bufferout_write(unsigned char c) {
	_cli();
	while (((irq_bufferout_i+1)%IRQ_BUFFER_SIZE) == irq_bufferout_o) {
		/* try to force transmit interrupt that gets the character out the door */
		uart_toggle_xmit_ien(uart);
		_sti();
		/* IODELAY during interrupt enable. Give the UART's interrupt a chance to interrupt the CPU */
		inp(uart->port+PORT_8250_MCR); /* iodelay */
		inp(uart->port+PORT_8250_MCR); /* iodelay */
		inp(uart->port+PORT_8250_MCR); /* iodelay */
		inp(uart->port+PORT_8250_MCR); /* iodelay */
		_cli();
	}

	/* put it in the buffer */
	irq_bufferout[irq_bufferout_i++] = c;
	if (irq_bufferout_i >= IRQ_BUFFER_SIZE) irq_bufferout_i = 0;
	if (irq_bufferout_count() == 1) uart_toggle_xmit_ien(uart);
	_sti();
}

static void show_console(struct info_8250 *uart) {
	static const char *msg = "Testing testing 1... 2... 3... 01234567890123456789.\r\n"
		"The big dog jumped the shark or something like that.\r\n";
	unsigned char pc=0,seqmatch=0,xmitseq=0,xmitbyte=0;
	const size_t msg_len = strlen(msg);
	unsigned int patience;
	char stuck_xmit=0;
	int c;

	printf("Incoming data will be printed out, and data you type will xmit (port=%X)\n",uart->port);
	printf("Tap ESC twice to exit. '~' = type out a predefined message.\n");
	printf("SHIFT + ~ to rapidly transmit the message.\n");
	printf("Type CTRL+A to initiate sequential byte test.\n");

	if (use_8250_int) irq_buffer_reset();

	while (1) {
		if (kbhit()) {
			c = getch();
			if (c == 27) {
				c = getch();
				if (c == 27) break;
			}
			if (c == '`') {
				stuck_xmit = !stuck_xmit;
			}
			else if (c == 1) { /* CTRL+S */
				xmitseq = !xmitseq;
			}
			else if (c == '~') {
				const char *p = msg;
				while (*p != 0) {
					c = *p++;
					if (use_8250_int) {
						irq_bufferout_write(c);
					}
					else {
						while (!uart_8250_can_write(uart));
						uart_8250_write(uart,(uint8_t)c);
					}
				}
			}
			else if (c == 13) {
				if (use_8250_int) {
					irq_bufferout_write(13);
					irq_bufferout_write(10);
				}
				else {
					while (!uart_8250_can_write(uart));
					uart_8250_write(uart,13);
					while (!uart_8250_can_write(uart));
					uart_8250_write(uart,10);
				}
				printf("\n");
			}
			else if (c == 10) {
				/* ignore */
			}
			else {
				if (use_8250_int) {
					irq_bufferout_write(c);
				}
				else {
					while (!uart_8250_can_write(uart));
					uart_8250_write(uart,(uint8_t)c);
				}
				fwrite(&c,1,1,stdout); fflush(stdout);
			}
		}

		if (use_8250_int) {
			int sm=0;
			/* the interrupt handler takes care of reading the data in (in bursts, if necessary).
			 * our job is to follow the buffer. Note we have a "patience" parameter to break out
			 * of the loop in cases where fast continious transmission prevents us from ever
			 * emptying the buffer entirely. */
			_cli();
			patience = msg_len;
			while (patience-- != 0 && irq_buffer_o != irq_buffer_i) {
				c = irq_buffer[irq_buffer_o++];
				if (irq_buffer_o >= IRQ_BUFFER_SIZE) irq_buffer_o = 0;
				_sti();

				if (seqmatch >= 16) {
					if (((pc+1)&0xFF) != c) {
						if (sm++ == 0)
							printf("Sequential byte error %u -> %u\n",pc,c);
					}
					else if (c == 0) {
						printf(":)"); fflush(stdout);
					}
				}
				else {
					if (c == 13) {
						printf("\n");
					}
					else {
						fwrite(&c,1,1,stdout); fflush(stdout);
					}
				}

				if (((pc+1)&0xFF) == c) {
					if (seqmatch < 255) seqmatch++;
				}
				else if (seqmatch > 0) {
					seqmatch--;
				}
				pc = c;
			}
			_sti();
		}
		else {
			while (uart_8250_can_read(uart)) {
				c = uart_8250_read(uart);
				if (seqmatch >= 16) {
					if (((pc+1)&0xFF) != c) {
						printf("Sequential byte error %u -> %u\n",pc,c);
					}
					else if (c == 0) {
						printf(":)"); fflush(stdout);
					}
				}
				else {
					if (c == 13) {
						printf("\n");
					}
					else {
						fwrite(&c,1,1,stdout); fflush(stdout);
					}

				}

				if (((pc+1)&0xFF) == c) {
					if (seqmatch < 255) seqmatch++;
				}
				else if (seqmatch > 0) {
					seqmatch--;
				}
				pc = c;
			}
		}

		if (xmitseq) {
			unsigned int c = 0;
			for (c=0;c < 0x40;c++) {
				if (use_8250_int) {
					irq_bufferout_write(xmitbyte++);
				}
				else {
					if (!uart_8250_can_write(uart)) break;
					uart_8250_write(uart,xmitbyte++);
				}
			} 
		}
		else if (stuck_xmit) {
			const char *p = msg;
			while (*p != 0) {
				c = *p++;
				if (use_8250_int) {
					irq_bufferout_write(c);
				}
				else {
					while (!uart_8250_can_write(uart));
					uart_8250_write(uart,(uint8_t)c);
				}
			}
		}
	}

	printf("\nDONE\n");
}

#ifdef ISAPNP
static unsigned char devnode_raw[4096];

void pnp_serial_scan() {
	/* most of the time the serial ports are BIOS controlled and on the motherboard.
	 * they usually don't even show up in a PnP isolation scan. so we have to use
	 * the "get device nodes" functions of the PnP BIOS. */
	{
		struct isa_pnp_device_node far *devn;
		unsigned int ret_ax,nodesize=0xFFFF;
		unsigned char numnodes=0xFF;
		struct isapnp_tag tag;
		unsigned char node;

		printf("Enumerating PnP system device nodes...\n");

		ret_ax = isa_pnp_bios_number_of_sysdev_nodes(&numnodes,&nodesize);
		if (ret_ax == 0 && numnodes != 0xFF && nodesize < sizeof(devnode_raw)) {
			/* NTS: How nodes are enumerated in the PnP BIOS: set node = 0, pass address of node
			 *      to BIOS. BIOS, if it returns node information, will also overwrite node with
			 *      the node number of the next node, or with 0xFF if this is the last one.
			 *      On the last one, stop enumerating. */
			for (node=0;node != 0xFF;) {
				unsigned char far *rsc;
				int port = -1;
				int irq = -1;

				/* apparently, start with 0. call updates node to
				 * next node number, or 0xFF to signify end */
				ret_ax = isa_pnp_bios_get_sysdev_node(&node,devnode_raw,
						ISA_PNP_BIOS_GET_SYSDEV_NODE_CTRL_NOW);

				if (ret_ax != 0)
					break;

				devn = (struct isa_pnp_device_node far*)devnode_raw;
				if (!is_rs232_or_compat_pnp_device(devn))
					continue;

				/* there are three config blocks, one after the other.
				 *  [allocated]
				 *  [possible]
				 *  [??]
				 * since we're not a configuration utility, we only care about the first one */
				rsc = devnode_raw + sizeof(*devn);
				if (isapnp_read_tag(&rsc,devnode_raw + devn->size,&tag)) {
					do {
						if (tag.tag == ISAPNP_TAG_END) /* end tag */
							break;

						switch (tag.tag) {
/*---------------------------------------------------------------------------------*/
case ISAPNP_TAG_IRQ_FORMAT: {
	struct isapnp_tag_irq_format far *x = (struct isapnp_tag_irq_format far*)tag.data;
	unsigned int i;
	for (i=0;i < 16 && irq < 0;i++) {
		if (x->irq_mask & (1U << (unsigned int)i))
			irq = i;
	}
} break;
case ISAPNP_TAG_IO_PORT: {
	struct isapnp_tag_io_port far *x = (struct isapnp_tag_io_port far*)tag.data;
	if (x->length >= 8 && port < 0) port = x->min_range;
} break;
case ISAPNP_TAG_FIXED_IO_PORT: {
	struct isapnp_tag_fixed_io_port far *x = (struct isapnp_tag_fixed_io_port far*)tag.data;
	if (x->length >= 8 && port < 0) port = x->base;
} break;
/*---------------------------------------------------------------------------------*/
						};
					} while (isapnp_read_tag(&rsc,devnode_raw + devn->size,&tag));
				}

				if (port < 0)
					continue;

				if (add_pnp_8250(port,irq))
					printf("Found PnP port @ 0x%03x IRQ %d\n",port,irq);
			}
		}
	}
}
#endif

int main() {
	unsigned char msr_redraw = 1;
	unsigned char redraw = 1;
	unsigned char p_msr = 0;
	int i,die,choice;

	printf("8250/16450/16550 test program\n");
#ifdef ISAPNP
	printf("ISA Plug & Play version\n");
#endif

	cpu_probe();		/* ..for the DOS probe routine */
	probe_dos();		/* ..for the Windows detection code */
	detect_windows();	/* Windows virtualizes the COM ports, and we don't want probing to occur to avoid any disruption */

	if (!probe_8254()) {
		printf("8254 not found (I need this for time-sensitive portions of the driver)\n");
		return 1;
	}

	if (!probe_8259()) {
		printf("8259 not found (I need this for portions of the test involving serial interrupts)\n");
		return 1;
	}

	if (!init_8250()) {
		printf("Cannot init 8250 library\n");
		return 1;
	}

#if defined(ISAPNP)
	if (!init_isa_pnp_bios()) {
		printf("Cannot init ISA PnP\n");
		return 1;
	}
	if (find_isa_pnp_bios()) {
		pnp_serial_scan();
	}
	else {
		printf("Warning, ISA PnP BIOS not found\n");
	}
#else
	printf("%u BIOS I/O ports listed\nThey are: ",(unsigned int)bios_8250_ports);
	for (i=0;i < (int)bios_8250_ports;i++) printf("0x%04x ",get_8250_bios_port(i));
	printf("\n");

	printf("Now probing ports: "); fflush(stdout);
	for (i=0;!base_8250_full() && i < (int)bios_8250_ports;i++) {
		const uint16_t port = get_8250_bios_port(i);
		if (port == 0) continue;
		printf("0x%03X ",port); fflush(stdout);
		if (probe_8250(port)) printf("[OK] ");
	}
	if (windows_mode == WINDOWS_NONE || windows_mode == WINDOWS_REAL) {
		/* if we're running under Windows it's likely the kernel is virtualizing the ports. play it safe and
		 * stick to the "BIOS" ports that Windows and it's virtualization likely added to the data area */
		for (i=0;!base_8250_full() && i < (int)(sizeof(standard_8250_ports)/sizeof(standard_8250_ports[0]));i++) {
			const uint16_t port = standard_8250_ports[i];
			if (port == 0) continue;
			printf("0x%03X ",port); fflush(stdout);
			if (probe_8250(port)) printf("[OK] ");
		}
	}
	printf("\n");
#endif

	for (i=0;i < base_8250_ports;i++) {
		struct info_8250 *inf = &info_8250_port[i];
		printf("[%u] @ %03X (type %s IRQ %d)\n",i+1,inf->port,type_8250_to_str(inf->type),inf->irq);
	}
	printf("Choice? "); fflush(stdout);
	choice = -1;
	scanf("%d",&choice);
	choice--;
	if (choice < 0 || choice >= base_8250_ports) return 0;
	uart = &info_8250_port[choice];

	if (uart->irq != -1) {
		old_irq = _dos_getvect(irq2int(uart->irq));
		_dos_setvect(irq2int(uart->irq),uart_irq);
		if (uart->irq >= 0) {
			p8259_unmask(uart->irq);
			if (uart->irq >= 8) p8259_OCW2(8,P8259_OCW2_SPECIFIC_EOI | (uart->irq&7));
			p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | (uart->irq&7));
		}
	}

	uart_8250_enable_interrupt(uart,0);	/* disable interrupts (set IER=0) */
	uart_8250_disable_FIFO(uart);
	uart_8250_set_MCR(uart,3);		/* turn on RTS and DTS */
	uart_8250_set_line_control(uart,UART_8250_LCR_8BIT | UART_8250_LCR_PARITY);	/* 8 bit 1 stop bit odd parity */
	uart_8250_set_baudrate(uart,uart_8250_baud_to_divisor(uart,9600));

	for (die=0;die == 0;) {
		unsigned char msr = uart_8250_read_MSR(uart);

		if (redraw) {
			unsigned long baud=0;
			unsigned char bits=0,stop_bits=0,parity=0;
			unsigned char mcr = uart_8250_read_MCR(uart);

			redraw = 0;
			msr_redraw = 1;

			uart_8250_get_config(uart,&baud,&bits,&stop_bits,&parity);
			printf("\x0D");
			printf("State: %lu baud %u-bit %u stop bits %s DTR=%u RTS=%u LOOP=%u\n",baud,bits,stop_bits,type_8250_parity(parity),
				(mcr & 1) ? 1 : 0/*DTR*/,
				(mcr & 2) ? 1 : 0/*RTS*/,
				(mcr & 16) ? 1 : 0/*LOOP*/);
			printf("1. Change config   2. Toggle DTR  3. Toggle RTS   4. Show on console\n");
			printf("5. Config FIFO     ");
			if (use_8250_int)	printf("6. To poll IO  ");
			else			printf("6. To int IO   ");
			printf("7. Toggle LOOP ");
			printf("\n");
		}

		if ((msr ^ p_msr) & 0xF0) msr_redraw = 1;
		p_msr = msr;

		if (msr_redraw) {
			msr_redraw = 0;
			printf(	"\x0D"
				"CTS=%u DSR=%u RI=%u CD=%u ? ",
				(msr & 16) ? 1 : 0,
				(msr & 32) ? 1 : 0,
				(msr & 64) ? 1 : 0,
				(msr & 128) ? 1 : 0);
			fflush(stdout);
		}

		if (kbhit()) {
			choice = getch();
			if (choice == 27) {
				die++;
				break;
			}
			else if (choice == '1') {
				change_config(uart);
				redraw = 1;
			}
			else if (choice == '2') {
				uart_8250_set_MCR(uart,uart_8250_read_MCR(uart) ^ 1);
				redraw = 1;
			}
			else if (choice == '3') {
				uart_8250_set_MCR(uart,uart_8250_read_MCR(uart) ^ 2);
				redraw = 1;
			}
			else if (choice == '7') {
				uart_8250_set_MCR(uart,uart_8250_read_MCR(uart) ^ 16);
				redraw = 1;
			}
			else if (choice == '4') {
				show_console(uart);
				redraw = 1;
			}
			else if (choice == '5') {
				config_fifo(uart);
				redraw = 1;
			}
			else if (choice == '6') {
				/* NTS: Apparently a lot of hardware has problems with just turning on interrupts.
				 *      So do everything we can to fucking whip the UART into shape. Drain & reset
				 *      it's FIFO. Clear all interrupt events. Unmask the IRQ. Enable all interrupt
				 *      events. What ever it fucking takes */
				_cli();

				/* ACK the IRQ to ensure the PIC will send more. Doing this resolves a bug on
				 * a Toshiba 465CDX and IBM NetVista where enabling interrupts would seem to
				 * "hang" the machine (when in fact it was just the PIC not firing interrupts
				 * and therefore preventing the keyboard and timer from having their interrupts
				 * serviced) */
				if (uart->irq >= 8) p8259_OCW2(8,P8259_OCW2_SPECIFIC_EOI | (uart->irq&7));
				p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | (uart->irq&7));
				redraw = 1;

				use_8250_int = !use_8250_int;
				if (use_8250_int) uart_8250_enable_interrupt(uart,0xF);
				else uart_8250_enable_interrupt(uart,0x0);
				for (i=0;i < 256 && (inp(uart->port+PORT_8250_IIR) & 1) == 0;i++) {
					inp(uart->port);
					inp(uart->port+PORT_8250_MSR);
					inp(uart->port+PORT_8250_MCR);
					inp(uart->port+PORT_8250_LSR);
					inp(uart->port+PORT_8250_IIR);
					inp(uart->port+PORT_8250_IER);
				}
				if (i == 256) printf("Warning: Unable to clear UART interrupt conditions\n");
				_sti();
			}
		}
	}

	uart_8250_enable_interrupt(uart,0);	/* disable interrupts (set IER=0) */
	uart_8250_set_MCR(uart,0);		/* RTS/DTR and aux lines off */
	uart_8250_disable_FIFO(uart);
	uart_8250_set_line_control(uart,UART_8250_LCR_8BIT | UART_8250_LCR_PARITY);	/* 8 bit 1 stop bit odd parity */
	uart_8250_set_baudrate(uart,uart_8250_baud_to_divisor(uart,9600));

	if (uart->irq != -1) {
		_dos_setvect(irq2int(uart->irq),old_irq);
		if (uart->irq >= 0) p8259_mask(uart->irq);
	}

	return 0;
}

