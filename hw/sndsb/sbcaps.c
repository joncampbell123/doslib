
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

/* we can do output method. if we can't, then don't bother playing, because it flat out won't work.
 * if we can, then you want to check if it's supported, because if it's not, you may get weird results, but nothing catastrophic. */
int sndsb_dsp_out_method_can_do(struct sndsb_ctx *cx,unsigned long wav_sample_rate,unsigned char wav_stereo,unsigned char wav_16bit) {
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
	if (cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_MAX) {
		MSG("play method out of range");
		return 0; /* invalid DSP output method */
	}
	if (cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_1xx && !wav_16bit && cx->dma8 < 0) {
		MSG("DMA-based playback, 8-bit PCM, no channel assigned (dma8)");
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

/* output method is supported (as in, recommended) */
int sndsb_dsp_out_method_supported(struct sndsb_ctx *cx,unsigned long wav_sample_rate,unsigned char wav_stereo,unsigned char wav_16bit) {
#if !(TARGET_MSDOS == 16 && (defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
# define MSG(x) cx->reason_not_supported = x
#else
# define MSG(x)
	cx->reason_not_supported = "";
#endif

	if (!sndsb_dsp_out_method_can_do(cx,wav_sample_rate,wav_stereo,wav_16bit))
		return 0;

	if (cx->dsp_play_method < SNDSB_DSPOUTMETHOD_4xx && wav_sample_rate < 4000) {
		MSG("Non-SB16 playback below 4000Hz probably not going to work");
		return 0;
	}
	if (cx->dsp_alias_port && cx->dsp_vmaj > 2) {
		MSG("DSP alias I/O ports only exist on original Sound Blaster\nDSP 1.xx and 2.xx");
		return 0;
	}

	if (cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_4xx) {
		if (cx->is_gallant_sc6600) {
			if (cx->dsp_vmaj < 3) {
				MSG("DSP 4.xx playback requires SB16 or clone [SC-6000]");
				return 0;
			}
		}
		else {
			if (cx->dsp_vmaj < 4) {
				MSG("DSP 4.xx playback requires SB16");
				return 0;
			}
		}
	}

	if (cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_1xx && cx->goldplay_mode && !cx->dsp_autoinit_dma) {
		MSG("Goldplay mode requires auto-init DMA to work properly");
		return 0;
	}
	if (cx->dsp_autoinit_command && cx->dsp_vmaj < 2) {
		MSG("Auto-init DSP command support requires DSP 2.0 or higher");
		return 0;
	}
	if ((cx->dsp_play_method == SNDSB_DSPOUTMETHOD_DIRECT || cx->goldplay_mode) && cx->windows_emulation) {
		MSG("Direct mode or goldplay mode not recommended\nfor use within a Windows DOS box, it won't work");
		return 0;
	}
	if (cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_1xx && cx->dsp_nag_mode && !cx->dsp_autoinit_dma) {
		MSG("Nag mode requires auto-init DMA to work properly");
		return 0;
	}

	if (wav_stereo && cx->dsp_vmaj < 3) {
		MSG("You are playing stereo audio on a DSP that doesn't support stereo");
		return 0;
	}

	if (wav_stereo && cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_1xx && cx->dsp_play_method < SNDSB_DSPOUTMETHOD_3xx) {
		MSG("You are playing stereo audio in a DSP mode\nthat doesn't support stereo");
		return 0;
	}
	if (wav_stereo && cx->dsp_play_method == SNDSB_DSPOUTMETHOD_DIRECT) {
		MSG("Direct DAC mode does not support stereo");
		return 0;
	}

	if (cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_201 &&
		(cx->dsp_vmaj < 2 || (cx->dsp_vmaj == 2 && cx->dsp_vmin == 0))) {
		MSG("DSP 2.01+ or higher playback requested for DSP older than v2.01");
		return 0;
	}
	if (cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_200 && cx->dsp_vmaj < 2) {
		MSG("DSP 2.0 or higher playback requested for DSP older than v2.0");
		return 0;
	}
	if (cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_1xx && cx->dsp_vmaj < 1) {
		MSG("DSP 1.xx or higher playback requested for\na DSP who's version I can't determine");
		return 0;
	}

	/* this library can play DMA without an IRQ channel assigned, but there are some restrictions on doing so */
	if (cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_1xx && cx->irq < 0) {
		/* we can do it if auto-init DMA and auto-init DSP and we poll the ack register (best for SB16).
		 * for pre-SB16, we can ignore the IRQ and playback will continue anyway. */
		if (cx->dsp_autoinit_dma && cx->dsp_autoinit_command &&
			((cx->dsp_adpcm > 0 && cx->enable_adpcm_autoinit) || cx->dsp_adpcm == 0) &&
			(cx->poll_ack_when_no_irq || cx->dsp_vmaj < 4) &&
			!(cx->vdmsound || cx->windows_xp_ntvdm || cx->windows_9x_me_sbemul_sys)) {
			/* yes */
		}
		/* we can do it if auto-init DMA and single-cycle DSP and we're nagging the DSP */
		else if (cx->dsp_nag_mode && sndsb_will_dsp_nag(cx)) {
			if (cx->dsp_play_method == SNDSB_DSPOUTMETHOD_4xx) {
				/* yes */
			}
			else if ((cx->force_hispeed || (wav_sample_rate*(wav_stereo?2:1)) > (cx->dsp_record ? 13000UL : 23000UL)) && cx->hispeed_blocking) {
				/* no */
				MSG("No IRQ assigned & DSP nag mode is ineffective\nif the DSP will run in 2.0/Pro highspeed DSP mode.");
				return 0;
			}
			else {
				/* yes */
			}
		}
		else {
			/* anything else is iffy */
			MSG("No IRQ assigned, no known combinations are selected that\n"
				"allow DSP playback to work. Try DSP auto-init with Poll ack\n"
				"or DSP single-cycle with nag mode enabled.");
			return 0;
		}
	}

	if (cx->dsp_nag_mode) {
		/* nag mode can cause problems with DSP 4.xx commands? */
		if (cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_4xx) {
			MSG("DSP nag mode on a SB16 in DSP 4.xx mode can cause problems.\n"
						"Halting, popping/cracking, stereo L/R swapping timing glitches.\n"
						"Use DSP auto-init and non-IRQ polling for more reliable DMA.");
			return 0;
		}
		/* nag mode can cause lag from the idle command if hispeed mode is involved */
		if (cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_201 && cx->hispeed_matters && cx->hispeed_blocking &&
			cx->dsp_nag_hispeed && (cx->force_hispeed || (wav_sample_rate*(wav_stereo?2:1)) > (cx->dsp_record ? 13000UL : 23000UL))) {
			MSG("DSP nag mode when hispeed DSP playback is involved can cause\n"
				"lagging and delay on this system because the DSP will block during playback");
			return 0;
		}
	}

	MSG("Target sample rate out of range");
	if (cx->dsp_adpcm > 0) {
		/* Neither VDMSOUND.EXE or NTVDM's SB emulation handle ADPCM well */
		if (cx->vdmsound || cx->windows_xp_ntvdm || cx->windows_9x_me_sbemul_sys) {
			MSG("You are attempting ADPCM within Windows\nemulation that will likely not support ADPCM playback");
			return 0;
		}

		/* Gallant SC-6600 clones do not support auto-init ADPCM, though they support all modes */
		if (cx->is_gallant_sc6600 && cx->enable_adpcm_autoinit && cx->dsp_autoinit_command) {
			MSG("SC-6600 SB clones do not support auto-init ADPCM");
			return 0;
		}

		/* NTS: If we could easily differentiate Creative SB 2.0 from clones, we could identify the
		 *      slightly out-of-spec ranges supported by the SB 2.0 that deviates from Creative
		 *      documentation */
		if (cx->dsp_adpcm == ADPCM_4BIT) {
			if (wav_sample_rate > 12000UL) return 0;
		}
		else if (cx->dsp_adpcm == ADPCM_2_6BIT) {
			if (wav_sample_rate > 13000UL) return 0;
		}
		else if (cx->dsp_adpcm == ADPCM_2BIT) {
			if (wav_sample_rate > 11000UL) return 0; /* NTS: On actual Creative SB 2.0 hardware, this can apparently go up to 15KHz */
		}
		else {
			return 0;
		}
	}
	else if (cx->dsp_play_method == SNDSB_DSPOUTMETHOD_4xx) {
		/* based on Sound Blaster 16 PnP cards that max out at 48000Hz apparently */
		/* FIXME: Is there a way for us to distinguish a Sound Blaster 16 (max 44100Hz)
		 *        from later cards (max 48000Hz) *other* than whether or not it is Plug & Play?
		 *        Such as using the DSP version? At what DSP version did the card go from
		 *        a max 44100Hz to 48000Hz? */
		if (wav_sample_rate > cx->max_sample_rate_dsp4xx) return 0;
	}
	else if (cx->ess_extensions && cx->dsp_play_method == SNDSB_DSPOUTMETHOD_3xx) {
		/* I've been able to drive ESS chips up to 48Khz and beyond (though beyond 48KHz 16-bit stereo
		 * the ISA bus can't keep up well). But let's cap it at 48KHz anyway */
		if (wav_sample_rate > 48000) return 0;
	}
	else if ((!cx->hispeed_matters && cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_1xx) ||
		cx->dsp_play_method == SNDSB_DSPOUTMETHOD_3xx || cx->dsp_play_method == SNDSB_DSPOUTMETHOD_201) {
		/* Because of the way Sound Blaster Pro stereo works and the way the time constant
		 * is generated, the maximum sample rate is halved in stereo playback. On Pro and
		 * old SB16 cards this means a max of 44100Hz mono 22050Hz stereo. On SB16 ViBRA
		 * cards, this usually means a maximum of 48000Hz mono 24000Hz stereo.
		 *
		 * For DSP 2.01+ support, we also use this calculation because hispeed mode is involved */
		if (wav_sample_rate > ((cx->dsp_record ? cx->max_sample_rate_sb_hispeed : cx->max_sample_rate_sb_hispeed) / (wav_stereo ? 2U : 1U))) return 0;
	}
	else if (cx->dsp_play_method == SNDSB_DSPOUTMETHOD_200 || cx->dsp_play_method == SNDSB_DSPOUTMETHOD_1xx) {
		if (wav_sample_rate > (cx->dsp_record ? cx->max_sample_rate_sb_rec : cx->max_sample_rate_sb_play)) return 0;
	}
	else if (cx->dsp_play_method == SNDSB_DSPOUTMETHOD_DIRECT) {
		if (wav_sample_rate > (cx->dsp_record ? cx->max_sample_rate_sb_rec_dac : cx->max_sample_rate_sb_play_dac)) return 0;
	}
	MSG(NULL);
	/* Creative SB16 cards do not pay attention to the Sound Blaster Pro stereo bit.
	 * Playing stereo using the 3xx method on 4.xx DSPs will not work. Most SB16 clones
	 * will pay attention to that bit however, but it's best not to assume that will happen. */
	if (cx->dsp_vmaj >= 4 && cx->dsp_play_method == SNDSB_DSPOUTMETHOD_3xx && wav_stereo) {
		MSG("Sound Blaster Pro stereo playback on SB16 (DSP 4.xx)\nwill not play as stereo because Creative SB16\ncards ignore the mixer bit");
		return 0;
	}
	/* SB16 cards seem to alias hispeed commands to normal DSP and let them set the time constant all the way up to the max supported by
	 * the DSP, hispeed mode or not. */
	if (cx->dsp_vmaj >= 4 && (cx->dsp_play_method == SNDSB_DSPOUTMETHOD_201 || cx->dsp_play_method == SNDSB_DSPOUTMETHOD_3xx) && cx->hispeed_matters) {
		MSG("Sound Blaster 2.0/Pro high-speed DSP modes not\nrecommended for use on your DSP (DSP 4.xx detected)");
		return 0;
	}
	/* friendly reminder to the user that despite DSP autoinit enable 1.xx commands are not auto-init */
	if (cx->dsp_autoinit_command && cx->dsp_play_method == SNDSB_DSPOUTMETHOD_1xx) {
		MSG("DSP 1.xx commands do not support auto-init. Playback\nis automatically using single-cycle commands instead.");
		return 1; /* we support it, but just to let you know... */
	}
	/* playing DMA backwards with 16-bit audio is not advised.
	 * it COULD theoretically work with a 16-bit DMA channel because of how it counts, but...
	 * there's also the risk you use an 8-bit DMA channel which of course gets the byte order wrong! */
	if (cx->backwards && wav_16bit) {
		MSG("16-bit PCM played backwards is not recommended\nbyte order may not be correct to sound card");
		return 0;
	}
	/* it's also a good bet Windows virtualization never even considers DMA in decrement mode because nobody really ever uses it */
	if (cx->backwards && cx->windows_emulation) {
		MSG("DMA played backwards is not recommended from\nwithin a Windows DOS box");
		return 0;
	}
	/* EMM386.EXE seems to handle backwards DMA just fine, but we can't assume v86 monitors handle it well */
#if TARGET_MSDOS == 32
	if (cx->backwards && dos_ltp_info.paging && dos_ltp_info.dma_dos_xlate) {
#else
	if (cx->backwards && (cpu_flags&CPU_FLAG_V86_ACTIVE)) {
#endif
		MSG("DMA played backwards is not recommended from\nwithin a virtual 8086 mode monitor");
		return 0;
	}

	/* NTS: Virtualbox supports backwards DMA, it's OK */

# if !(TARGET_MSDOS == 16 && (defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
	cx->reason_not_supported = NULL;
# endif
	return 1;
#undef MSG
}

