
#include <conio.h>

#include <hw/8237/8237.h>
#include <hw/sndsb/sndsb.h>

/* we can do output method. if we can't, then don't bother playing, because it flat out won't work.
 * if we can, then you want to check if it's supported, because if it's not, you may get weird results, but nothing catastrophic. */
int sndsb_dsp_out_method_can_do(struct sndsb_ctx * const cx,const unsigned long wav_sample_rate,const unsigned char wav_stereo,const unsigned char wav_16bit) {
#if !(TARGET_MSDOS == 16 && (defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
# define MSG(x) cx->reason_not_supported = x
#else
# define MSG(x)
	cx->reason_not_supported = "";
#endif

	if (!cx->dsp_ok) {
		MSG("DSP not detected");
		return 0; /* No DSP, no playback */
	}
    if (wav_sample_rate == 0UL) {
        MSG("Cannot support a sample rate of zero");
        return 0;
    }
	if (cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_MAX) {
		MSG("play method out of range");
		return 0; /* invalid DSP output method */
	}
	if (cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_1xx && !wav_16bit && cx->dma8 < 0) {
		MSG("DMA-based playback, 8-bit PCM, no channel assigned (dma8)");
		return 0;
	}

    /* DSP 4.xx sample rate control limited to 16 bits */
    if (cx->dsp_play_method == SNDSB_DSPOUTMETHOD_4xx && wav_sample_rate > 0xFFFFUL) {
        MSG("Sample rate too high (larger than 16-bit unsigned int)");
        return 0;
    }

	if (cx->dsp_play_method == SNDSB_DSPOUTMETHOD_3xx && cx->ess_extensions) {
		/* OK. we can use ESS extensions with flipped sign */
	}
	else if (cx->dsp_play_method < SNDSB_DSPOUTMETHOD_4xx && cx->audio_data_flipped_sign) {
		MSG("Flipped sign playback requires DSP 4.xx playback");
		return 0;
	}

	if (cx->dsp_play_method == SNDSB_DSPOUTMETHOD_3xx && cx->ess_extensions) {
		/* OK. we can use ESS extensions to do 16-bit playback */
	}
	else if (cx->dsp_play_method < SNDSB_DSPOUTMETHOD_4xx && wav_16bit) {
		MSG("16-bit PCM playback requires DSP 4.xx mode");
		return 0;
	}

	if (cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_1xx && wav_16bit && cx->dma16 < 0) {
		MSG("DMA-based playback, 16-bit PCM, no channel assigned (dma16)");
		return 0;
	}
	if (cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_1xx && wav_16bit && cx->dma16 >= 4 && !(d8237_flags&D8237_DMA_SECONDARY)) {
		MSG("DMA-based playback, 16-bit PCM, dma16 channel refers to\nnon-existent secondary DMA controller");
		return 0;
	}
	if (cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_1xx && wav_16bit && cx->dma16 >= 0 && cx->dma16 < 4 && !(d8237_flags&D8237_DMA_PRIMARY)) {
		MSG("DMA-based playback, 16-bit PCM, dma16 channel refers to\nnon-existent primary DMA controller");
		return 0;
	}
	if (cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_1xx && !wav_16bit && cx->dma8 >= 4 && !(d8237_flags&D8237_DMA_SECONDARY)) { /* as if this would ever happen, but.. */
		MSG("DMA-based playback, 8-bit PCM, dma8 channel refers to\nnon-existent secondary DMA controller");
		return 0;
	}
	if (cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_1xx && !wav_16bit && cx->dma8 >= 0 && cx->dma8 < 4 && !(d8237_flags&D8237_DMA_PRIMARY)) {
		MSG("DMA-based playback, 8-bit PCM, dma8 channel refers to\nnon-existent primary DMA controller");
		return 0;
	}

	if (cx->dsp_adpcm > 0) {
		if (cx->dsp_record) {
			MSG("No such thing as ADPCM recording");
			return 0;
		}
		if (cx->dsp_play_method == SNDSB_DSPOUTMETHOD_DIRECT) {
			MSG("No such thing as direct DAC ADPCM playback");
			return 0;
		}
		if (wav_16bit) {
			MSG("No such thing as 16-bit ADPCM playback");
			return 0;
		}
		if (wav_stereo) {
			MSG("No such thing as stereo ADPCM playback");
			return 0;
		}
		if (cx->audio_data_flipped_sign) {
			MSG("No such thing as flipped sign ADPCM playback");
			return 0;
		}
		if (cx->goldplay_mode) {
			MSG("Goldplay ADPCM playback not supported");
			return 0;
		}
	}
	else if (cx->goldplay_mode) {
#if TARGET_MSDOS == 16
		/* bug-check: goldplay 16-bit DMA is not possible if somehow the goldplay_dma[] field is not WORD-aligned
		 * and 16-bit audio is using the 16-bit DMA channel (misaligned while 8-bit DMA is fine) */
		if (cx->buffer_16bit && cx->dma16 >= 4 && ((unsigned int)(cx->goldplay_dma))&1) {
			MSG("16-bit PCM Goldplay playback requested\nand DMA buffer is not word-aligned.");
			return 0;
		}
#endif
	}

# if !(TARGET_MSDOS == 16 && (defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
	cx->reason_not_supported = NULL;
# endif
	return 1;
#undef MSG
}

