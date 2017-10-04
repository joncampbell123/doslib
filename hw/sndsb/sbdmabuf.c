
#include <conio.h>

#include <hw/8237/8237.h>		/* 8237 DMA */
#include <hw/sndsb/sndsb.h>

int sndsb_assign_dma_buffer(struct sndsb_ctx * const cx,const struct dma_8237_allocation * const dma) {
    if (dma != NULL) {
        cx->buffer_size = dma->length;
        cx->buffer_phys = dma->phys;
        cx->buffer_lin = dma->lin;
    }
    else {
        cx->buffer_size = 0;
        cx->buffer_phys = 0;
        cx->buffer_lin = NULL;
    }

	return 1;
}

static uint32_t sndsb_recommended_dma_buffer_size_common_mod(struct sndsb_ctx * const ctx,uint32_t ret) {
	/* Known constraint: Windows 3.1 Creative SB16 drivers don't like it when DOS apps
	 *                   use too large a DMA buffer. It causes Windows to complain about
	 *                   "a DOS program violating the integrity of the operating system".
	 *
	 *                   FIXME: Even with small buffers, it "violates the integrity" anyway.
	 *                          So what the fuck is wrong then? */
	if (windows_mode == WINDOWS_ENHANCED && windows_version < 0x35F && /* Windows 3.1 and Creative SB16 drivers v3.57 */
		ctx->windows_creative_sb16_drivers && ctx->windows_creative_sb16_drivers_ver == (0x300 + 57)) {
		if (ret > (4UL * 1024UL)) ret = 4UL * 1024UL;
	}

    return ret;
}

uint32_t sndsb_recommended_dma_buffer_size(struct sndsb_ctx * const ctx,uint32_t limit) {
	uint32_t ret = (64UL * 1024UL) - 16UL;
	if (limit != 0UL && ret > limit) ret = limit;
	return sndsb_recommended_dma_buffer_size_common_mod(ctx,ret);
}

uint32_t sndsb_recommended_16bit_dma_buffer_size(struct sndsb_ctx * const ctx,uint32_t limit) {
	uint32_t ret = ((d8237_can_do_16bit_128k() ? 128UL : 64UL) * 1024UL) - 16UL;
	if (limit != 0UL && ret > limit) ret = limit;
	return sndsb_recommended_dma_buffer_size_common_mod(ctx,ret);
}

int sndsb_shutdown_dma(struct sndsb_ctx * const cx) {
	unsigned char ch = cx->buffer_16bit ? cx->dma16 : cx->dma8;
	if ((signed char)ch == -1) return 0;
	/* set up the DMA channel */
	outp(d8237_ioport(ch,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(ch) | D8237_MASK_SET); /* mask */
	return 1;
}

int sndsb_setup_dma(struct sndsb_ctx * const cx) {
	unsigned char ch = cx->buffer_16bit ? cx->dma16 : cx->dma8;
	unsigned char dma_mode = D8237_MODER_MODESEL_SINGLE;

	/* ESS bugfix: except for goldplay mode, we tell the chipset to use demand mode fetching.
	 * So then, setup the DMA controller for it too! */
	if (cx->ess_extensions && !cx->goldplay_mode)
		dma_mode = D8237_MODER_MODESEL_DEMAND;

	/* if we're doing the Windows "spring" buffer hack, then don't do anything.
	 * later when the calling program queries the DMA position, we'll setup DSP playback and call this function again */
	if (cx->windows_emulation && cx->windows_springwait == 0 && cx->windows_xp_ntvdm)
		return 1;

	if (cx->backwards)
		cx->direct_dsp_io = cx->buffer_size - 1;
	else
		cx->direct_dsp_io = 0;

	if ((signed char)ch == -1) return 0;
	/* set up the DMA channel */
	outp(d8237_ioport(ch,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(ch) | D8237_MASK_SET); /* mask */

	outp(d8237_ioport(ch,D8237_REG_W_WRITE_MODE),
		(cx->chose_autoinit_dma ? D8237_MODER_AUTOINIT : 0) |
		(cx->backwards ? D8237_MODER_ADDR_DEC : 0) |
		D8237_MODER_CHANNEL(ch) |
		D8237_MODER_TRANSFER(cx->dsp_record ? D8237_MODER_XFER_WRITE : D8237_MODER_XFER_READ) |
		D8237_MODER_MODESEL(dma_mode));

	if (cx->goldplay_mode) {
		/* goldplay mode REQUIRES auto-init DMA */
		if (!cx->chose_autoinit_dma) return -1;

		cx->gold_memcpy = (cx->buffer_16bit?2:1)*(cx->buffer_stereo?2:1);

#if TARGET_MSDOS == 32
		if (cx->goldplay_dma == NULL) {
			if ((cx->goldplay_dma=dma_8237_alloc_buffer(16)) == NULL)
				return 0;
		}
#endif

		/* Goldplay mode: The size of ONE sample is given to the DMA controller.
		 * This tricks the DMA controller into re-transmitting that sample continuously
		 * to the sound card. Then the demo uses the timer interrupt to modify that byte
		 * and make audio. This was apparently popular with Goldplay in the 1991-1993
		 * demoscene time frame, and evidently worked fine, but on today's PCs with CPU
		 * caches and buffers this crap would obviously never fly.
		 *
		 * Note we allow the program to do this with 16-bit output, even though the
		 * original Goldplay library was limited to 8 and nobody ever did this kind of
		 * hackery by the time 16-bit SB output was the norm. But my test code shows
		 * that you can pull that stunt with stereo and 16-bit audio modes too! */
		d8237_write_count(ch,(cx->buffer_stereo ? 2 : 1)*(cx->buffer_16bit ? 2 : 1));
		/* point it to our "goldplay_dma" */
#if TARGET_MSDOS == 32
		d8237_write_base(ch,cx->goldplay_dma->phys + (cx->backwards ? (cx->gold_memcpy-1) : 0));

		if ((cx->buffer_16bit?1:0)^(cx->audio_data_flipped_sign?1:0))
			memset(cx->goldplay_dma->lin,0,4);
		else
			memset(cx->goldplay_dma->lin,128,4);
#else
		{
			unsigned char far *p = (unsigned char far*)(cx->goldplay_dma);
			d8237_write_base(ch,((uint32_t)FP_SEG(p) << 4UL) + (uint32_t)FP_OFF(p) + (cx->backwards ? (cx->gold_memcpy-1) : 0));

			if ((cx->buffer_16bit?1:0)^(cx->audio_data_flipped_sign?1:0))
				_fmemset(p,0,4);
			else
				_fmemset(p,128,4);
		}
#endif
	}
	else {
		d8237_write_count(ch,cx->buffer_dma_started_length);
		if (cx->backwards)
			d8237_write_base(ch,cx->buffer_phys+cx->buffer_dma_started+cx->buffer_dma_started_length-1);
		else
			d8237_write_base(ch,cx->buffer_phys+cx->buffer_dma_started); /* RAM location with not much around */
	}

	outp(d8237_ioport(ch,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(ch)); /* unmask */
	return 1;
}

uint32_t sndsb_read_dma_buffer_position(struct sndsb_ctx * const cx) {
	uint32_t r;

	/* the program is asking for DMA position. If we're doing the Windows springwait hack,
	 * then NOW is the time to initialize DSP transfer! */
	if (cx->windows_emulation && cx->windows_springwait == 1 && cx->windows_xp_ntvdm) {
		sndsb_prepare_dsp_playback(cx,cx->buffer_rate,cx->buffer_stereo,cx->buffer_16bit);
		sndsb_setup_dma(cx);
		sndsb_begin_dsp_playback(cx);
		cx->windows_springwait = 2;
	}

	/* "direct" and "goldplay" methods require the program to update the play point in some fashion,
	 * usually by programming IRQ 0 to tick at the sample rate */
	if (cx->dsp_play_method == SNDSB_DSPOUTMETHOD_DIRECT || cx->goldplay_mode) {
		r = cx->direct_dsp_io;
		if (r >= cx->buffer_size) r = cx->buffer_size - 1;
	}
	else if (cx->buffer_16bit) {
		if (cx->dma16 < 0) return 0;
		r = d8237_read_count(cx->dma16);
		if (cx->backwards) {
			/* TODO */
		}
		else {
			if (r >= 0x1FFFEUL) r = 0; /* FIXME: the 8237 library should have a "is terminal count" function */
			if (r >= cx->buffer_dma_started_length) r = cx->buffer_dma_started_length - 1;
			r = cx->buffer_dma_started_length - (r+1);
			r += cx->buffer_dma_started;
		}
	}
	else {
		if (cx->dma8 < 0) return 0;
		r = d8237_read_count(cx->dma8);
		if (cx->backwards) {
			if (r >= 0xFFFFUL) r = 0;
			if (r >= cx->buffer_dma_started_length) r = cx->buffer_dma_started_length - 1;
			r += cx->buffer_dma_started;
		}
		else {
			if (r >= 0xFFFFUL) r = 0;
			if (r >= cx->buffer_dma_started_length) r = cx->buffer_dma_started_length - 1;
			r = cx->buffer_dma_started_length - (r+1);
			r += cx->buffer_dma_started;
		}
	}

	return r;
}

