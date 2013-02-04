/* NOTES:

   2011/10/28: Ah-ha! This code was having difficulty using 16-bit
      DMA channels because I somehow missed in Gravis documentation
      that the DMA start address is subject to 16-bit address translation,
      just like anything else related to 16-bit audio on the Gravis
      chipsets. The GF1 test/init routine is now able to use full
      GF1 capabilities and DMA properly when the card uses DMA channels
      5, 6, and 7

   Gravis Ultrasound Max:

       - I ran into an annoying H/W glitch while writing this code
         that is related to starting/stopping DMA transfers.
	 Apparently, if you first unmask the AT DMA channel, then
	 program the GUS DRAM addr and program in the settings
	 RIGHT OFF THE BAT, you will mostly get good transfers
	 but some transfers will have audible static (as if for
	 some odd reason the GUS is getting the DMA data but
	 NOT writing every 100th byte to it's RAM or something).

	 The workaround apparently is to cautiously start the DMA:
	   - Write 0x00 to DMA control to stop, THEN read to ack
	   - Program AT DMA controller, but do not unmask channel yet
	   - Write configuration to GUS DMA control, but do not yet
	     set the enable bit (bit 0)
	   - Write DRAM address to GUS DMA control
	   - Then, write DMA control again with "enable DMA" bit set
	   - Then, instruct the AT DMA controller to unmask the channel.

	 Following this sequence of steps resolves the staticy crackly
	 audio observed with a GUS Max on an ancient Pentium/100MHz
	 system of mine.

	    -- Jonathan C.


   Bidirectional looping misconception:

      - Each voice has a "current" position, a start, and an end.
        When the current position hits the end, the voice stops.
	Unless the voice is said to loop, then it goes back to start.
	A voice can also be marked as "bi-directional" which means
	that instead the voice's "backwards" bit is set and the
	current position heads back towards the start from the end.

	DOSBox 0.74 GUS emulation:
	   - When the current position passes start, go forward again.
	     Thus, setting start == 0 and end == len-1 makes the sample
	     loop properly.
	GUS MAX:
	   - When the current position was start, and less than the
	     previous position, go forward again. Note this makes it
	     impossible to ping-pong loop a sample starting at 0
	     (or 1 if 16-bit) because when start == 0 the GF1 goes right
	     past the point and into 0xFFF.. without changing
	     direction. To make it work, set start to 2 samples in.


    Proper voice programming sequence:

       The SDK doesn't say anything about this, but it seems that the
       best way to get the voice programmed the way you want without
       weird side effects, is to stop the voice, program the parameters,
       and then set the mode as the LAST STEP. If you set the mode, then
       set the positions and such, you risk the voice immediately going
       off backwards, or other side effects.

       Unfortuantely because of the way this code is written, you will
       need to set the mode twice. Once prior to programming, and then
       as the final step.


    Pre-initialized state and "stuck" IRQ: [BUG: FIXME]

       Apparently, if a previous program was using the GUS and left the
       timers active, or failed to clean up IRQ flags, our test routine
       will fail to trigger the IRQ with timers and fail the GUS. This
       can happen if you first run certain demoscene programs like "Dope"
       or "2nd Reality" then attempt to use this program.

       Someday when you have time, hunt down every possible way to
       a) forcibly stop the timers b) forcibly clear all pending IRQs
       and then have the code do just that before the timer test.

       Commenting out the timer test is not an option, since it would
       then be trivial for some other device to pass the RAM test.

       Note the Interwave SDK and Gravis documents mention a register
       with a bit you set to clear all pending IRQs at bootup. Maybe
       that would be of use?

       In the meantime, the solution is to either
         a) Run ULTRINIT from the ULTRASND directory
	 b) Run this program with no IRQ and DMA, play some audio,
	    then quit, and run again.


    DMA transfer programming:

       The GUS SDK shows that you can set a bit to get a DMA terminal
       count interrupt, meaning an IRQ when the DMA finishes. The
       actual GUS will set bit 6 (but not fire an IRQ) when the DMA
       transfer is complete. DOSBox will NOT set bit 6 when DMA TC
       complete, unless you asked for the IRQ. This is why this code
       checks both and terminates either then bit 6 is set, or the
       DMA controller indicates terminal count.


    DMA problem on real hardware with DMA channels 5, 6, and 7.  [BUG: FIXME]

       If your ultrasound is configured for high DMA channels, this
       code fails the DMA test and does not succeed in transferring
       the test message to GUS DRAM. Yet, high DMA channels work fine
       in DOSBox.

    Card identification [TODO: FIXME]

       The Gravis SDK mentions several registers that can be used to
       identify the card (if it is a GUS 3.7 or MAX or later). The
       GUS MAX has the ability to remove the legacy GUS Classic
       sample rate limits, which might be of use to you here.

       Also refer to the Linux driver source code on how to go about this process:

       http://www.cs.fsu.edu/~baker/devices/lxr/http/source/linux/sound/isa/gus/gus_dma.c


    SBOS initialization check [TODO: FIXME add to Sound Blaster library]

       Add code where if the DSP version comes back as 0xAA,0xAA, then
       do a quick check to see if the card is actually a GUS, and if so,
       fiddle with the "Sound Blaster emulation" registers to turn them
       off and reset the return value to 0x00 so that future SB testing
       does not see the GUS as SB.

 */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#include <hw/8237/8237.h>		/* 8237 DMA */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/ultrasnd/ultrasnd.h>

static int debug_on = 0;
static int ultrasnd_test_irq_fired = 0;
static int ultrasnd_test_irq_n = 0;

/* actual sample rate table (based on total voices) */
const uint16_t ultrasnd_rate_per_voices[33] = {
    44100, 44100, 44100, 44100,  44100, 44100, 44100, 44100,  	/* 0-7 */
    44100, 44100, 44100, 44100,  44100, 44100, 44100, 41160,  	/* 8-15 */
    38587, 36317, 34300, 32494,  30870, 29400, 28063, 26843,	/* 16-23 */
    25725, 24696, 23746, 22866,  22050, 21289, 20580, 19916,	/* 24-31 */
    19293 };							/* 32 */

struct ultrasnd_ctx ultrasnd_card[MAX_ULTRASND];
struct ultrasnd_ctx *ultrasnd_env = NULL;
int ultrasnd_inited = 0;

void ultrasnd_debug(int on) {
	debug_on = on;
}

uint32_t ultrasnd_dram_16bit_xlate(uint32_t a) {
	/* Ewwww... */
	return (a & 0x000C0000) | ((a & 0x3FFFF) >> 1UL);
}

uint32_t ultrasnd_dram_16bit_xlate_from(uint32_t a) {
	/* Ewwww... */
	return (a & 0xFFFC0000) | ((a & 0x1FFFF) << 1UL);
}

static void gus_delay(struct ultrasnd_ctx *u) {
	inp(u->port+0x102);
	inp(u->port+0x102);
	inp(u->port+0x102);
	inp(u->port+0x102);
	inp(u->port+0x102);
	inp(u->port+0x102);
	inp(u->port+0x102);
	inp(u->port+0x102);
	inp(u->port+0x102);
	inp(u->port+0x102);
	inp(u->port+0x102);
	inp(u->port+0x102);
}

void ultrasnd_select_voice(struct ultrasnd_ctx *u,uint8_t reg) {
	outp(u->port+0x102,reg);
	outp(u->port+0x102,reg); /* NTS: the "gravis ultrasound encyclopedia" recommends doing this... why? */
	outp(u->port+0x102,reg);
}

void ultrasnd_select_write(struct ultrasnd_ctx *u,uint8_t reg,uint8_t data) {
	outp(u->port+0x103,reg);
	outp(u->port+0x105,data);
	gus_delay(u);
	outp(u->port+0x105,data);
}

uint16_t ultrasnd_select_read16(struct ultrasnd_ctx *u,uint8_t reg) {
	outp(u->port+0x103,reg);
	return inpw(u->port+0x104);
}

void ultrasnd_select_write16(struct ultrasnd_ctx *u,uint8_t reg,uint16_t data) {
	outp(u->port+0x103,reg);
	outpw(u->port+0x104,data);
	gus_delay(u);
	outpw(u->port+0x104,data);
}

uint8_t ultrasnd_select_read(struct ultrasnd_ctx *u,uint8_t reg) {
	outp(u->port+0x103,reg);
	gus_delay(u);
	return inp(u->port+0x105);
}

uint8_t ultrasnd_selected_read(struct ultrasnd_ctx *u) {
	return inp(u->port+0x105);
}

unsigned char ultrasnd_peek(struct ultrasnd_ctx *u,uint32_t ofs) {
	if (u == NULL) return 0xFF;
	if (ofs & 0xFF000000UL) return 0xFF; /* The GUS only has 24-bit addressing */

	ultrasnd_select_write16(u,0x43,(uint16_t)ofs); /* 0x43: DRAM address low 16 bits */
	ultrasnd_select_write(u,0x44,(uint8_t)(ofs >> 16UL)); /* 0x44: DRAM address upper 8 bits */
	return inp(u->port+0x107);
}

void ultrasnd_poke(struct ultrasnd_ctx *u,uint32_t ofs,uint8_t b) {
	if (u == NULL) return;
	if (ofs & 0xFF000000UL) return; /* The GUS only has 24-bit addressing */

	ultrasnd_select_write16(u,0x43,(uint16_t)ofs); /* 0x43: DRAM address low 16 bits */
	ultrasnd_select_write(u,0x44,(uint8_t)(ofs >> 16UL)); /* 0x44: DRAM address upper 8 bits */
	outp(u->port+0x107,b);
}

int ultrasnd_valid_dma(struct ultrasnd_ctx *u,int8_t i) {
	switch (i) {
		case 1: return 1;
		case 3: return 2;
		case 5: return 3;
		case 6: return 4;
		case 7: return 5;
	};
	return 0;
}

int ultrasnd_valid_irq(struct ultrasnd_ctx *u,int8_t i) {
	switch (i) {
		case 2: return 1;
		case 3: return 3;
		case 5: return 2;
		case 7: return 4;
		case 11: return 5;
		case 12: return 6;
		case 15: return 7;
	};
	return 0;
}

static void interrupt far ultrasnd_test_irq() {
	ultrasnd_test_irq_fired++;

	/* ack PIC */
	if (ultrasnd_test_irq_n >= 8) p8259_OCW2(8,P8259_OCW2_NON_SPECIFIC_EOI);
	p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
}

void ultrasnd_set_active_voices(struct ultrasnd_ctx *u,unsigned char voices) {
	if (voices < 14) u->active_voices = 14;
	else if (voices > 32) u->active_voices = 32;
	else u->active_voices = voices;
	u->output_rate = ultrasnd_rate_per_voices[u->active_voices];
	ultrasnd_select_write(u,0x0E,0xC0 | (u->active_voices - 1));
}

/* NOTE: If possible, you are supposed to provide both IRQs and both DMA channels provided by ULTRASND,
 *       or given by a probing function. However this function also supports basic probing without any
 *       knowledge of what IRQ is involved. To do that, pass in the context with port number nonzero
 *       and IRQ and DMA channels set to -1 (unknown).
 *
 *       if program_cfg is nonzero, this routine will write the IRQ and DMA configuration registers. */
int ultrasnd_probe(struct ultrasnd_ctx *u,int program_cfg) {
	void (interrupt far *old_irq)() = NULL;
	unsigned char c1,c2,c2i,c;
	unsigned int i,patience;

	/* The "Gravis Ultrasound Programmers Encyclopedia" describes a probe function that only
	 * checks whether the onboard RAM can be peek'd and poke'd. Lame. Our test here goes
	 * further to ensure that it looks and acts like a Gravis Ultrasound */
	if (u == NULL) return 1;
	if (u->port < 0) return 1;
	if ((u->port&0xF) != 0) return 1;

	/* this card can only work with certain IRQs */
	if (u->irq1 >= 0 && ultrasnd_valid_irq(u,u->irq1) == 0) {
		if (debug_on) fprintf(stderr,"Gravis Ultrasound[0x%03x]: IRQ %u is not a valid GUS IRQ.\n",u->port);
		return 1;
	}
	if (u->irq2 >= 0 && ultrasnd_valid_irq(u,u->irq2) == 0) {
		if (debug_on) fprintf(stderr,"Gravis Ultrasound[0x%03x]: IRQ %u is not a valid GUS IRQ.\n",u->port,u->irq2);
		return 1;
	}

	/* reset the GF1 */
	ultrasnd_select_write(u,0x4C,0x00); /* 0x4C: reset register -> master reset (bit 0=0) disable DAC (bit 1=0) master IRQ disable (bit 2=0) */
	t8254_wait(t8254_us2ticks(20000)); /* wait 20ms for good measure, most docs imply 14 CPU cycles via IN statements but I don't think that's enough */
	c1 = ultrasnd_selected_read(u); /* FIXME: Reading the reg back immediately works correctly, but writing the selector register and reading back returns incorrect data? Is that DOSBox being weird or does the GUS actually do that? */

	/* FIXME: DOSBox's emulation implies that the timers fire the IRQ anyway even if we don't set bit 2. Is that what the actual GUS does or is DOSBox full of it on that matter? */
	c2i = 0x03 | (u->irq1 >= 0 ? 0x4 : 0x0); /* don't set bit 2 unless we intend to actually service the IRQ interrupt */
	ultrasnd_select_write(u,0x4C,c2i); /* 0x4C: reset register -> end reset (bit 0=1) enable DAC (bit 1=1) master IRQ enable (bit 2) */
	t8254_wait(t8254_us2ticks(20000)); /* wait 20ms for good measure, most docs imply 14 CPU cycles via IN statements but I don't think that's enough */
	c2 = ultrasnd_selected_read(u); /* FIXME: Reading the reg back immediately works correctly, but writing the selector register and reading back returns incorrect data? Is that DOSBox being weird or does the GUS actually do that? */

	if (c1 == 0xFF && c2 == 0xFF) {
		if (debug_on) fprintf(stderr,"Gravis Ultrasound[0x%03x]: Read back 0xFF during reset, so it's probably not there\n",u->port);
		return 1;
	}

	/* FIXME: The ultrasound SDK documentation never says anything about being able to read the registers.
	 *        The programmer's encyclopedia though implies you can. So then, can we hit the reset register
	 *        and read it back to know it's there? [DOSBox emulation: yes, we can!] */
	/* Did we really write Ultrasound register 0x4C or is it something else? */
	if ((c1&7) != 0 || (c2&7) != c2i) {
		/* HACK: DOSBox 0.74 emulates a GUS that happily leaves your bits set after reset.
		         The Real Thing however resets your bits as part of the reset. So what you
			 will likely read back is only one of the bits set, and you will need to
			 turn on the DAC by reading and writing again. */
		if (c1 == 0 && c2 == 1) {
			ultrasnd_select_write(u,0x4C,c2i); /* 0x4C: reset register -> end reset (bit 0=1) enable DAC (bit 1=1) master IRQ enable (bit 2) */
			t8254_wait(t8254_us2ticks(20000)); /* wait 20ms for good measure, most docs imply 14 CPU cycles via IN statements but I don't think that's enough */
			c2 = ultrasnd_selected_read(u); /* FIXME: Reading the reg back immediately works correctly, but writing the selector register and reading back returns incorrect data? Is that DOSBox being weird or does the GUS actually do that? */
			if ((c2&7) != c2i) {
				fprintf(stderr,"Gravis Ultrasound[0x%03x]: Reset register OK but DAC enable bit is stuck off, which means that your GUS is probably not going to output the audio we are playing. If UltraInit has been showing error messages you may want to check that the line/speaker output amplifier is OK. c1=0x%02x c2=%02x\n",u->port,c1,c2);
			}
		}
		else {
			if (debug_on) fprintf(stderr,"Gravis Ultrasound[0x%03x]: Reset register is supposed to be read/write. This is probably not a GUS. c1=0x%02x c2=%02x\n",u->port,c1,c2);
			return 1;
		}
	}

	/* the tests below can cause interrupts. so hook the IRQ */
	if (u->irq1 >= 0) {
		ultrasnd_test_irq_fired = 0;
		ultrasnd_test_irq_n = u->irq1;
		old_irq = _dos_getvect(irq2int(u->irq1));
		_dos_setvect(irq2int(u->irq1),ultrasnd_test_irq);
		/* perhaps a probing test prior to this call fired off the IRQ, but we failed to hook it because we didn't know. */
		_cli();
		p8259_OCW2(u->irq1,P8259_OCW2_SPECIFIC_EOI | (u->irq1&7));
		if (u->irq1 >= 8) p8259_OCW2(2,P8259_OCW2_SPECIFIC_EOI | 2); /* IRQ 8-15 -> IRQ 2 chaining */
		_sti();
		p8259_unmask(u->irq1);
		t8254_wait(t8254_us2ticks(1000)); /* 1ms */
		/* in case we did unmask an IRQ, it probably fired about now and bumped up our IRQ counter */
		_cli();
		if (ultrasnd_test_irq_fired != 0 && debug_on)
			fprintf(stderr,"Gravis Ultrasound[0x%03x]: IRQ %d was apparently stuck prior to this probe function, and I just got it un-stuck\n",u->port,u->irq1);
		ultrasnd_test_irq_fired = 0;
		_sti();
	}

	if (u->irq1 >= 0 && program_cfg) {
		/* now use the Mixer register to enable line out, and to select the IRQ control register.
		 * we can't actually read back the current configuration, because the registers are of
		 * course write only. but we can program them to match what we know about IRQ and DMA
		 * settings */
		outp(u->port+0x000,0x40 | 0x08);	/* bit 1: 0=enable line out, bit 0: 0=enable line in, bit 3: 1=enable latches  bit 6: 1=next I/O goes to IRQ latches */
		c1 = ultrasnd_valid_irq(u,u->irq1);
		if (u->irq2 == u->irq1) c1 |= 0x40; /* same IRQ */
		else c1 |= ultrasnd_valid_irq(u,u->irq2) << 3;
		outp(u->port+0x00B,c1);

		outp(u->port+0x000,0x00 | 0x08);	/* bit 1: 0=enable line out, bit 0: 0=enable line in, bit 3: 1=enable latches  bit 6: 0=next I/O goes to DMA latches */
		c1 = ultrasnd_valid_dma(u,u->dma1);
		if (u->dma2 == u->dma1) c1 |= 0x40; /* same IRQ */
		else c1 |= ultrasnd_valid_dma(u,u->dma2) << 3;
		outp(u->port+0x00B,c1);
	}
	else {
		/* just enable line out, do not program the IRQ and DMA settings */
		outp(u->port+0x000,0x00 | 0x08);	/* bit 1: 0=enable line out, bit 0: 0=enable line in, bit 3: 1=enable latches  bit 6: 1=next I/O goes to DMA latches */
	}

	/* clear pending or active DMA, in case the reset didn't do anything with it */
	ultrasnd_select_write(u,0x41,0x00);	/* disable any DMA */
	ultrasnd_select_read(u,0x41); /* read to clear DMA terminal count---even though we didn't ask for TC IRQ */

	/* stop all voices, in case the last program left them running
	   and firing off IRQs (like most MS-DOS GUS demos, apparently) */
	for (i=0;i < 32;i++) {
		ultrasnd_select_voice(u,i);
		ultrasnd_select_write(u,0x06,0);
		ultrasnd_select_write(u,0x07,0);
		ultrasnd_select_write(u,0x08,0);
		ultrasnd_select_write16(u,0x09,0);
		ultrasnd_select_write(u,0x00,3);
		ultrasnd_select_write(u,0x0D,0);
		ultrasnd_select_read(u,0x8D);
	}

	/* clear all IRQ events */
	patience = 5000;
	ultrasnd_select_voice(u,0);
	do {
		if (--patience == 0) {
			if (debug_on) fprintf(stderr,"GUS: Ran out of patience attempting to clear interrupt events\n");
			break;
		}

		_cli();
		if ((c1 = (ultrasnd_select_read(u,0x8F) & 0xC0)) != 0xC0) {
			ultrasnd_select_voice(u,c1&0x1F);
			ultrasnd_select_read(u,0x8D);
			ultrasnd_select_write(u,0x00,3);
			_sti();
		}
	} while (c1 != 0xC0);
	c1 = inp(u->port+0x006);
	_sti();

	/* any IRQs that did happen, ignore them */
	ultrasnd_test_irq_fired = 0;

	/* Gravis SDK also shows there are two timer registers. When loaded by the application they count up to 0xFF then generate an IRQ.
	 * So if we load them and no counting happens, it's not a GUS. */
	outp(u->port+0x008,0x04); /* select "timer stuff" */
	outp(u->port+0x009,0xE0); /* clear timer IRQ, mask off timer 1 & 2 */
	ultrasnd_select_write(u,0x45,0x00); /* disable timer 1 & 2 IRQ */
	ultrasnd_select_write(u,0x46,0x80); /* load timer 1 */
	ultrasnd_select_write(u,0x47,0x80); /* load timer 2 */
	ultrasnd_select_write(u,0x45,0x0C); /* enable timer 1 & 2 IRQ */
	outp(u->port+0x008,0x04); /* select "timer stuff" */
	outp(u->port+0x009,0x03); /* unmask timer IRQs, clear reset, start timers */

	if ((inp(u->port+0x006)&0xC) != 0)
		fprintf(stderr,"Gravis Ultrasound[0x%03x]: IRQ status register still shows pending IRQ for timers despite clearing and resetting them\n",u->port);
	if (u->irq1 >= 0 && ultrasnd_test_irq_fired != 0)
		fprintf(stderr,"Gravis Ultrasound[0x%03x]: IRQ fired at or before the timer was enabled. Did something else happen?\n",u->port,ultrasnd_test_irq_fired);

	/* wait 500ms, more than enough time for the timers to count up to 0xFF and signal IRQ */
	/* NTS: The SDK doc doesn't say, but apparently the timer will hit 0xFF, fire the IRQ, then reset to 0 and count up again (or else DOSBox is mis-emulating the GUS) */
	for (i=0,c1=0;i < 500 && (c1&0xC) != 0xC;i++) {
		t8254_wait(t8254_us2ticks(1000)); /* 1ms */
		c1 = inp(u->port+0x006); /* IRQ status */
	}
	ultrasnd_select_write(u,0x45,0x00); /* disable timer 1 & 2 IRQ */
	outp(u->port+0x008,0x04); /* select "timer stuff" */
	outp(u->port+0x009,0xE0); /* clear timer IRQ, mask off timer 1 & 2 */
	if ((c1&0xC) != 0xC) {
		if (debug_on) fprintf(stderr,"Gravis Ultrasound[0x%03x]: Timer readback fail 0x%02x\n",c1);
		goto timerfail;
	}
	if (u->irq1 >= 0 && ultrasnd_test_irq_fired == 0) {
		if (debug_on) fprintf(stderr,"Gravis Ultrasound[0x%03x]: Timer never signalled IRQ\n");
		goto irqfail;
	}

	/* wait for the timer to stop */
	patience = 100;
	do {
		t8254_wait(t8254_us2ticks(1000)); /* 1ms */
		c = inp(u->port+0x008);
		if (--patience == 0) {
			fprintf(stderr,"Gravis Ultrasound[0x%03x]: Timeout waiting for timers to stop\n",u->port);
			break;
		}
	} while ((c&0x60) == 0x00);

	/* check the RAM. is there at least a 4KB region we can peek and poke? */
	/* FIXME: According to Wikipedia there are versions of the card that don't have RAM at all (just ROM). How do we work with those? */
	for (i=0;i < 0x1000U;i++) ultrasnd_poke(u,i,(uint8_t)(i^0x55));
	for (i=0;i < 0x1000U;i++) if (ultrasnd_peek(u,i) != ((uint8_t)i^0x55)) goto ramfail;

	/* unless we know otherwise, samples cannot straddle a 256KB boundary */
	u->boundary256k = 1;

	/* initial versions came stock with 256KB with support for additional RAM in 256KB increments */
	u->total_ram = 256UL << 10UL; /* 256KB */
	for (i=1;i < ((8UL << 20UL) >> 18UL);i++) { /* test up to 8MB (8MB / 256KB banks) */
		uint32_t ofs = (uint32_t)i << (uint32_t)18UL;

		/* put sentry value at base to detect address wrapping */
		ultrasnd_poke(u,0,0x55);
		ultrasnd_poke(u,1,0xAA);

		/* now write a test value to the base of the 256KB bank */
		ultrasnd_poke(u,ofs,0x33);
		ultrasnd_poke(u,ofs+1,0x99);

		/* if the value at base changed, then address wrapping occurred
		 * and the test immediately after would falsely detect RAM there */
		if (ultrasnd_peek(u,0) != 0x55 || ultrasnd_peek(u,1) != 0xAA)
			break;

		/* if the value at the bank is not what we wrote, then RAM isn't there */
		if (ultrasnd_peek(u,ofs) != 0x33 || ultrasnd_peek(u,ofs+1) != 0x99)
			break;

		/* final test: write to the bytes at the end of the bank and see if they made it */
		ultrasnd_poke(u,ofs+0x3FFFE,0x11);
		ultrasnd_poke(u,ofs+0x3FFFF,0x55);
		if (ultrasnd_peek(u,ofs+0x3FFFE) != 0x11 || ultrasnd_peek(u,ofs+0x3FFFF) != 0x55)
			break;

		/* it's RAM. count it. move on */
		u->total_ram = ofs + 0x40000UL;
	}

	/* Next: the DMA test */
	if (u->dma1 >= 0) {
		static const char *testing = "Test message 1234 ABCD AaaababbabaCcocococo. If you can read this the DMA transfer worked.";
		unsigned char FAR *buffer,FAR *str;
		const unsigned int testlen = 512;

		if ((str=buffer=ultrasnd_dram_buffer_alloc(u,testlen)) != NULL) {
			char c;
			int i,len;
			unsigned int patience,rem;
			uint32_t phys = u->dram_xfer_a->phys;
			uint32_t lin = (uint32_t)u->dram_xfer_a->lin;

#if TARGET_MSDOS == 32
			memset(buffer,128,testlen);
#else
			_fmemset(buffer,128,testlen);
#endif

			_cli();

			/* Stop GUS DMA */
			ultrasnd_select_write(u,0x41,0x00);	/* disable any DMA */
			ultrasnd_select_read(u,0x41); /* read to clear DMA terminal count---even though we didn't ask for TC IRQ */

			/* clear GUS DRAM so that we're not comparing stale data */
			for (i=0;i < 0x2000;i++) ultrasnd_poke(u,i,0x00);

			/* Fill the buffer with a chosen phrase */
			len = strlen(testing)+1;
			for (i=0;i < len;i++) str[i] = testing[i];

			/* Now initiate a DMA transfer (host) */
			outp(d8237_ioport(u->dma1,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(u->dma1) | D8237_MASK_SET); /* mask */
			outp(d8237_ioport(u->dma1,D8237_REG_W_WRITE_MODE),
					D8237_MODER_CHANNEL(u->dma1) |
					D8237_MODER_TRANSFER(D8237_MODER_XFER_READ) | /* "READ" from system memory */
					D8237_MODER_MODESEL(D8237_MODER_MODESEL_DEMAND));
			d8237_write_base(u->dma1,u->dram_xfer_a->phys); /* RAM location with not much around */
			d8237_write_count(u->dma1,testlen);

			/* Now initiate a DMA transfer (GUS DRAM) */
			ultrasnd_select_write(u,0x41,(u->dma1 >= 4 ? 4 : 0)); /* data size in bit 2, writing to DRAM, enable DMA */
			if (u->dma1 >= 4) /* Ugh, even DMA is subject to Gravis 16-bit translation */
				ultrasnd_select_write16(u,0x42,ultrasnd_dram_16bit_xlate(0x1000)>>4);
			else
				ultrasnd_select_write16(u,0x42,0x1000>>4);
			ultrasnd_select_write(u,0x41,(u->dma1 >= 4 ? 4 : 0) | 0x1); /* data size in bit 2, writing to DRAM, enable DMA */

			/* GO! */
			outp(d8237_ioport(u->dma1,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(u->dma1)); /* unmask */

			/* wait for the GUS to indicate DMA terminal count */
			/* Note that DOSBox's emulation of a GUS does not indicate DMA TC, since we didn't ask for it, but the actual GF1 chipset
			 * will return with bit 6 set whether we asked for TC IRQ or not. */
			patience = 1000;
			do {
				rem = d8237_read_count(u->dma1);
				if (rem == 0 || rem >= 0xFFFE)
					break;

				t8254_wait(t8254_us2ticks(1000));
			} while (--patience != 0);
			if (rem >= 0xFFFE) rem = 0; /* the DMA counter might return 0xFFFF when terminal count reached */

			/* mask DMA channel again */
			outp(d8237_ioport(u->dma1,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(u->dma1) | D8237_MASK_SET); /* mask */

			/* stop DMA */
			ultrasnd_select_write(u,0x41,0x00);
			ultrasnd_select_read(u,0x41); /* read to clear DMA terminal count---even though we didn't ask for TC IRQ */

			_sti();
			ultrasnd_dram_buffer_free(u);

			if ((unsigned int)rem >= testlen) goto dmafail;
			else if (rem != 0) {
				fprintf(stderr,"Gravis Ultrasound[0x%03x]: DMA transfer is incomplete (%u/%u), probably stalled out during transfer. You should choose another DMA channel.\n",u->port,rem,testlen);
				goto dmafail;
			}

			/* OK then, did our message make it into DRAM? */
			for (i=0;i < len;i++) {
				c = ultrasnd_peek(u,i+0x1000);
				if (c != testing[i]) {
					fprintf(stderr,"Gravis Ultrasound[0x%03x]: DMA transfer corrupted test data (phys=%8lx lin=%8lx)\n",u->port,(unsigned long)phys,(unsigned long)lin);
					fprintf(stderr,"  I wrote: '%s'\n",testing);
					fprintf(stderr," Got back: '");
					for (i=0;i < len;i++) fprintf(stderr,"%c",ultrasnd_peek(u,i+0x1000));
					fprintf(stderr,"'\n");
					goto dmafail;
				}
			}
		}
		else {
			fprintf(stderr,"Gravis Ultrasound[0x%03x]: Unable to allocate buffer for DMA testing\n",u->port);
		}
	}

	/* read IRQ status register. failure to clear IRQ conditions is something to be concerned about */
	c1 = inp(u->port+0x006);
	if (c1 != 0x00) fprintf(stderr,"Gravis Ultrasound[0x%03x]: IRQ status register nonzero after init. The probe function missed something.\n",u->port);

	if (u->irq1 >= 0) {
		_dos_setvect(irq2int(u->irq1),old_irq);
		p8259_mask(u->irq1);
	}

	if (u->dma1 >= 0)
		u->use_dma = 1;
	else
		u->use_dma = 0;

	for (i=0;i < 32;i++) u->voicemode[i] = 0;
	ultrasnd_set_active_voices(u,14);
	return 0;
dmafail:
	fprintf(stderr,"Gravis Ultrasound[0x%03x]: DMA test failed, nothing was transferred\n",u->port);
	goto commoncleanup;
irqfail:
	fprintf(stderr,"Gravis Ultrasound[0x%03x]: IRQ test failed, timers did not signal IRQ handler\n",u->port);
	goto commoncleanup;
ramfail:
	fprintf(stderr,"Gravis Ultrasound[0x%03x]: RAM test failed\n",u->port);
	goto commoncleanup;
timerfail:
	fprintf(stderr,"Gravis Ultrasound[0x%03x]: Timer test failed (IRQstatus=0x%02X)\n",u->port,inp(u->port+0x006));
	goto commoncleanup;
commoncleanup:
	if (u->irq1 >= 0) {
		unsigned char irr,i;

		_dos_setvect(irq2int(u->irq1),old_irq);
		p8259_mask(u->irq1);

		/* problem: the GUS's IRQ acts like a level-triggered interrupt. if we don't
		 * take pains to clear it, it remains "stuck" and nobody else can signal that IRQ */

		ultrasnd_select_write(u,0x4C,0x00); /* 0x4C: reset register -> master reset (bit 0=0) disable DAC (bit 1=0) master IRQ disable (bit 2=0) */
		t8254_wait(t8254_us2ticks(20000)); /* wait 20ms for good measure, most docs imply 14 CPU cycles via IN statements but I don't think that's enough */
		ultrasnd_select_write(u,0x4C,0x01); /* 0x4C: reset register -> end reset (bit 0=1) enable DAC (bit 1=1) master IRQ enable (bit 2) */
		t8254_wait(t8254_us2ticks(20000)); /* wait 20ms for good measure, most docs imply 14 CPU cycles via IN statements but I don't think that's enough */

		/* clear pending or active DMA, in case the reset didn't do anything with it */
		ultrasnd_select_write(u,0x41,0x00);	/* disable any DMA */
		ultrasnd_select_read(u,0x41); /* read to clear DMA terminal count---even though we didn't ask for TC IRQ */

		/* Gravis SDK also shows there are two timer registers. When loaded by the application they count up to 0xFF then generate an IRQ.
		 * So if we load them and no counting happens, it's not a GUS. */
		outp(u->port+0x008,0x04); /* select "timer stuff" */
		outp(u->port+0x009,0xE0); /* clear timer IRQ, mask off timer 1 & 2 */
		ultrasnd_select_write(u,0x45,0x00); /* disable timer 1 & 2 IRQ */
		outp(u->port+0x008,0x04); /* select "timer stuff" */
		outp(u->port+0x009,0xE0); /* clear timer IRQ, mask off timer 1 & 2 */

		/* stop all voices, in case the last program left them running
		   and firing off IRQs (like most MS-DOS GUS demos, apparently) */
		for (i=0;i < 32;i++) {
			ultrasnd_select_voice(u,i);
			ultrasnd_select_write(u,0x06,0);
			ultrasnd_select_write(u,0x07,0);
			ultrasnd_select_write(u,0x08,0);
			ultrasnd_select_write16(u,0x09,0);
			ultrasnd_select_write(u,0x00,3);
			ultrasnd_select_write(u,0x0D,0);
			ultrasnd_select_read(u,0x8D);
		}

		/* clear all IRQ events */
		patience = 5000;
		ultrasnd_select_voice(u,0);
		do {
			if (--patience == 0) {
				if (debug_on) fprintf(stderr,"GUS: Ran out of patience attempting to clear interrupt events\n");
				break;
			}

			_cli();
			if ((c1 = (ultrasnd_select_read(u,0x8F) & 0xC0)) != 0xC0) {
				ultrasnd_select_voice(u,c1&0x1F);
				ultrasnd_select_read(u,0x8D);
				ultrasnd_select_write(u,0x00,3);
				_sti();
			}
		} while (c1 != 0xC0);

		/* suck up all possible IRQ events */
		for (i=0;i < 255;i++) c1 = inp(u->port+0x006); /* IRQ status */

		_sti();

		/* the IRQ might actually be waiting to be ACKed.
		 * if we don't take care of this, conflicts can happen, like the PLAYMP3
		 * program probing for Ultrasnd then SoundBlaster, and the un-acked IRQ
		 * prevents the Sound Blaster (on the same IRQ) from getting it's IRQ signal.
		 * This is more likely than you think considering DOSBox's default configuration */
		irr = (unsigned short)p8259_read_IRR(u->irq1);
		if (irr & (1 << (u->irq1 & 7))) {
			fprintf(stderr,"Gravis Ultrasound[0x%03x]: Stray IRQ signal\n",u->port);
			p8259_OCW2(u->irq1,P8259_OCW2_SPECIFIC_EOI|(u->irq1&7));
		}

		irr = (unsigned short)p8259_read_IRR(u->irq1);
		if (irr & (1 << (u->irq1 & 7))) {
			fprintf(stderr,"Gravis Ultrasound[0x%03x]: Stray IRQ signal...again!\n",u->port);
			p8259_OCW2(u->irq1,P8259_OCW2_SPECIFIC_EOI|(u->irq1&7));
		}
	}
	return 1;
}

int ultrasnd_irq_taken(int irq) {
	int i;

	for (i=0;i < MAX_ULTRASND;i++) {
		struct ultrasnd_ctx *u = &ultrasnd_card[i];
		if (u->irq1 == irq || u->irq2 == irq)
			return 1;
	}

	return 0;
}

int ultrasnd_dma_taken(int dma) {
	int i;

	for (i=0;i < MAX_ULTRASND;i++) {
		struct ultrasnd_ctx *u = &ultrasnd_card[i];
		if (u->dma1 == dma || u->dma2 == dma)
			return 1;
	}

	return 0;
}

int ultrasnd_port_taken(int port) {
	int i;

	for (i=0;i < MAX_ULTRASND;i++) {
		struct ultrasnd_ctx *u = &ultrasnd_card[i];
		if (u->port == port)
			return 1;
	}

	return 0;
}

void ultrasnd_init_ctx(struct ultrasnd_ctx *u) {
	u->port = -1;
	u->use_dma = 0;
	u->dma1 = u->dma2 = -1;
	u->irq1 = u->irq2 = -1;
	u->dram_xfer_a = NULL;
}

void ultrasnd_free_card(struct ultrasnd_ctx *u) {
	ultrasnd_dram_buffer_free(u);
	ultrasnd_init_ctx(u);
	if (u == ultrasnd_env) ultrasnd_env = NULL;
}

int ultrasnd_card_taken(struct ultrasnd_ctx *u) {
	return (u->port >= 0);
}

int init_ultrasnd() {
	int i;

	if (!ultrasnd_inited) {
		ultrasnd_inited = 1;
		for (i=0;i < MAX_ULTRASND;i++)
			ultrasnd_init_ctx(&ultrasnd_card[i]);
	}

	return 1;
}

struct ultrasnd_ctx *ultrasnd_alloc_card() {
	int i;

	for (i=0;i < MAX_ULTRASND;i++) {
		if (!ultrasnd_card_taken(&ultrasnd_card[i]))
			return &ultrasnd_card[i];
	}

	return NULL;
}

static void interrupt far dummy_irq_hi() {
	p8259_OCW2(8,P8259_OCW2_NON_SPECIFIC_EOI);
	p8259_OCW2(2,P8259_OCW2_NON_SPECIFIC_EOI);
}

static void interrupt far dummy_irq() {
	p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
}

/* updates a bitmask on what IRQs have fired on their own (and therefore are not fired by the GUS).
 * also returns a bitmask of "stuck" interrupts, which is important given my comments below on
 * DOSBox and the GUS's persistent IRQ signal once timers have been triggered */
static unsigned short ultrasnd_irq_watch(unsigned short *eirq) {
	void far *oldvec[16] = {NULL}; /* Apparently some systems including DOSBox have masked IRQs pointing to NULL interrupt vectors. So to properly test we have to put a dummy there. Ewwwww... */
	unsigned short irr,stuck = 0,irq,intv;
	unsigned int patience = 0;
	unsigned char ml,mh;
#if TARGET_MSDOS == 32
	uint32_t *intvec = (uint32_t*)0x00000000;
#else
	uint32_t far *intvec = (uint32_t far *)MK_FP(0x0000,0x000);
#endif

	for (irq=0;irq < 16;irq++) {
		intv = irq2int(irq);
		/* read the interrupt table directly. assume the DOS extender (if 32-bit) would translate getvect() and cause problems */
		if (intvec[intv] == 0x00000000UL) {
			oldvec[irq] = _dos_getvect(intv);
			if (irq >= 8) _dos_setvect(intv,dummy_irq_hi);
			else _dos_setvect(intv,dummy_irq);
		}
		else {
			oldvec[irq] = NULL;
		}
	}

	/* BUG: DOSBox's emulation (and possibly the real thing) fire the IRQ
	 *      for the timer even if we don't set Master IRQ enable. This
	 *      results in the IRQ being stuck active but masked off. So what
	 *      we have to do is unmask the IRQ long enough for it to fire,
	 *      then re-mask it for the activity test. Doing this also means that
	 *      if the IRQ was active for something else, then that something
	 *      else will have it's interrupt serviced properly. Else, if it
	 *      was the GUS, then this lets the PIC get the IRQ signal out of
	 *      it's way to allow the GUS to fire again. */

	_cli();
	ml = p8259_read_mask(0);	/* IRQ0-7 */
	mh = p8259_read_mask(8);	/* IRQ8-15 */
	p8259_write_mask(0,0x00);	/* unmask all interrupts, give the IRQ a chance to fire. keep a record of any IRQs that remain stuck */
	p8259_write_mask(8,0x00);
	_sti();
	stuck = 0xFFFF;
	for (patience=0;patience < 250;patience++) {
		t8254_wait(t8254_us2ticks(1000));
		_cli();
		irr  = (unsigned short)p8259_read_IRR(0);
		irr |= (unsigned short)p8259_read_IRR(8) << 8U;
		_sti();
		stuck &= irr;
	}
	/* issue a specific EOI for any interrupt that remains stuck and unserviced */
	for (irq=2;irq <= 15;irq++) {
		if (irq == 2 || irq == 9) {
			/* do not disturb the IRQ 8-15 -> IRQ 2 chain or IRQ 9 weirdness. GUS cards don't use IRQ 9 anyway. */
		}
		else if (stuck & (1U << irq)) {
			_cli();
			p8259_OCW2(irq,P8259_OCW2_SPECIFIC_EOI | (irq&7));
			if (irq >= 8) p8259_OCW2(2,P8259_OCW2_SPECIFIC_EOI | 2); /* IRQ 8-15 -> IRQ 2 chaining */
			_sti(); /* then give the EOI'd IRQ a chance to clear */
		}
	}
	if (eirq != NULL) {
		_cli();
		/* now carry out the test and note interrupts as they happen */
		p8259_write_mask(0,0xFF);	/* mask off all interrupts */
		p8259_write_mask(8,0xFF);
		patience = 250;
		do {
			t8254_wait(t8254_us2ticks(1000));

			irr  = (unsigned short)p8259_read_IRR(0);
			irr |= (unsigned short)p8259_read_IRR(8) << 8U;
			for (irq=0;irq < 16;irq++) {
				if (stuck & (1U << irq)) {
				}
				else if (irr & (1U << irq)) {
					*eirq |= 1U << irq;
				}
			}
		} while (--patience != 0);
	}
//	fprintf(stdout,"Pre-test IRQ elimination: 0x%04X irr=0x%04x stuck=0x%04x\n",*eirq,irr,stuck);

	/* restore interrupt mask */
	p8259_write_mask(0,ml);
	p8259_write_mask(8,mh);
	_sti();

	for (irq=0;irq < 16;irq++) {
		if (oldvec[irq] != NULL) {
			intv = irq2int(irq);
			intvec[intv] = 0x00000000UL;
		}
	}

	return stuck;
}

int ultrasnd_test_irq_timer(struct ultrasnd_ctx *u,int irq) {
	unsigned char c1;
	unsigned int i;

	i  = (unsigned short)p8259_read_IRR(0);
	i |= (unsigned short)p8259_read_IRR(8) << 8U;
	if (i & (1 << irq)) {
		if (debug_on) fprintf(stderr,"Gravis Ultrasound[0x%03x]: IRQ timer test cannot work, IRQ %u already fired\n",u->port,irq);
		return 0;
	}

	_cli();
	p8259_mask(irq);

	/* Gravis SDK also shows there are two timer registers. When loaded by the application they count up to 0xFF then generate an IRQ.
	 * So if we load them and no counting happens, it's not a GUS. */
	outp(u->port+0x008,0x04); /* select "timer stuff" */
	outp(u->port+0x009,0xE0); /* clear timer IRQ, mask off timer 1 & 2 */
	ultrasnd_select_write(u,0x45,0x00); /* disable timer 1 & 2 IRQ */
	ultrasnd_select_write(u,0x46,0xFE); /* load timer 1 */
	ultrasnd_select_write(u,0x47,0xFE); /* load timer 2 */
	ultrasnd_select_write(u,0x45,0x0C); /* enable timer 1 & 2 IRQ */
	outp(u->port+0x008,0x04); /* select "timer stuff" */
	outp(u->port+0x009,0x03); /* unmask timer IRQs, clear reset, start timers */

	/* wait 50ms, more than enough time for the timers to count up to 0xFF and signal IRQ */
	for (i=0;i < 500 && (c1&0xC) != 0xC;i++) {
		t8254_wait(t8254_us2ticks(1000)); /* 1ms */
		c1 = inp(u->port+0x006); /* IRQ status */
	}
	ultrasnd_select_write(u,0x45,0x00); /* disable timer 1 & 2 IRQ */
	outp(u->port+0x008,0x04); /* select "timer stuff" */
	outp(u->port+0x009,0xE0); /* clear timer IRQ, mask off timer 1 & 2 */
	if ((c1&0xC) != 0xC) {
		if (debug_on) fprintf(stderr,"Gravis Ultrasound[0x%03x]: IRQ timer test failed, timers did not complete (0x%02x)\n",u->port,c1);
		return 0;
	}

	i  = (unsigned short)p8259_read_IRR(0);
	i |= (unsigned short)p8259_read_IRR(8) << 8U;
	if (!(i & (1 << irq))) {
//		fprintf(stderr,"Gravis Ultrasound[0x%03x]: IRQ timer test failed, IRQ %d did not fire\n",u->port,irq);
		return 0;
	}

	return 1;
}

struct ultrasnd_ctx *ultrasnd_try_base(uint16_t base) {
	unsigned char irq_tries[] = {2,3,5,7,11,12,15},tri;
	unsigned short eliminated_irq = 0U,stuck,iv,i,cc;
	struct ultrasnd_ctx *u;

	if (ultrasnd_port_taken(base))
		return NULL;
	if ((u = ultrasnd_alloc_card()) == NULL)
		return NULL;

	ultrasnd_init_ctx(u);
	u->port = base;

	if (u->irq1 < 0) {
		/* prior to probing, take note of IRQs that fire without help from the GUS.
		 * ignore return value (stuck IRQs) */
		ultrasnd_irq_watch(&eliminated_irq);
	}

	if (ultrasnd_probe(u,0)) {
		ultrasnd_free_card(u);
		return NULL;
	}

	if (u->irq1 < 0) {
		/* FIXME: DOSBox 0.74: This code hangs somewhere in here if auto-detecting a GUS configured on IRQ 11, or 15.
		 *        Update: The vector patching code in the watch function fixes it for the most part---but if you run
		 *        the real-mode version and then the protected mode version, the IRQ hang will occur again. */
		for (cc=0;u->irq1 < 0 && cc < 2;cc++) {
			/* probe again, updating the eliminated mask but also noting which IRQs are stuck.
			 * if DOSBox's emulation is correct, the GUS will persist in it's IRQ signalling
			 * because of our timer testing and the stuck IRQ might be the one we're looking for. */
			stuck = ultrasnd_irq_watch(NULL);
			if (stuck == 0)
				break;
			else if ((stuck & (stuck - 1)) == 0) { /* if only 1 bit set */
				i=0;
				for (iv=0;iv < 16;iv++) {
					if (stuck & (1 << iv)) {
						i = iv;
						break;
					}
				}

				if ((eliminated_irq & (1 << iv)) == 0) {
					if (ultrasnd_valid_irq(u,i) != 0 && ultrasnd_test_irq_timer(u,i))
						u->irq1 = i;
					else
						eliminated_irq |= 1 << iv;

					stuck = ultrasnd_irq_watch(NULL);
				}
			}
			else {
				break; /* we'll have to try another way */
			}
		}

		for (tri=0;u->irq1 < 0 && tri < sizeof(irq_tries);tri++) {
			if (eliminated_irq & (1 << irq_tries[tri]))
				continue;

			if (ultrasnd_valid_irq(u,irq_tries[tri]) != 0 && ultrasnd_test_irq_timer(u,irq_tries[tri]))
				u->irq1 = irq_tries[tri];
			else
				eliminated_irq |= 1 << irq_tries[tri];

			ultrasnd_irq_watch(NULL);
		}
		ultrasnd_irq_watch(NULL);
	}

	if (u->dma1 < 0) {
		/* TODO: DMA channel probing code */
	}

	return u;
}

/* try initializing a card based on the ULTRASND env variable */
struct ultrasnd_ctx *ultrasnd_try_ultrasnd_env() {
	struct ultrasnd_ctx *u;
	int val[6],par=0;
	char *p;

	if ((p = getenv("ULTRASND")) == NULL)
		return NULL;
	if ((u = ultrasnd_alloc_card()) == NULL)
		return NULL;

	ultrasnd_init_ctx(u);
	for (par=0;par < 6;par++) val[par] = -1;
	/* ULTRASND=<port>,<play DMA>,<rec DMA>,<GUS IRQ>,<SB MIDI IRQ> */
	par = 0;
	while (*p) {
		if (*p == ' ') {
			p++;
			continue;
		}

		if (isdigit(*p)) {
			if (par == 0) { /* port num, hex */
				val[par] = (int)strtol(p,NULL,16); /* hexadecimal */
			}
			else {
				val[par] = (int)strtol(p,NULL,0); /* decimal */
			}
		}
		else {
			val[par] = -1;
		}

		while (*p && *p != ',') p++;
		if (*p == ',') p++;
		if (++par >= 6) break;
	}

	if (ultrasnd_port_taken(val[0]))
		return NULL;

	if (val[0] >= 0x200)
		u->port = val[0];
	else
		return NULL;

	if (val[1] >= 0)
		u->dma1 = val[1];
	if (val[2] >= 0)
		u->dma2 = val[2];
	if (val[3] >= 0)
		u->irq1 = val[3];
	if (val[4] >= 0)
		u->irq2 = val[4];

	return (ultrasnd_env=u);
}

uint16_t ultrasnd_sample_rate_to_fc(struct ultrasnd_ctx *u,unsigned int r) {
	uint32_t m;

	/* "frequency counter" is 6.8 fixed point */
	m = ((1UL << 9UL) * (unsigned long)r) / (unsigned long)u->output_rate;
	return (uint16_t)m << 1U;
}

unsigned char ultrasnd_read_voice_mode(struct ultrasnd_ctx *u,unsigned char voice) {
	ultrasnd_select_voice(u,voice);
	return ultrasnd_select_read(u,0x80);
}

void ultrasnd_set_voice_mode(struct ultrasnd_ctx *u,unsigned char voice,uint8_t mode) {
	ultrasnd_select_voice(u,voice); mode &= ~3;
	mode |= ultrasnd_select_read(u,0x00) & 3;

	ultrasnd_select_voice(u,voice);
	ultrasnd_select_write(u,0x00,mode);

	t8254_wait(t8254_us2ticks(100)); /* 100us */

	ultrasnd_select_voice(u,voice);
	ultrasnd_select_write(u,0x00,mode);
	u->voicemode[voice] = mode;
}

void ultrasnd_set_voice_fc(struct ultrasnd_ctx *u,unsigned char voice,uint16_t fc) {
	ultrasnd_select_voice(u,voice);
	ultrasnd_select_write16(u,0x01,fc);
}

void ultrasnd_set_voice_start(struct ultrasnd_ctx *u,unsigned char voice,uint32_t ofs) {
	if (u->voicemode[voice] & ULTRASND_VOICE_MODE_16BIT) ofs = ultrasnd_dram_16bit_xlate(ofs);
	ultrasnd_select_voice(u,voice);
	ultrasnd_select_write16(u,0x02,ofs >> 7UL);
	ultrasnd_select_write16(u,0x03,ofs << 9UL);
}

void ultrasnd_set_voice_end(struct ultrasnd_ctx *u,unsigned char voice,uint32_t ofs) {
	if (u->voicemode[voice] & ULTRASND_VOICE_MODE_16BIT) ofs = ultrasnd_dram_16bit_xlate(ofs);
	ultrasnd_select_voice(u,voice);
	ultrasnd_select_write16(u,0x04,ofs >> 7UL);
	ultrasnd_select_write16(u,0x05,ofs << 9UL);
}

void ultrasnd_stop_voice(struct ultrasnd_ctx *u,int i) {
	unsigned char c;
	int j;

	ultrasnd_select_voice(u,i);
	for (j=0;j < 16;j++) {
		c = ultrasnd_select_read(u,0x80); /* voice select (read) */
		ultrasnd_select_write(u,0x00,c | 3); /* voice select (write), set STOP VOICE */
		t8254_wait(t8254_us2ticks(100)); /* 100us */
		c = ultrasnd_select_read(u,0x80); /* voice select (read) */
		ultrasnd_select_write(u,0x00,c | 3); /* voice select (write), set STOP VOICE */
		t8254_wait(t8254_us2ticks(100)); /* 100us */
		c = ultrasnd_select_read(u,0x80); /* voice select (read) */
		if (c&1) break; /* if the voice stopped, then we succeeded */
	}

//	if (!(c&1)) fprintf(stderr,"Cannot stop voice %02x\n",c);
	u->voicemode[i] = c;
}

void ultrasnd_start_voice(struct ultrasnd_ctx *u,int i) {
	unsigned char c;
	int j;

	ultrasnd_select_voice(u,i);
	for (j=0;j < 16;j++) {
		c = ultrasnd_select_read(u,0x80); /* voice select (read) */
		ultrasnd_select_write(u,0x00,c & (~3)); /* voice select (write), clear STOP VOICE */
		t8254_wait(t8254_us2ticks(100)); /* 100us */
		c = ultrasnd_select_read(u,0x80); /* voice select (read) */
		ultrasnd_select_write(u,0x00,c & (~3)); /* voice select (write), clear STOP VOICE */
		t8254_wait(t8254_us2ticks(100)); /* 100us */
		c = ultrasnd_select_read(u,0x80); /* voice select (read) */
		if (!(c&1)) break; /* if the voice started, then we succeeded */
	}

//	if (c&1) fprintf(stderr,"Cannot start voice %02x\n",c);
	u->voicemode[i] = c;
}

void ultrasnd_start_voice_imm(struct ultrasnd_ctx *u,int i) {
	unsigned char c;

	ultrasnd_select_voice(u,i);
	c = ultrasnd_select_read(u,0x80); /* voice select (read) */
	ultrasnd_select_write(u,0x00,c & (~3)); /* voice select (write), clear STOP VOICE */
	u->voicemode[i] = c;
}

void ultrasnd_set_voice_ramp_rate(struct ultrasnd_ctx *u,unsigned char voice,unsigned char adj,unsigned char rate) {
	ultrasnd_select_voice(u,voice);
	ultrasnd_select_write(u,0x06,(adj & 0x3F) | (rate << 6));
}

void ultrasnd_set_voice_ramp_start(struct ultrasnd_ctx *u,unsigned char voice,unsigned char start) {
	ultrasnd_select_voice(u,voice);
	ultrasnd_select_write(u,0x07,start);
}

void ultrasnd_set_voice_ramp_end(struct ultrasnd_ctx *u,unsigned char voice,unsigned char end) {
	ultrasnd_select_voice(u,voice);
	ultrasnd_select_write(u,0x08,end);
}

void ultrasnd_set_voice_volume(struct ultrasnd_ctx *u,unsigned char voice,uint16_t vol) {
	ultrasnd_select_voice(u,voice);
	ultrasnd_select_write16(u,0x09,vol);
}

uint32_t ultrasnd_read_voice_current(struct ultrasnd_ctx *u,unsigned char voice) {
	uint32_t ofs;
	uint16_t a,b;

	/* nobody says or implies that the GUS keeps the halves buffered while
	   we read, so assume the worst case scenario and re-read if we see
	   the upper half change. */
	ultrasnd_select_voice(u,voice);
	do {
		a = ultrasnd_select_read16(u,0x8A);
		ofs = ultrasnd_select_read16(u,0x8B) >> 9UL;
		b = ultrasnd_select_read16(u,0x8A);
	} while (a != b);
	ofs |= (uint32_t)a << 7UL;
	ofs &= 0xFFFFF;

	/* NOTE TO SELF: As seen on GUS MAX ISA card: Bits 22-20 are set for some reason.
	                 And our attempts to play forwards ends up in reverse? */
	if (u->voicemode[voice] & ULTRASND_VOICE_MODE_16BIT) ofs = ultrasnd_dram_16bit_xlate_from(ofs);
	return ofs;
}

void ultrasnd_set_voice_current(struct ultrasnd_ctx *u,unsigned char voice,uint32_t loc) {
	loc &= 0xFFFFF;
	if (u->voicemode[voice] & ULTRASND_VOICE_MODE_16BIT) loc = ultrasnd_dram_16bit_xlate(loc);
	ultrasnd_select_voice(u,voice);
	ultrasnd_select_write16(u,0x0A,loc >> 7UL);
	ultrasnd_select_write16(u,0x0B,loc << 9UL);
}

void ultrasnd_set_voice_pan(struct ultrasnd_ctx *u,unsigned char voice,uint8_t pan) {
	ultrasnd_select_voice(u,voice);
	ultrasnd_select_write(u,0x0C,pan);
}

void ultrasnd_set_voice_ramp_control(struct ultrasnd_ctx *u,unsigned char voice,uint8_t ctl) {
	ultrasnd_select_voice(u,voice);
	ultrasnd_select_write(u,0x0D,ctl);
}

unsigned char FAR *ultrasnd_dram_buffer_alloc(struct ultrasnd_ctx *u,unsigned long len) {
	if (len >= 0xFF00UL) {
		ultrasnd_dram_buffer_free(u);
		return NULL;
	}

	if (u->dram_xfer_a != NULL) {
		if (len <= u->dram_xfer_a->length) return u->dram_xfer_a->lin;
		ultrasnd_dram_buffer_free(u);
	}

	if ((u->dram_xfer_a = dma_8237_alloc_buffer(len)) == NULL)
		return NULL;

	return u->dram_xfer_a->lin;
}

int ultrasnd_send_dram_buffer(struct ultrasnd_ctx *u,uint32_t ofs,unsigned long len,uint8_t flags) {
	unsigned char FAR *src;
	uint8_t dma = u->use_dma;
	unsigned int patience;
	uint32_t phys,i;
	uint16_t rem;

	if (u == NULL || u->dram_xfer_a == NULL || len > u->dram_xfer_a->length || len > 0xFF00UL)
		return 0;

	src = u->dram_xfer_a->lin;
	phys = u->dram_xfer_a->phys;

	/* cannot do half-word transfers when 16-bit is involved */
	if (dma && ((len & 1) && ((u->dma1 >= 4) || (flags & ULTRASND_DMA_DATA_SIZE_16BIT))))
		dma = 0;
	/* the target DRAM address must be 16-bit aligned (32-bit aligned for 16-bit audio) */
	if (dma && (ofs & 0xF) != 0)
		dma = 0;
	if (dma && u->dma1 >= 4 && (ofs & 0x1F) != 0)
		dma = 0;

	if (dma) {
		_cli();

		/* disable GUS DMA */
		ultrasnd_select_write(u,0x41,0x00);
		ultrasnd_select_read(u,0x41); /* read to clear DMA terminal count---even though we didn't ask for TC IRQ */

		/* Now initiate a DMA transfer (host) */
		outp(d8237_ioport(u->dma1,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(u->dma1) | D8237_MASK_SET); /* mask */
		outp(d8237_ioport(u->dma1,D8237_REG_W_WRITE_MODE),
			D8237_MODER_CHANNEL(u->dma1) |
			D8237_MODER_TRANSFER(D8237_MODER_XFER_READ) | /* "READ" from system memory */
			D8237_MODER_MODESEL(D8237_MODER_MODESEL_DEMAND));
		d8237_write_base(u->dma1,phys); /* RAM location with not much around */
		d8237_write_count(u->dma1,len);

		/* Now initiate a DMA transfer (GUS DRAM) */
		ultrasnd_select_write(u,0x41,(u->dma1 >= 4 ? 4 : 0) | (flags & 0xC0)); /* data size in bit 2, writing to DRAM, enable DMA, and bits 6-7 provided by caller */
		if (u->dma1 >= 4) /* Ugh, even DMA is subject to Gravis 16-bit translation */
			ultrasnd_select_write16(u,0x42,(uint16_t)(ultrasnd_dram_16bit_xlate(ofs)>>4UL));
		else
			ultrasnd_select_write16(u,0x42,(uint16_t)(ofs>>4UL));
		ultrasnd_select_write(u,0x41,(u->dma1 >= 4 ? 4 : 0) | 0x1 | (flags & 0xC0)); /* data size in bit 2, writing to DRAM, enable DMA, and bits 6-7 provided by caller */

		/* GO! */
		outp(d8237_ioport(u->dma1,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(u->dma1)); /* unmask */

		_sti();

		/* watch it run */
		/* Note that DOSBox's emulation of a GUS does not indicate DMA TC, since we didn't ask for it, but the actual GF1 chipset
		 * will return with bit 6 set whether we asked for TC IRQ or not. */
		patience = 10000; /* 100ns * 10000 = 1 sec */
		do {
			rem = d8237_read_count(u->dma1);
			if (rem == 0 || rem >= 0xFFFE)
				break;

			t8254_wait(t8254_us2ticks(100));
		} while (--patience != 0);
		if (rem >= 0xFFFE) rem = 0;

		if (patience == 0)
			fprintf(stderr,"GUS DMA transfer timeout (rem=%lu)\n",rem);
		if (rem != 0)
			fprintf(stderr,"GUS DMA transfer TC while DMA controller has %u remaining\n",rem);

		/* mask DMA channel again */
		outp(d8237_ioport(u->dma1,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(u->dma1) | D8237_MASK_SET); /* mask */

		/* stop DMA */
		ultrasnd_select_write(u,0x41,0x00);
		ultrasnd_select_read(u,0x41); /* read to clear DMA terminal count---even though we didn't ask for TC IRQ */
	}
	else {
		if (flags & ULTRASND_DMA_FLIP_MSB) {
			if (flags & ULTRASND_DMA_DATA_SIZE_16BIT) {
				for (i=0;i < len;i += 2) {
					ultrasnd_poke(u,ofs+i,src[i]);
					ultrasnd_poke(u,ofs+i+1,src[i+1] ^ 0x80);
				}
			}
			else {
				for (i=0;i < len;i++) ultrasnd_poke(u,ofs+i,src[i] ^ 0x80);
			}
		}
		else {
			for (i=0;i < len;i++) ultrasnd_poke(u,ofs+i,src[i]);
		}
	}

	return 1;
}

void ultrasnd_dram_buffer_free(struct ultrasnd_ctx *u) {
	if (u->dram_xfer_a != NULL) {
		dma_8237_free_buffer(u->dram_xfer_a);
		u->dram_xfer_a = NULL;
	}
}

