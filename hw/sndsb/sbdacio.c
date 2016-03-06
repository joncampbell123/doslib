
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

static inline void sndsb_timer_tick_directio_post_read(unsigned short port,unsigned short count) {
	while (count-- != 0) inp(port);
}

static inline unsigned char sndsb_timer_tick_directio_poll_ready(unsigned short port,unsigned short count) {
	unsigned char r = 0;

	do { r = inp(port);
	} while ((r&0x80) && count-- != 0);

	return !(r&0x80);
}

void sndsb_timer_tick_directi_data(struct sndsb_ctx *cx) {
	if (sndsb_timer_tick_directio_poll_ready(cx->baseio+SNDSB_BIO_DSP_WRITE_STATUS+(cx->dsp_alias_port?1:0),cx->dsp_direct_dac_poll_retry_timeout)) {
		cx->buffer_lin[cx->direct_dsp_io] = inp(cx->baseio+SNDSB_BIO_DSP_READ_DATA);
		if (cx->backwards) {
			if (cx->direct_dsp_io == 0) cx->direct_dsp_io = cx->buffer_size - 1;
			else cx->direct_dsp_io--;
		}
		else {
			if ((++cx->direct_dsp_io) >= cx->buffer_size) cx->direct_dsp_io = 0;
		}
		cx->timer_tick_func = sndsb_timer_tick_directi_cmd;
		cx->direct_dac_sent_command = 0;
		sndsb_timer_tick_directio_post_read(cx->baseio+SNDSB_BIO_DSP_WRITE_STATUS+(cx->dsp_alias_port?1:0),cx->dsp_direct_dac_read_after_command);
	}
}

void sndsb_timer_tick_directi_cmd(struct sndsb_ctx *cx) {
	if (sndsb_timer_tick_directio_poll_ready(cx->baseio+SNDSB_BIO_DSP_WRITE_STATUS+(cx->dsp_alias_port?1:0),cx->dsp_direct_dac_poll_retry_timeout)) {
		outp(cx->baseio+SNDSB_BIO_DSP_WRITE_DATA+(cx->dsp_alias_port?1:0),0x20);	/* direct DAC read */
		cx->timer_tick_func = sndsb_timer_tick_directi_data;
		cx->direct_dac_sent_command = 1;
		sndsb_timer_tick_directio_post_read(cx->baseio+SNDSB_BIO_DSP_WRITE_STATUS+(cx->dsp_alias_port?1:0),cx->dsp_direct_dac_read_after_command);
	}
}

void sndsb_timer_tick_directo_data(struct sndsb_ctx *cx) {
	if (sndsb_timer_tick_directio_poll_ready(cx->baseio+SNDSB_BIO_DSP_WRITE_STATUS+(cx->dsp_alias_port?1:0),cx->dsp_direct_dac_poll_retry_timeout)) {
		outp(cx->baseio+SNDSB_BIO_DSP_WRITE_DATA+(cx->dsp_alias_port?1:0),cx->buffer_lin[cx->direct_dsp_io]);
		if (cx->backwards) {
			if (cx->direct_dsp_io == 0) cx->direct_dsp_io = cx->buffer_size - 1;
			else cx->direct_dsp_io--;
		}
		else {
			if ((++cx->direct_dsp_io) >= cx->buffer_size) cx->direct_dsp_io = 0;
		}
		cx->timer_tick_func = sndsb_timer_tick_directo_cmd;
		cx->direct_dac_sent_command = 0;
		sndsb_timer_tick_directio_post_read(cx->baseio+SNDSB_BIO_DSP_WRITE_STATUS+(cx->dsp_alias_port?1:0),cx->dsp_direct_dac_read_after_command);
	}
}

void sndsb_timer_tick_directo_cmd(struct sndsb_ctx *cx) {
	if (sndsb_timer_tick_directio_poll_ready(cx->baseio+SNDSB_BIO_DSP_WRITE_STATUS+(cx->dsp_alias_port?1:0),cx->dsp_direct_dac_poll_retry_timeout)) {
		outp(cx->baseio+SNDSB_BIO_DSP_WRITE_DATA+(cx->dsp_alias_port?1:0),0x10);	/* direct DAC write */
		cx->timer_tick_func = sndsb_timer_tick_directo_data;
		cx->direct_dac_sent_command = 1;
		sndsb_timer_tick_directio_post_read(cx->baseio+SNDSB_BIO_DSP_WRITE_STATUS+(cx->dsp_alias_port?1:0),cx->dsp_direct_dac_read_after_command);
	}
}

