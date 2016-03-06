
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/dos/dosbox.h>
#include <hw/8237/8237.h>		/* 8237 DMA */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/dos/doswin.h>
#include <hw/dos/tgusmega.h>
#include <hw/dos/tgussbos.h>

/* Windows 9x VxD enumeration */
#include <windows/w9xvmm/vxd_enum.h>

/* uncomment this to enable debugging messages */
//#define DBG

#if defined(DBG)
# define DEBUG(x) (x)
#else
# define DEBUG(x)
#endif

void sndsb_ess_extensions_probe(struct sndsb_ctx *cx) {
	uint16_t ess_ver;
	int in;

	/* Use DSP command 0xE7 to detect ESS chipset */
	if (!sndsb_write_dsp(cx,0xE7)) return;

	/* I like how the Linux kernel codes this part, I'll go ahead and borrow the style :) */
	in = sndsb_read_dsp(cx); if (in < 0) return;
	ess_ver = (uint16_t)in << 8;
	in = sndsb_read_dsp(cx); if (in < 0) return;
	ess_ver += (uint16_t)in;

	if ((ess_ver&0xFFF0) == 0x6880) {
		if (ess_ver & 8) { /* ESS 1869? I know how to program that! */
			cx->mixer_chip = SNDSB_MIXER_ESS688;
			cx->ess_chipset = SNDSB_ESS_1869;
			cx->ess_extensions = 1;

			/* NTS: The ESS 1869 and later have PnP methods to configure themselves, and the
			 * registers are documented as readonly for that reason, AND, on the ESS 1887 in
			 * the Compaq system I test, the 4-bit value that supposedly corresponds to IRQ
			 * doesn't seem to do anything. */

			/* The ESS 1869 (on the Compaq) appears to use the same 8-bit DMA for 16-bit as well.
			 * Perhaps the second DMA channel listed by the BIOS is the second channel (for full
			 * duplex?) */
			if (cx->dma8 >= 0 && cx->dma16 < 0)
				cx->dma16 = cx->dma8;
			if (cx->dma16 >= 0 && cx->dma8 < 0)
				cx->dma8 = cx->dma16;
		}
		else { /* ESS 688? I know how to program that! */
			cx->mixer_chip = SNDSB_MIXER_ESS688;
			cx->ess_chipset = SNDSB_ESS_688;
			cx->ess_extensions = 1;

			/* that also means that we can deduce the true IRQ/DMA from the chipset */
			if ((in=sndsb_ess_read_controller(cx,0xB1)) != -1) { /* 0xB1 Legacy Audio Interrupt Control */
				switch (in&0xF) {
					case 0x5:
						cx->irq = 5;
						break;
					case 0xA:
						cx->irq = 7;
						break;
					case 0xF:
						cx->irq = 10;
						break;
					case 0x0: /* "2,9,all others" */
						cx->irq = 9;
						break;
					default:
						break;
				}
			}
			if ((in=sndsb_ess_read_controller(cx,0xB2)) != -1) { /* 0xB2 DRQ Control */
				switch (in&0xF) {
					case 0x0:
						cx->dma8 = cx->dma16 = -1;
						break;
					case 0x5:
						cx->dma8 = cx->dma16 = 0;
						break;
					case 0xA:
						cx->dma8 = cx->dma16 = 1;
						break;
					case 0xF:
						cx->dma8 = cx->dma16 = 3;
						break;
					default:
						if (cx->dma8 >= 0 && cx->dma16 < 0)
							cx->dma16 = cx->dma8;
						if (cx->dma16 >= 0 && cx->dma8 < 0)
							cx->dma8 = cx->dma16;
						break;
				}
			}
		}

		/* TODO: 1869 datasheet recommends reading mixer index 0x40
		 *       four times to read back 0x18 0x69 A[11:8] A[7:0]
		 *       where A is the base address of the configuration
		 *       device. I don't have an ESS 1869 on hand to test
		 *       and dev that. Sorry. --J.C. */

		if (cx->ess_chipset != 0 && cx->dsp_copyright[0] == 0) {
			const char *s = sndsb_ess_chipset_str(cx->ess_chipset);
			if (s != NULL) strcpy(cx->dsp_copyright,s);
		}
	}
}

