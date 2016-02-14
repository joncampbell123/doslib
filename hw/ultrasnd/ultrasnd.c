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

/* actual sample rate table (based on total voices).
 * Based on observed Gravis Ultrasound MAX behavior. */
const uint32_t ultrasnd_rate_per_voices[33] = {
    0UL,     205800UL,154350UL,205800UL, 154350UL,123480UL,102900UL,88200UL,  	/* 0-7 */
    77175UL, 68600UL, 61740UL, 56127UL,  51450UL, 47492UL, 44100UL, 41160UL,  	/* 8-15 */
    38587UL, 36317UL, 34300UL, 32494UL,  30870UL, 29400UL, 28063UL, 26843UL,	/* 16-23 */
    25725UL, 24696UL, 23746UL, 22866UL,  22050UL, 21289UL, 20580UL, 19916UL,	/* 24-31 */
    19293UL };									/* 32 */

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

static struct ultrasnd_ctx *ultrasnd_test_card = NULL;

static void interrupt far ultrasnd_test_irq() {
	ultrasnd_test_irq_fired++;

	/* ack PIC */
	if (ultrasnd_test_irq_n >= 8) p8259_OCW2(8,P8259_OCW2_NON_SPECIFIC_EOI);
	p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
}

void ultrasnd_set_active_voices(struct ultrasnd_ctx *u,unsigned char voices) {
	if (voices < 1) u->active_voices = 1;
	else if (voices > 32) u->active_voices = 32;
	else u->active_voices = voices;
	u->output_rate = ultrasnd_rate_per_voices[u->active_voices];
	ultrasnd_select_write(u,0x0E,0xC0 | (u->active_voices - 1));
}

void ultrasnd_abort_dma_transfer(struct ultrasnd_ctx *u) {
	/* clear pending or active DMA, in case the reset didn't do anything with it */
	ultrasnd_select_write(u,0x41,0x00);	/* disable any DMA */
	ultrasnd_select_read(u,0x41); /* read to clear DMA terminal count---even though we didn't ask for TC IRQ */
}

void ultrasnd_stop_timers(struct ultrasnd_ctx *u) {
	unsigned int patience;

	patience = 16;
	do {
		outp(u->port+0x008,0x04); /* select "timer stuff" */
		outp(u->port+0x009,0x00); /* turn off timer 1 & 2 */
		outp(u->port+0x009,0x60); /* mask timer 1 & 2 */
		outp(u->port+0x009,0xE0); /* clear timer IRQ */
		outp(u->port+0x009,0x80); /* clear timer IRQ */
	} while (--patience != 0 && (inp(u->port+0x008) & 0xE0) != 0);

	ultrasnd_select_write(u,0x45,0x0C); /* enable timer 1 & 2 IRQ */
	ultrasnd_select_write(u,0x46,0x00); /* reset */
	ultrasnd_select_write(u,0x47,0x00); /* reset */
	ultrasnd_select_write(u,0x45,0x00); /* disable timer 1 & 2 IRQ */
}

void ultrasnd_stop_all_voices(struct ultrasnd_ctx *u) {
	unsigned char c;
	unsigned int i;

	/* stop all voices, in case the last program left them running
	   and firing off IRQs (like most MS-DOS GUS demos, apparently) */
	for (i=0;i < 32;i++) {
		ultrasnd_select_voice(u,i);
		ultrasnd_select_write(u,0x00,3); /* force stop the voice */
		ultrasnd_select_write(u,0x00,3); /* force stop the voice */
		ultrasnd_select_write(u,0x06,0);
		ultrasnd_select_write(u,0x07,0);
		ultrasnd_select_write(u,0x08,0);
		ultrasnd_select_write16(u,0x09,0);
		ultrasnd_select_write(u,0x00,3);
		ultrasnd_select_write(u,0x0D,0);
		ultrasnd_select_read(u,0x8D); /* clear volume ramp IRQ pending */
	}
	ultrasnd_select_voice(u,0);

	/* forcibly clear any pending voice IRQs */
	i=0;
	do { c = ultrasnd_select_read(u,0x8F);
	} while ((++i) < 256 && (c&0xC0) != 0xC0);
}

/* NOTE: If possible, you are supposed to provide both IRQs and both DMA channels provided by ULTRASND,
 *       or given by a probing function. However this function also supports basic probing without any
 *       knowledge of what IRQ is involved. To do that, pass in the context with port number nonzero
 *       and IRQ and DMA channels set to -1 (unknown).
 *
 *       if program_cfg is nonzero, this routine will write the IRQ and DMA configuration registers. */
int ultrasnd_probe(struct ultrasnd_ctx *u,int program_cfg) {
	void (interrupt far *old_irq)() = NULL;
	unsigned char c1,c2,c3,c3i;
	unsigned int i;

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
	c1 = ultrasnd_selected_read(u);

	/* take the GF1 out of reset. don't enable the DAC/IRQ right away, because reset/end reset clears bits 1-2.
	 * if we were to write 0x07 now, we would read back 0x01 and DAC/IRQ enable would remain unset. */
	ultrasnd_select_write(u,0x4C,0x01); /* 0x4C: reset register -> end reset (bit 0=1) */
	c2 = ultrasnd_selected_read(u);

	/* once out of reset, now we can set DAC enable (bit 1) and IRQ enable (bit 2) */
	ultrasnd_select_write(u,0x4C,0x03); /* 0x4C: reset register -> stay out of reset (bit 0=1) DAC enable (bit1) IRQ enable (bit2) */
	c3 = ultrasnd_selected_read(u);

	/* if we read back nothing, then it's not a GUS. there's probably nothing there, even. */
	if (c1 == 0xFF && c2 == 0xFF && c3 == 0xFF) {
		if (debug_on) fprintf(stderr,"Gravis Ultrasound[0x%03x]: Read back 0xFF during reset, so it's probably not there\n",u->port);
		return 1;
	}

	/* the reset phase should show c1 == 0x00, c2 == 0x01, and c3 == 0x03.
	 * testing with real hardware says the reset register is readable, even though the GUS SDK doesn't say so. */
	if ((c1&7) != 0 || (c2&7) != 1 || (c3&7) != 3) {
		if (debug_on) fprintf(stderr,"Gravis Ultrasound[0x%03x]: Reset register is supposed to be read/write. This is probably not a GUS. c1=0x%02x c2=%02x\n",u->port,c1,c2);
		return 1;
	}

	/* force the card into a known state */
	ultrasnd_test_card = u;
	ultrasnd_test_irq_n = u->irq1;
	ultrasnd_test_irq_fired = 0;
	ultrasnd_abort_dma_transfer(u);
	ultrasnd_stop_all_voices(u);
	ultrasnd_stop_timers(u);
	ultrasnd_drain_irq_events(u);

	if (u->irq1 >= 0 && program_cfg) {
		/* now use the Mixer register to enable line out, and to select the IRQ control register.
		 * we can't actually read back the current configuration, because the registers are of
		 * course write only. but we can program them to match what we know about IRQ and DMA
		 * settings */
		outp(u->port+0x000,0x40 | 0x08);	/* bit 1: 0=enable line out, bit 0: 0=enable line in, bit 3: 1=enable latches  bit 6: 1=next I/O goes to IRQ latches */
		c1 = ultrasnd_valid_irq(u,u->irq1);	/* IRQ1 must be set */
		if (u->irq2 == u->irq1) c1 |= 0x40;	/* same IRQ */
		else c1 |= ultrasnd_valid_irq(u,u->irq2) << 3; /* IRQ2 can be unset */
		outp(u->port+0x00B,c1);

		outp(u->port+0x000,0x00 | 0x08);	/* bit 1: 0=enable line out, bit 0: 0=enable line in, bit 3: 1=enable latches  bit 6: 0=next I/O goes to DMA latches */
		c1 = ultrasnd_valid_dma(u,u->dma1);	/* DMA1 can be unset */
		if (u->dma2 == u->dma1) c1 |= 0x40;	/* same DMA */
		else c1 |= ultrasnd_valid_dma(u,u->dma2) << 3; /* DMA2 can be unset */
		outp(u->port+0x00B,c1);
	}
	else {
		/* just enable line out, do not program the IRQ and DMA settings */
		outp(u->port+0x000,0x00 | 0x08);	/* bit 1: 0=enable line out, bit 0: 0=enable line in, bit 3: 1=enable latches  bit 6: 1=next I/O goes to DMA latches */
	}

	/* the tests below can cause interrupts. so hook the IRQ */
	if (u->irq1 >= 0) {
		old_irq = _dos_getvect(irq2int(u->irq1));
		_dos_setvect(irq2int(u->irq1),ultrasnd_test_irq);
		p8259_mask(u->irq1);
		outp(p8259_irq_to_base_port(u->irq1,0),P8259_OCW2_SPECIFIC_EOI|(u->irq1&7)); /* make sure IRQ is acked */
		p8259_unmask(u->irq1);
	}

	/* IRQ is safe to enable now */
	c3i = 0x03 | (u->irq1 >= 0 ? 0x4 : 0x0); /* don't set bit 2 unless we intend to actually service the IRQ interrupt */
	ultrasnd_select_write(u,0x4C,c3i); /* 0x4C: reset register -> stay out of reset (bit 0=1) DAC enable (bit1) IRQ enable (bit2) */

	/* enabling the IRQ may have triggered one. if so, let me know please */
	if (u->irq1 >= 0) {
		_cli();
		if (ultrasnd_test_irq_fired != 0 && debug_on)
			fprintf(stderr,"Gravis Ultrasound[0x%03x]: IRQ %d stuck on card, despite all attempts to clear it\n",u->port,u->irq1);
		ultrasnd_test_irq_fired = 0;
		_sti();
	}

	if (u->irq1 >= 0 && ultrasnd_test_irq_fired != 0 && debug_on)
		fprintf(stderr,"Gravis Ultrasound[0x%03x]: IRQ fired at or before the timer was enabled. Did something else happen?\n",u->port,ultrasnd_test_irq_fired);

	/* Gravis SDK also shows there are two timer registers. When loaded by the application they count up to 0xFF then generate an IRQ.
	 * So if we load them and no counting happens, it's not a GUS. */
	ultrasnd_stop_timers(u);
	ultrasnd_select_write(u,0x45,0x0C); /* enable timer 1 & 2 IRQ */
	ultrasnd_select_write(u,0x46,0x80); /* load timer 1 (0x7F * 80us = 10.16ms) */
	ultrasnd_select_write(u,0x47,0x80); /* load timer 2 (0x7F * 320us = 40.64ms) */
	outp(u->port+0x008,0x04); /* select "timer stuff" */
	outp(u->port+0x009,0xE0); /* mask and irq clear */
	outp(u->port+0x009,0x60); /* mask timers */
	outp(u->port+0x009,0x00); /* unmask timers */
	outp(u->port+0x009,0x03); /* start timers */

	/* wait 100ms, more than enough time for the timers to count up to 0xFF and signal IRQ */
	/* NTS: The SDK doc doesn't say, but apparently the timer will hit 0xFF, fire the IRQ, then reset to 0 and count up again (or else DOSBox is mis-emulating the GUS) */
	c1 = inp(u->port+0x006);
	for (i=0,c1=0;i < 20 && (c1&0xC) != 0xC;i++) {
		_cli();
		t8254_wait(t8254_us2ticks(10000)); /* 10ms */
		c1 = inp(u->port+0x006); /* IRQ status */
		_sti();
	}
	ultrasnd_stop_timers(u);
	if ((c1&0xC) != 0xC) {
		if (debug_on) fprintf(stderr,"Gravis Ultrasound[0x%03x]: Timer readback fail 0x%02x\n",u->port,c1);
		goto timerfail;
	}
	if (u->irq1 >= 0 && ultrasnd_test_irq_fired == 0) {
		if (debug_on) fprintf(stderr,"Gravis Ultrasound[0x%03x]: Timer never signalled IRQ\n",u->port);
		goto irqfail;
	}

	/* check the RAM. is there at least a 4KB region we can peek and poke? */
	/* FIXME: According to Wikipedia there are versions of the card that don't have RAM at all (just ROM). How do we work with those? */
	for (i=0;i < 0x1000U;i++) ultrasnd_poke(u,i,(uint8_t)(i^0x55));
	for (i=0;i < 0x1000U;i++) if (ultrasnd_peek(u,i) != ((uint8_t)i^0x55)) goto ramfail;

	/* unless we know otherwise, samples cannot straddle a 256KB boundary */
	u->boundary256k = 1;

	/* initial versions came stock with 256KB with support for additional RAM in 256KB increments */
	u->total_ram = 256UL << 10UL; /* 256KB */
	for (i=1;i < ((1UL << 20UL) >> 18UL);i++) { /* test up to 1MB (1MB / 256KB banks) */
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
				if (debug_on)
					fprintf(stderr,"Gravis Ultrasound[0x%03x]: DMA transfer is incomplete (%u/%u), probably stalled out during transfer. You should choose another DMA channel.\n",u->port,rem,testlen);
				goto dmafail;
			}

			/* OK then, did our message make it into DRAM? */
			for (i=0;i < len;i++) {
				c = ultrasnd_peek(u,i+0x1000);
				if (c != testing[i]) {
					if (debug_on) {
						fprintf(stderr,"Gravis Ultrasound[0x%03x]: DMA transfer corrupted test data (phys=%8lx lin=%8lx)\n",u->port,(unsigned long)phys,(unsigned long)lin);
						fprintf(stderr,"  I wrote: '%s'\n",testing);
						fprintf(stderr," Got back: '");
						for (i=0;i < len;i++) fprintf(stderr,"%c",ultrasnd_peek(u,i+0x1000));
						fprintf(stderr,"'\n");
					}
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
		if (old_irq == NULL) p8259_mask(u->irq1);
	}

	if (u->dma1 >= 0)
		u->use_dma = 1;
	else
		u->use_dma = 0;

	ultrasnd_abort_dma_transfer(u);
	ultrasnd_stop_all_voices(u);
	ultrasnd_stop_timers(u);
	ultrasnd_drain_irq_events(u);

	for (i=0;i < 32;i++) u->voicemode[i] = 0;
	ultrasnd_set_active_voices(u,14);
	return 0;
dmafail:
	if (debug_on) fprintf(stderr,"Gravis Ultrasound[0x%03x]: DMA test failed, nothing was transferred\n",u->port);
	goto commoncleanup;
irqfail:
	if (debug_on) fprintf(stderr,"Gravis Ultrasound[0x%03x]: IRQ test failed, timers did not signal IRQ handler\n",u->port);
	goto commoncleanup;
ramfail:
	if (debug_on) fprintf(stderr,"Gravis Ultrasound[0x%03x]: RAM test failed\n",u->port);
	goto commoncleanup;
timerfail:
	if (debug_on) fprintf(stderr,"Gravis Ultrasound[0x%03x]: Timer test failed (IRQstatus=0x%02X)\n",u->port,inp(u->port+0x006));
	goto commoncleanup;
commoncleanup:
	if (u->irq1 >= 0) {
		_dos_setvect(irq2int(u->irq1),old_irq);
		if (old_irq == NULL) p8259_mask(u->irq1);
	}

	/* reset the GF1 */
	ultrasnd_select_write(u,0x4C,0x00); /* 0x4C: reset register -> master reset (bit 0=0) disable DAC (bit 1=0) master IRQ disable (bit 2=0) */
	t8254_wait(t8254_us2ticks(20000)); /* wait 20ms for good measure, most docs imply 14 CPU cycles via IN statements but I don't think that's enough */
	c1 = ultrasnd_selected_read(u);

	/* take the GF1 out of reset. don't enable the DAC/IRQ right away, because reset/end reset clears bits 1-2.
	 * if we were to write 0x07 now, we would read back 0x01 and DAC/IRQ enable would remain unset. */
	ultrasnd_select_write(u,0x4C,0x01); /* 0x4C: reset register -> end reset (bit 0=1) */
	t8254_wait(t8254_us2ticks(1000));
	c2 = ultrasnd_selected_read(u);

	ultrasnd_abort_dma_transfer(u);
	ultrasnd_stop_all_voices(u);
	ultrasnd_stop_timers(u);
	ultrasnd_drain_irq_events(u);

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

struct ultrasnd_ctx *ultrasnd_try_base(uint16_t base) {
	struct ultrasnd_ctx *u;

	if (ultrasnd_port_taken(base))
		return NULL;
	if ((u = ultrasnd_alloc_card()) == NULL)
		return NULL;

	ultrasnd_init_ctx(u);
	u->port = base;

	if (ultrasnd_probe(u,0)) {
		ultrasnd_free_card(u);
		return NULL;
	}

	if (u->irq1 < 0) {
		/* TODO: IRQ probing code. We *had* probing code but it was a bit too intrusive! */
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

uint16_t ultrasnd_sample_rate_to_fc(struct ultrasnd_ctx *u,unsigned long r) {
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
	mode |= ultrasnd_select_read(u,0x80) & 3;

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

void ultrasnd_drain_irq_events(struct ultrasnd_ctx *u) {
	unsigned char irqstat,patience = 255;
	unsigned int pass = 0,i;
	unsigned char c;

	/* read IRQ status. flush all possible reasons for an IRQ signal */
	do {
		irqstat = inp(u->port+6);
		if (irqstat & 0x80)
			ultrasnd_select_read(u,0x41);
		else if (irqstat & 0x60)
			ultrasnd_select_read(u,0x8F);
		else if (irqstat & 0x0C)
			ultrasnd_stop_timers(u);
		else
			break;

		if (--patience == 0) break;
	} while (1);

	do {
		if ((++pass) >= 256) break;
		for (i=0;i < 256;i++) ultrasnd_select_read(u,0x41);
		for (i=0;i < 256;i++) ultrasnd_select_read(u,0x8F);
		ultrasnd_stop_timers(u);
		c = inp(u->port+6);
	} while ((c & 0xE0) != 0);
}

void ultrasnd_stop_voice(struct ultrasnd_ctx *u,int i) {
	unsigned char c;
	int j;

	ultrasnd_select_voice(u,i);
	for (j=0;j < 16;j++) {
		c = ultrasnd_select_read(u,0x80) & (~ULTRASND_VOICE_MODE_IRQ/*disable IRQ */); /* voice select (read) */
		ultrasnd_select_write(u,0x00,c | 3); /* voice select (write), set STOP VOICE */
		t8254_wait(t8254_us2ticks(100)); /* 100us */
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

int ultrasnd_send_dram_buffer(struct ultrasnd_ctx *u,uint32_t ofs,unsigned long len,uint16_t flags) {
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
		ultrasnd_select_write(u,0x41,(u->dma1 >= 4 ? 4 : 0) | (flags & 0xE0)); /* data size in bit 2, writing to DRAM, enable DMA, and bits 6-7 provided by caller */
		ultrasnd_select_read(u,0x41); /* read to clear DMA terminal count---even though we didn't ask for TC IRQ */
		u->dma_tc_irq_happened = 0;

		/* Now initiate a DMA transfer (host) */
		outp(d8237_ioport(u->dma1,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(u->dma1) | D8237_MASK_SET); /* mask */
		outp(d8237_ioport(u->dma1,D8237_REG_W_WRITE_MODE),
			D8237_MODER_CHANNEL(u->dma1) |
			D8237_MODER_TRANSFER(D8237_MODER_XFER_READ) | /* "READ" from system memory */
			D8237_MODER_MODESEL(D8237_MODER_MODESEL_DEMAND));
		d8237_write_base(u->dma1,phys); /* RAM location with not much around */
		d8237_write_count(u->dma1,len);

		/* Now initiate a DMA transfer (GUS DRAM) */
		ultrasnd_select_write(u,0x41,(u->dma1 >= 4 ? 4 : 0) | (flags & 0xE0)); /* data size in bit 2, writing to DRAM, enable DMA, and bits 6-7 provided by caller */
		if (u->dma1 >= 4) /* Ugh, even DMA is subject to Gravis 16-bit translation */
			ultrasnd_select_write16(u,0x42,(uint16_t)(ultrasnd_dram_16bit_xlate(ofs)>>4UL));
		else
			ultrasnd_select_write16(u,0x42,(uint16_t)(ofs>>4UL));
		ultrasnd_select_write(u,0x41,(u->dma1 >= 4 ? 4 : 0) | 0x1 | (flags & 0xE0)); /* data size in bit 2, writing to DRAM, enable DMA, and bits 6-7 provided by caller */

		/* GO! */
		u->dma_tc_irq_happened = 0;
		outp(d8237_ioport(u->dma1,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(u->dma1)); /* unmask */

		_sti();

		/* watch it run */
		patience = 10000; /* 100ns * 10000 = 1 sec */
		do {
			if (u->irq1 >= 0 && (flags & ULTRASND_DMA_TC_IRQ) != 0 && !(flags & ULTRASND_VOICE_MODE_IRQ_BUT_DMA_WAIT)) {
				/* wait for caller's IRQ handler to set the flag */
				if (u->dma_tc_irq_happened) break;
			}
			else {
				rem = d8237_read_count(u->dma1);
				if (rem == 0 || rem >= 0xFFFE)
					break;
			}

			t8254_wait(t8254_us2ticks(100));
		} while (--patience != 0);
		rem = d8237_read_count(u->dma1);
		if (rem >= 0xFFFE) rem = 0;

		if (debug_on) {
			if (patience == 0)
				fprintf(stderr,"GUS DMA transfer timeout (rem=%lu)\n",rem);
			if (rem != 0)
				fprintf(stderr,"GUS DMA transfer TC while DMA controller has %u remaining\n",rem);
		}

		/* mask DMA channel again */
		outp(d8237_ioport(u->dma1,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(u->dma1) | D8237_MASK_SET); /* mask */

		/* stop DMA */
		ultrasnd_select_write(u,0x41,(u->dma1 >= 4 ? 4 : 0) | (flags & 0xE0)); /* data size in bit 2, writing to DRAM, enable DMA, and bits 6-7 provided by caller */
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

