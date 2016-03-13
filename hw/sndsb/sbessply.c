
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

int sndsb_begin_dsp_playback_s_ESS(struct sndsb_ctx *cx,unsigned short lv) {
	/* ESS 688/1869 chipset specific DSP playback.
	   using this mode bypasses a lot of the Sound Blaster Pro emulation
	   and restrictions and allows us to run up to 48KHz 16-bit stereo */
	unsigned short t16;
	int b;

	_cli();

	/* clear IRQ */
	sndsb_interrupt_ack(cx,3);

	/* need to reset the DSP to kick it into extended mode,
	 * or the DSP will fail to play audio. FIXME: is the problem
	 * simply that we shut off the DMA enable bit? */
	if (!cx->ess_extended_mode) sndsb_reset_dsp(cx);

	b = 0x00; /* DMA disable */
	b |= (cx->chose_autoinit_dsp) ? 0x04 : 0x00;
	b |= (cx->dsp_record) ? 0x0A : 0x00; /* [3]=DMA converter in ADC mode [1]=DMA read for ADC */
	if (sndsb_ess_write_controller(cx,0xB8,b) == -1) {
		_sti();
		return 0;
	}

	b = sndsb_ess_read_controller(cx,0xA8);
	if (b == -1) {
		_sti();
		return 0;
	}
	b &= ~0xB; /* clear mono/stereo and record monitor (bits 3, 1, and 0) */
	b |= (cx->buffer_stereo?1:2);	/* 10=mono 01=stereo */
	if (sndsb_ess_write_controller(cx,0xA8,b) == -1) {
		_sti();
		return 0;
	}

	/* NTS: The meaning of bits 1:0 in register 0xB9
	 *
	 *      00 single DMA transfer mode
	 *      01 demand DMA transfer mode, 2 bytes/request
	 *      10 demand DMA transfer mode, 4 bytes/request
	 *      11 reserved
	 *
	 * NOTES on what happens if you set bits 1:0 (DMA transfer type) to the "reserved" 11 value:
	 *
	 *      ESS 688 (Sharp laptop)          Nothing, apparently. Treated the same as 4 bytes/request
	 *
	 *      ESS 1887 (Compaq Presario)      Triggers a hardware bug where the chip appears to fetch
	 *                                      3 bytes per demand transfer but then only handle 1 byte,
	 *                                      which translates to audio playing at 3x the sample rate
	 *                                      it should be. NOT because the DAC is running any faster,
	 *                                      but because the chip is only playing back every 3rd sample!
	 *                                      This play only 3rds behavior is consistent across 8/16-bit
	 *                                      PCM and mono/stereo.
	 */

	/* TODO: This should be one of the options the user can tinker with for testing! */
	if (cx->goldplay_mode)
		b = cx->buffer_16bit ? 1 : 0;	/* demand transfer DMA 2 bytes (16-bit) or single transfer DMA (8-bit) */
	else
		b = 2;  /* demand transfer DMA 4 bytes per request */

	if (sndsb_ess_write_controller(cx,0xB9,b) == -1) {
		_sti();
		return 0;
	}

	if (cx->buffer_rate > 22050) {
		/* bit 7: = 1
		 * bit 6:0: = sample rate divider
		 *
		 * rate = 795.5KHz / (256 - x) */
		b = 256 - (795500UL / (unsigned long)cx->buffer_rate);
		if (b < 0x80) b = 0x80;
	}
	else {
		/* bit 7: = 0
		 * bit 6:0: = sample rate divider
		 *
		 * rate = 397.7KHz / (128 - x) */
		b = 128 - (397700UL / (unsigned long)cx->buffer_rate);
		if (b < 0) b = 0;
	}
	if (sndsb_ess_write_controller(cx,0xA1,b) == -1) {
		_sti();
		return 0;
	}

	b = 256 - (7160000UL / ((unsigned long)cx->buffer_rate * 32UL)); /* 80% of rate/2 times 82 I think... */
	if (sndsb_ess_write_controller(cx,0xA2,b) == -1) {
		_sti();
		return 0;
	}

	t16 = -(lv+1);
	if (sndsb_ess_write_controller(cx,0xA4,t16) == -1 || /* DMA transfer count low */
			sndsb_ess_write_controller(cx,0xA5,t16>>8) == -1) { /* DMA transfer count high */
		_sti();
		return 0;
	}

	b = sndsb_ess_read_controller(cx,0xB1);
	if (b == -1) {
		_sti();
		return 0;
	}
	b &= ~0xA0; /* clear compat game IRQ, fifo half-empty IRQs */
	b |= 0x50; /* set overflow IRQ, and "no function" */
	if (sndsb_ess_write_controller(cx,0xB1,b) == -1) {
		_sti();
		return 0;
	}

	b = sndsb_ess_read_controller(cx,0xB2);
	if (b == -1) {
		_sti();
		return 0;
	}
	b &= ~0xA0; /* clear compat */
	b |= 0x50; /* set DRQ/DACKB inputs for DMA */
	if (sndsb_ess_write_controller(cx,0xB2,b) == -1) {
		_sti();
		return 0;
	}

	b = 0x51; /* enable FIFO+DMA, reserved, load signal */
	b |= (cx->buffer_16bit ^ cx->audio_data_flipped_sign) ? 0x20 : 0x00; /* signed complement mode or not */
	if (sndsb_ess_write_controller(cx,0xB7,b) == -1) {
		_sti();
		return 0;
	}

	b = 0x90; /* enable FIFO+DMA, reserved, load signal */
	b |= (cx->buffer_16bit ^ cx->audio_data_flipped_sign) ? 0x20 : 0x00; /* signed complement mode or not */
	b |= (cx->buffer_stereo) ? 0x08 : 0x40; /* [3]=stereo [6]=!stereo */
	b |= (cx->buffer_16bit) ? 0x04 : 0x00; /* [2]=16bit */
	if (sndsb_ess_write_controller(cx,0xB7,b) == -1) {
		_sti();
		return 0;
	}

	b = sndsb_ess_read_controller(cx,0xB8);
	if (b == -1) {
		_sti();
		return 0;
	}
	if (sndsb_ess_write_controller(cx,0xB8,b | 1) == -1) { /* enable DMA */
		_sti();
		return 0;
	}

	return 1;
}

int sndsb_stop_dsp_playback_s_ESS(struct sndsb_ctx *cx) {
	int b;

	b = sndsb_ess_read_controller(cx,0xB8);
	if (b != -1) {
		b &= ~0x01; /* stop DMA */
		sndsb_ess_write_controller(cx,0xB8,b);
	}

	return 1;
}

void sndsb_send_buffer_again_s_ESS(struct sndsb_ctx *cx,unsigned long lv) {
	unsigned short t16;
	unsigned char b;

	/* stop DMA for a bit */
	b = sndsb_ess_read_controller(cx,0xB8);
	sndsb_ess_write_controller(cx,0xB8,b & (~1));

	t16 = -(lv+1);
	sndsb_ess_write_controller(cx,0xA4,t16); /* DMA transfer count low */
	sndsb_ess_write_controller(cx,0xA5,t16>>8); /* DMA transfer count high */

	/* start DMA again */
	b = sndsb_ess_read_controller(cx,0xB8);
	sndsb_ess_write_controller(cx,0xB8,b | 1);
}


