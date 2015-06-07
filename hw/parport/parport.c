
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/parport/parport.h>

const char *parport_type_str[PARPORT_MAX] = {
	"Standard",
	"Bi-directional",
	"ECP",
	"EPP",
	"ECP+EPP"
};

int				init_parports = 0;
int				bios_parports = 0;
uint16_t			standard_parport_ports[STANDARD_PARPORT_PORTS] = {0x3BC,0x378,0x278};
struct info_parport		info_parport[MAX_PARPORTS];
int				info_parports=0;

int already_got_parport(uint16_t port) {
	unsigned int i;

	for (i=0;i < (unsigned int)info_parports;i++) {
		if (info_parport[i].port == port)
			return 1;
	}

	return 0;
}

uint16_t get_bios_parport(unsigned int index) {
	if (index >= (unsigned int)bios_parports)
		return 0;

#if TARGET_MSDOS == 32
	return *((uint16_t*)(0x400 + 8 + (index*2)));
#else
	return *((uint16_t far*)MK_FP(0x40,8 + (index*2)));
#endif
}

int init_parport() {
	if (!init_parports) {
		uint16_t eqw;

		bios_parports = 0;
		info_parports = 0;
		init_parports = 1;

		memset(info_parport,0,sizeof(info_parport));

		/* read the BIOS equipment word[11-9]. how many serial ports? */
#if TARGET_MSDOS == 32
		eqw = *((uint16_t*)(0x400 + 0x10));
#else
		eqw = *((uint16_t far*)MK_FP(0x40,0x10));
#endif
		bios_parports = (eqw >> 14) & 3;

	}
	return 1;
}

/* TODO: Add code where if caller guesses by PnP info the port is ECP or EPP (or both) we
 *       omit some tests and assume it's correct.
 *
 *       Example: If the BIOS reports two I/O ports and they are 0x400 apart, then the port
 *       is an ECP type and ECP type tests are unnecessary. If PnP data lists the I/O range
 *       as being 8 ports long, then it's probably an EPP type port too and we can omit
 *       those tests. */
int add_pnp_parport(uint16_t port,int irq,int dma,int type) {
	struct info_parport *prt;
	char bit10 = 0;

	if (port == 0)
		return 0;
	if (already_got_parport(port))
		return 0;
	if (info_parports >= MAX_PARPORTS)
		return 0;
	prt = &info_parport[info_parports];
	prt->bit10 = 0;
	prt->max_xfer_size = 1;
	prt->port = port;
	prt->output_mode = PARPORT_MODE_STANDARD_STROBE_ACK;
	prt->type = PARPORT_STANDARD;
	prt->irq = irq;
	prt->dma = dma;

	if (type >= 0) {
		/* if the caller knows better than us, then take his word for it */
		prt->type = type;

		if (PARPORT_SUPPORTS_ECP(prt->type)) {
			unsigned char a,b;

			/* detect ECP by attempting to enable the configuration mode */
			outp(prt->port+PARPORT_IO_ECP_CONTROL,7 << 5);
			a = inp(prt->port+PARPORT_IO_ECP_REG_A);
			b = inp(prt->port+PARPORT_IO_ECP_REG_B);

			if (a != 0xFF && b != 0xFF) {
				prt->type = PARPORT_ECP;

				switch ((a >> 4) & 7) {
					case 0:	prt->max_xfer_size = 2; break;
					case 1:	prt->max_xfer_size = 1; break;
					case 2:	prt->max_xfer_size = 4; break;
				}

				if (prt->irq < 0) {
					switch ((b >> 3) & 7) {
						case 1:	prt->irq = 7; break;
						case 2:	prt->irq = 9; break;
						case 3:	prt->irq = 10; break;
						case 4:	prt->irq = 11; break;
						case 5:	prt->irq = 14; break;
						case 6:	prt->irq = 15; break;
						case 7:	prt->irq = 5; break;
					}
				}

				if (prt->dma < 0) {
					switch (b&7) {
						case 1: prt->dma = 1; break;
						case 2: prt->dma = 2; break;
						case 3: prt->dma = 3; break;
						case 5: prt->dma = 5; break;
						case 6: prt->dma = 6; break;
						case 7: prt->dma = 7; break;
					}
				}
			}

			/* switch to EPP mode for the test below if the ECP port supports it */
			if (prt->type == PARPORT_ECP)
				outp(prt->port+PARPORT_IO_ECP_CONTROL,4 << 5);
		}
	}
	else {
		/* this might be one of those EPP/ECP ports, set it to "standard byte mode" to enable the below test to find the port.
		 * documentation mentions that "in ECP mode the SPP ports may disappear" so to successfully probe ports left in ECP
		 * mode we have to write to the control register to bring it back.
		 *
		 * The (minimal) documentation I have doesn't say whether this port can be read back.
		 * On older 10-bit decode hardware, this will probably only end up writing over the control bits (0x77A -> 0x37A)
		 * and should be safe to carry out. On newer hardware (mid 90's) it's highly unlikely anything would be assigned
		 * to I/O ports 0x778-0x77F. On newer PCI hardware, it's even less likely. */
		outp(prt->port+PARPORT_IO_ECP_CONTROL,1 << 5);	/* mode=byte (bi-directional) ecpint=0 dma=0 ecpserv=0 */

		/* put it back into Standard mode, disable bi-directional */
		outp(prt->port+PARPORT_IO_CONTROL,0x04);	/* select=0 init=1 autolinefeed=0 strobe=0 */
		/* control ports are usually readable. if what we wrote didn't take then this is probably not a parallel port */
		if ((inp(prt->port+PARPORT_IO_CONTROL) & 0xF) != 0x4) return 0;

		/* write to the control port again. if this is ancient 10-bit decode hardware, then it is probably NOT ECP/EPP
		 * compliant. we detect this by whether this IO write corrupts the control register.
		 * note control = port+2 and extended control = port+0x402 */
		outp(prt->port+PARPORT_IO_ECP_CONTROL,(1 << 5) | 0xC);	/* CONTROL = sets bidirectional, and two control lines. EXTCTRL = maintains byte mode and turns on ECP interrupt+DMA */
		if ((inp(prt->port+PARPORT_IO_CONTROL) & 0xF) == 0xC) /* if the +0x402 IO write changed the control register, then it's 10-bit decode and not EPP/ECP */
			bit10 = 1;

		outp(prt->port+PARPORT_IO_ECP_CONTROL,(1 << 5) | 0x0); /* regain sane extended control */
		outp(prt->port+PARPORT_IO_CONTROL,0x04);

		/* the data port is write only, though you are always allowed to read it back.
		 * on bidirectional ports this is how you read back data.
		 * we do not attempt to write to the status port. it is read only. worse, on modern
		 * systems it can conflict with the ISA Plug & Play protocol (0x279) */
		outp(prt->port,0x55); if (inp(prt->port) != 0x55) return 0;
		outp(prt->port,0xAA); if (inp(prt->port) != 0xAA) return 0;
		outp(prt->port,0xFF); if (inp(prt->port) != 0xFF) return 0;
		outp(prt->port,0x00); if (inp(prt->port) != 0x00) return 0;

		/* do a parport init printer */
		outp(prt->port+PARPORT_IO_CONTROL,0x04);	/* select=0 init=1 autolinefeed=0 strobe=0 */
		if ((inp(prt->port+PARPORT_IO_CONTROL)&0xF) != 0x4) return 0;

		/* try to toggle bit 5 (bi-directional enable) */
		outp(prt->port+PARPORT_IO_CONTROL,0x04);	/* select=0 init=1 autolinefeed=0 strobe=0 bidir=0 */
		if ((inp(prt->port+PARPORT_IO_CONTROL) & (1 << 5)) == 0) {
			outp(prt->port+PARPORT_IO_CONTROL,0x24);	/* select=0 init=1 autolinefeed=0 strobe=0 bidir=1 */
			if ((inp(prt->port+PARPORT_IO_CONTROL) & (1 << 5)) != 0)
				prt->type = PARPORT_BIDIRECTIONAL;

			outp(prt->port+PARPORT_IO_CONTROL,0x04);	/* select=0 init=1 autolinefeed=0 strobe=0 bidir=0 */
		}

		/* standard I/O locations have standard IRQ assignments, apparently */
		prt->bit10 = bit10;
		if (!bit10) { /* if not 10-bit decode, then check for ECP control registers */
			unsigned char a,b;

			/* detect ECP by attempting to enable the configuration mode */
			outp(prt->port+PARPORT_IO_ECP_CONTROL,7 << 5);
			a = inp(prt->port+PARPORT_IO_ECP_REG_A);
			b = inp(prt->port+PARPORT_IO_ECP_REG_B);

			if (a != 0xFF && b != 0xFF) {
				prt->type = PARPORT_ECP;

				switch ((a >> 4) & 7) {
					case 0:	prt->max_xfer_size = 2; break;
					case 1:	prt->max_xfer_size = 1; break;
					case 2:	prt->max_xfer_size = 4; break;
				}

				if (prt->irq < 0) {
					switch ((b >> 3) & 7) {
						case 1:	prt->irq = 7; break;
						case 2:	prt->irq = 9; break;
						case 3:	prt->irq = 10; break;
						case 4:	prt->irq = 11; break;
						case 5:	prt->irq = 14; break;
						case 6:	prt->irq = 15; break;
						case 7:	prt->irq = 5; break;
					}
				}

				if (prt->dma < 0) {
					switch (b&7) {
						case 1: prt->dma = 1; break;
						case 2: prt->dma = 2; break;
						case 3: prt->dma = 3; break;
						case 5: prt->dma = 5; break;
						case 6: prt->dma = 6; break;
						case 7: prt->dma = 7; break;
					}
				}
			}

			/* switch to EPP mode for the test below if the ECP port supports it */
			if (prt->type == PARPORT_ECP)
				outp(prt->port+PARPORT_IO_ECP_CONTROL,4 << 5);
		}
	}

	if (type < 0 || PARPORT_SUPPORTS_EPP(type)) {
		/* the caller might be autodetecting EPP based on the I/O length.
		 * we double-check to be sure, it might be some bullshit BIOS
		 * reporting 8 ports long for a device that only responds to the
		 * first three */

		/* an EPP port could conceivably exist on a 10-bit decode ISA card */
		/* NOTES: Toshiba laptops do NOT support EPP mode, only ECP. There appears to be a hardware
		 *        bug in decoding the 0x778-0x77F I/O region where port 0x77A is repeated on 0x77B,
		 *        0x77C, etc.
		 *
		 *        The Compaq LTE Elite supports EPP and ECP. It follows the standard in ignoring
		 *        the bidir bit when setting mode 0, but does not disable EPP registers when
		 *        mode is not EPP.
		 *          ... erm, well, at least I think the extra registers are EPP. The address port
		 *          always returns 0x00 and the data port always returns 0x25 (this is with nothing
		 *          attached to the parallel port). The problem is we can only detect if something
		 *          is there, we can't tell if it's an EPP port or just some ancient ISA card that
		 *          responds to 0x3FB-0x3FF with it's own weirdness. A Google search for Compaq LTE
		 *          Elite and parallel port suggests that there are tools to configure the port as
		 *          EPP, so obviously at some level it speaks EPP mode. */
		if (inp(prt->port+PARPORT_IO_EPP_ADDRESS) != 0xFF && inp(prt->port+PARPORT_IO_EPP_DATA) != 0xFF) {
			if (prt->type == PARPORT_ECP)
				prt->type = PARPORT_ECP_AND_EPP;
			else
				prt->type = PARPORT_EPP;
		}
	}

	/* return the port to standard bidir mode */
	if (PARPORT_SUPPORTS_ECP(prt->type))
		outp(prt->port+PARPORT_IO_ECP_CONTROL,1 << 5);

	/* shut off the lines */
	outp(prt->port+PARPORT_IO_CONTROL,0x00);	/* select=0 init=1 autolinefeed=0 strobe=0 bidir=0 */
	outp(prt->port+PARPORT_IO_DATA,0x00);

	info_parports++;
	return 1;
}

int probe_parport(uint16_t port) {
	struct info_parport *prt;
	char bit10 = 0;

	if (port == 0)
		return 0;
	if (already_got_parport(port))
		return 0;
	if (info_parports >= MAX_PARPORTS)
		return 0;
	prt = &info_parport[info_parports];
	prt->bit10 = 0;
	prt->max_xfer_size = 1;
	prt->port = port;
	prt->output_mode = PARPORT_MODE_STANDARD_STROBE_ACK;
	prt->type = PARPORT_STANDARD;
	prt->irq = -1;
	prt->dma = -1;

	/* this might be one of those EPP/ECP ports, set it to "standard byte mode" to enable the below test to find the port.
	 * documentation mentions that "in ECP mode the SPP ports may disappear" so to successfully probe ports left in ECP
	 * mode we have to write to the control register to bring it back.
	 *
	 * The (minimal) documentation I have doesn't say whether this port can be read back.
	 * On older 10-bit decode hardware, this will probably only end up writing over the control bits (0x77A -> 0x37A)
	 * and should be safe to carry out. On newer hardware (mid 90's) it's highly unlikely anything would be assigned
	 * to I/O ports 0x778-0x77F. On newer PCI hardware, it's even less likely. */
	outp(prt->port+PARPORT_IO_ECP_CONTROL,1 << 5);	/* mode=byte (bi-directional) ecpint=0 dma=0 ecpserv=0 */

	/* put it back into Standard mode, disable bi-directional */
	outp(prt->port+PARPORT_IO_CONTROL,0x04);	/* select=0 init=1 autolinefeed=0 strobe=0 */
	/* control ports are usually readable. if what we wrote didn't take then this is probably not a parallel port */
	if ((inp(prt->port+PARPORT_IO_CONTROL) & 0xF) != 0x4) return 0;

	/* write to the control port again. if this is ancient 10-bit decode hardware, then it is probably NOT ECP/EPP
	 * compliant. we detect this by whether this IO write corrupts the control register.
	 * note control = port+2 and extended control = port+0x402 */
	outp(prt->port+PARPORT_IO_ECP_CONTROL,(1 << 5) | 0xC);	/* CONTROL = sets bidirectional, and two control lines. EXTCTRL = maintains byte mode and turns on ECP interrupt+DMA */
	if ((inp(prt->port+PARPORT_IO_CONTROL) & 0xF) == 0xC) /* if the +0x402 IO write changed the control register, then it's 10-bit decode and not EPP/ECP */
		bit10 = 1;

	outp(prt->port+PARPORT_IO_ECP_CONTROL,(1 << 5) | 0x0); /* regain sane extended control */
	outp(prt->port+PARPORT_IO_CONTROL,0x04);

	/* the data port is write only, though you are always allowed to read it back.
	 * on bidirectional ports this is how you read back data.
	 * we do not attempt to write to the status port. it is read only. worse, on modern
	 * systems it can conflict with the ISA Plug & Play protocol (0x279) */
	outp(prt->port,0x55); if (inp(prt->port) != 0x55) return 0;
	outp(prt->port,0xAA); if (inp(prt->port) != 0xAA) return 0;
	outp(prt->port,0xFF); if (inp(prt->port) != 0xFF) return 0;
	outp(prt->port,0x00); if (inp(prt->port) != 0x00) return 0;

	/* do a parport init printer */
	outp(prt->port+PARPORT_IO_CONTROL,0x04);	/* select=0 init=1 autolinefeed=0 strobe=0 */
	if ((inp(prt->port+PARPORT_IO_CONTROL)&0xF) != 0x4) return 0;

	/* try to toggle bit 5 (bi-directional enable) */
	outp(prt->port+PARPORT_IO_CONTROL,0x04);	/* select=0 init=1 autolinefeed=0 strobe=0 bidir=0 */
	if ((inp(prt->port+PARPORT_IO_CONTROL) & (1 << 5)) == 0) {
		outp(prt->port+PARPORT_IO_CONTROL,0x24);	/* select=0 init=1 autolinefeed=0 strobe=0 bidir=1 */
		if ((inp(prt->port+PARPORT_IO_CONTROL) & (1 << 5)) != 0)
			prt->type = PARPORT_BIDIRECTIONAL;

		outp(prt->port+PARPORT_IO_CONTROL,0x04);	/* select=0 init=1 autolinefeed=0 strobe=0 bidir=0 */
	}

	/* standard I/O locations have standard IRQ assignments, apparently */
	prt->bit10 = bit10;
	if (prt->port == 0x3BC)
		prt->irq = 2;
	else if (prt->port == 0x378)
		prt->irq = 7;
	else if (prt->port == 0x278)
		prt->irq = 5;

	if (!bit10) { /* if not 10-bit decode, then check for ECP control registers */
		unsigned char a,b;

		/* detect ECP by attempting to enable the configuration mode */
		outp(prt->port+PARPORT_IO_ECP_CONTROL,7 << 5);
		a = inp(prt->port+PARPORT_IO_ECP_REG_A);
		b = inp(prt->port+PARPORT_IO_ECP_REG_B);

		if (a != 0xFF && b != 0xFF) {
			prt->type = PARPORT_ECP;

			switch ((a >> 4) & 7) {
				case 0:	prt->max_xfer_size = 2; break;
				case 1:	prt->max_xfer_size = 1; break;
				case 2:	prt->max_xfer_size = 4; break;
			}

			switch ((b >> 3) & 7) {
				case 1:	prt->irq = 7; break;
				case 2:	prt->irq = 9; break;
				case 3:	prt->irq = 10; break;
				case 4:	prt->irq = 11; break;
				case 5:	prt->irq = 14; break;
				case 6:	prt->irq = 15; break;
				case 7:	prt->irq = 5; break;
			}

			switch (b&7) {
				case 1: prt->dma = 1; break;
				case 2: prt->dma = 2; break;
				case 3: prt->dma = 3; break;
				case 5: prt->dma = 5; break;
				case 6: prt->dma = 6; break;
				case 7: prt->dma = 7; break;
			}
		}

		/* switch to EPP mode for the test below if the ECP port supports it */
		if (prt->type == PARPORT_ECP)
			outp(prt->port+PARPORT_IO_ECP_CONTROL,4 << 5);
	}

	/* an EPP port could conceivably exist on a 10-bit decode ISA card */
	/* NOTES: Toshiba laptops do NOT support EPP mode, only ECP. There appears to be a hardware
	 *        bug in decoding the 0x778-0x77F I/O region where port 0x77A is repeated on 0x77B,
	 *        0x77C, etc.
	 *
	 *        The Compaq LTE Elite supports EPP and ECP. It follows the standard in ignoring
	 *        the bidir bit when setting mode 0, but does not disable EPP registers when
	 *        mode is not EPP.
	 *          ... erm, well, at least I think the extra registers are EPP. The address port
	 *          always returns 0x00 and the data port always returns 0x25 (this is with nothing
	 *          attached to the parallel port). The problem is we can only detect if something
	 *          is there, we can't tell if it's an EPP port or just some ancient ISA card that
	 *          responds to 0x3FB-0x3FF with it's own weirdness. A Google search for Compaq LTE
	 *          Elite and parallel port suggests that there are tools to configure the port as
	 *          EPP, so obviously at some level it speaks EPP mode. */
	if (inp(prt->port+PARPORT_IO_EPP_ADDRESS) != 0xFF && inp(prt->port+PARPORT_IO_EPP_DATA) != 0xFF) {
		if (prt->type == PARPORT_ECP)
			prt->type = PARPORT_ECP_AND_EPP;
		else
			prt->type = PARPORT_EPP;
	}

	/* return the port to standard bidir mode */
	if (PARPORT_SUPPORTS_ECP(prt->type))
		outp(prt->port+PARPORT_IO_ECP_CONTROL,1 << 5);

	/* shut off the lines */
	outp(prt->port+PARPORT_IO_CONTROL,0x00);	/* select=0 init=0 autolinefeed=0 strobe=0 bidir=0 */
	outp(prt->port+PARPORT_IO_DATA,0x00);

	info_parports++;
	return 1;
}

