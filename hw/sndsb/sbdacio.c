
#include <hw/sndsb/sndsb.h>

static inline void sndsb_timer_tick_directio_post_read(const unsigned short port,unsigned short count) {
	while (count-- != 0) inp(port);
}

static inline unsigned char sndsb_timer_tick_directio_poll_ready(const unsigned short port,unsigned short count) {
	unsigned char r = 0;

	do { r = inp(port);
	} while ((r&0x80) && count-- != 0);

	return !(r&0x80);
}

void sndsb_timer_tick_directi_data(struct sndsb_ctx * const cx) {
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

void sndsb_timer_tick_directi_cmd(struct sndsb_ctx * const cx) {
	if (sndsb_timer_tick_directio_poll_ready(cx->baseio+SNDSB_BIO_DSP_WRITE_STATUS+(cx->dsp_alias_port?1:0),cx->dsp_direct_dac_poll_retry_timeout)) {
		outp(cx->baseio+SNDSB_BIO_DSP_WRITE_DATA+(cx->dsp_alias_port?1:0),0x20);	/* direct DAC read */
		cx->timer_tick_func = sndsb_timer_tick_directi_data;
		cx->direct_dac_sent_command = 1;
		sndsb_timer_tick_directio_post_read(cx->baseio+SNDSB_BIO_DSP_WRITE_STATUS+(cx->dsp_alias_port?1:0),cx->dsp_direct_dac_read_after_command);
	}
}

void sndsb_timer_tick_directo_data(struct sndsb_ctx * const cx) {
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

void sndsb_timer_tick_directo_cmd(struct sndsb_ctx * const cx) {
	if (sndsb_timer_tick_directio_poll_ready(cx->baseio+SNDSB_BIO_DSP_WRITE_STATUS+(cx->dsp_alias_port?1:0),cx->dsp_direct_dac_poll_retry_timeout)) {
		outp(cx->baseio+SNDSB_BIO_DSP_WRITE_DATA+(cx->dsp_alias_port?1:0),0x10);	/* direct DAC write */
		cx->timer_tick_func = sndsb_timer_tick_directo_data;
		cx->direct_dac_sent_command = 1;
		sndsb_timer_tick_directio_post_read(cx->baseio+SNDSB_BIO_DSP_WRITE_STATUS+(cx->dsp_alias_port?1:0),cx->dsp_direct_dac_read_after_command);
	}
}

