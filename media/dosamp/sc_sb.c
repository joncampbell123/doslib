
#define HW_DOS_DONT_DEFINE_MMSYSTEM

#include <stdio.h>
#include <stdint.h>
#ifdef LINUX
#include <endian.h>
#else
#include <hw/cpu/endian.h>
#endif
#ifndef LINUX
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <direct.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <malloc.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#ifndef LINUX
#include <dos.h>
#endif

#ifndef LINUX
#include <hw/dos/dos.h>
#include <hw/cpu/cpu.h>
#include <hw/8237/8237.h>       /* 8237 DMA */
#include <hw/8254/8254.h>       /* 8254 timer */
#include <hw/8259/8259.h>       /* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/cpu/cpurdtsc.h>
#include <hw/dos/doswin.h>
#include <hw/dos/tgusmega.h>
#include <hw/dos/tgussbos.h>
#include <hw/dos/tgusumid.h>
#include <hw/isapnp/isapnp.h>
#include <hw/sndsb/sndsbpnp.h>
#endif

#include "wavefmt.h"
#include "dosamp.h"
#include "timesrc.h"
#include "dosptrnm.h"
#include "filesrc.h"
#include "resample.h"
#include "cvrdbuf.h"
#include "cvip.h"
#include "trkrbase.h"
#include "tmpbuf.h"
#include "snirq.h"
#include "sndcard.h"

#include "sc_sb.h"

#if defined(HAS_SNDSB)

/* private */
static struct sndsb_ctx *soundblaster_get_sndsb_ctx(soundcard_t sc) {
    if (sc->p.soundblaster.index < 0 || sc->p.soundblaster.index >= SNDSB_MAX_CARDS)
        return NULL;

    return &sndsb_card[sc->p.soundblaster.index];
}

/* private */
static void soundblaster_update_wav_dma_position(soundcard_t sc,struct sndsb_ctx *card) {
    _cli();
    sc->wav_state.dma_position = sndsb_read_dma_buffer_position(card);
    _sti();
}

/* private */
static void soundblaster_update_wav_play_delay(soundcard_t sc,struct sndsb_ctx *card) {
    signed long delay;

    /* DMA trails our "last IO" pointer */
    delay  = (signed long)card->buffer_last_io;
    delay -= (signed long)sc->wav_state.dma_position;

    /* delay == 0 is a special case.
     * if wav_state.play_empty, then it means there's no delay.
     * else, it means there's one whole buffer's worth delay.
     * we HAVE to make this distinction because this code is
     * written to load new audio data RIGHT BEHIND the DMA position
     * which could easily lead to buffer_last_io == DMA position! */
    if (delay < 0L) delay += (signed long)card->buffer_size;
    else if (delay == 0L && !sc->wav_state.play_empty) delay = (signed long)card->buffer_size;

    /* guard against inconcievable cases */
    if (delay < 0L) delay = 0L;
    if (delay >= (signed long)card->buffer_size) delay = (signed long)card->buffer_size;

    /* convert to samples */
    sc->wav_state.play_delay_bytes = (unsigned long)delay;
    sc->wav_state.play_delay = ((unsigned long)delay / sc->cur_codec.bytes_per_block) * sc->cur_codec.samples_per_block;

    /* play position is calculated here */
    sc->wav_state.play_counter = sc->wav_state.write_counter;
    if (sc->wav_state.play_counter >= sc->wav_state.play_delay_bytes)
        sc->wav_state.play_counter -= sc->wav_state.play_delay_bytes;
    else
        sc->wav_state.play_counter = 0;

    if (sc->wav_state.play_counter_prev < sc->wav_state.play_counter)
        sc->wav_state.play_counter_prev = sc->wav_state.play_counter;
}

/* this depends on keeping the "play delay" up to date */
static uint32_t dosamp_FAR soundblaster_can_write(soundcard_t sc) { /* in bytes */
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);
    uint32_t x;

    if (card == NULL) return (uint32_t)(~0UL);

    soundblaster_update_wav_dma_position(sc,card);
    soundblaster_update_wav_play_delay(sc,card);

    x = card->buffer_size;
    if (x >= sc->wav_state.play_delay_bytes) x -= sc->wav_state.play_delay_bytes;
    else x = 0;

    return x;
}

/* detect playback underruns, and force write pointer forward to compensate if so.
 * caller will direct us how many bytes into the future to set the write pointer,
 * since nobody can INSTANTLY write audio to the playback pointer.
 *
 * This way, the playback program can use this to force the write pointer forward
 * if underrun to at least ensure the next audio written will be immediately audible.
 * Without this, upon underrun, the written audio may not be heard until the play
 * pointer has gone through the entire buffer again. */
static int dosamp_FAR soundblaster_clamp_if_behind(soundcard_t sc,uint32_t ahead_in_bytes) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);
    int res = 0;

    if (card == NULL) return -1;

    soundblaster_update_wav_play_delay(sc,card);

    if (sc->wav_state.play_counter < sc->wav_state.play_counter_prev) {
        card->buffer_last_io  = sc->wav_state.dma_position;
        card->buffer_last_io += ahead_in_bytes;
        card->buffer_last_io -= card->buffer_last_io % sc->cur_codec.bytes_per_block;
        if (card->buffer_last_io >= card->buffer_size)
            card->buffer_last_io -= card->buffer_size;
        if (card->buffer_last_io >= card->buffer_size)
            card->buffer_last_io  = 0;

        res = 1;
        soundblaster_update_wav_play_delay(sc,card);
    }

    return res;
}

static unsigned char dosamp_FAR * dosamp_FAR soundblaster_mmap_write(soundcard_t sc,uint32_t dosamp_FAR * const howmuch,uint32_t want) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);
    uint32_t ret_pos;
    uint32_t m;

    if (card == NULL) return NULL;

    /* simpify I/O by only allowing a write that does not wrap around */

    /* determine how much can be written */
    if (card->buffer_last_io > card->buffer_size) return NULL;

    m = card->buffer_size - card->buffer_last_io;
    if (want > m) want = m;

    /* but, must be aligned to the block alignment of the format */
    want -= want % sc->cur_codec.bytes_per_block;

    /* this is what we will return */
    *howmuch = want;
    ret_pos = card->buffer_last_io;

    if (want != 0) {
        /* advance I/O. caller MUST fill in the buffer. */
        sc->wav_state.write_counter += want;
        card->buffer_last_io += want;
        if (card->buffer_last_io >= card->buffer_size)
            card->buffer_last_io = 0;

        /* now that audio has been written, buffer is no longer empty */
        sc->wav_state.play_empty = 0;

        /* update */
        soundblaster_update_wav_dma_position(sc,card);
        soundblaster_update_wav_play_delay(sc,card);
    }

    /* return ptr */
    return dosamp_ptr_add_normalize(card->buffer_lin,ret_pos);
}

/* non-mmap write (much like OSS or ALSA in Linux where you do not have direct access to the hardware buffer) */
static unsigned int dosamp_FAR soundblaster_buffer_write(soundcard_t sc,const unsigned char dosamp_FAR * buf,unsigned int len) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);
    unsigned char dosamp_FAR * dst;
    unsigned int r = 0;
    uint32_t todo;

    if (card == NULL) return 0;

    while (len > 0) {
        dst = soundblaster_mmap_write(sc,&todo,(uint32_t)len);
        if (dst == NULL || todo == 0) break;

#if TARGET_MSDOS == 16
        _fmemcpy(dst,dosamp_cptr_add_normalize(buf,r),todo);
#else
        memcpy(dst,buf+r,todo);
#endif

        len -= todo;
        r += todo;
    }

    return r;
}

static int dosamp_FAR soundblaster_open(soundcard_t sc) {
    if (sc->wav_state.is_open) return -1; /* already open! */

    {
        struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);

        if (card == NULL) return -1; /* no such card */
    }

    sc->wav_state.is_open = 1;
    return 0;
}

static int dosamp_FAR soundblaster_close(soundcard_t sc) {
    if (sc->wav_state.is_open) {
        sc->wav_state.is_open = 0;
    }

    return 0;
}

static int dosamp_FAR soundblaster_poll(soundcard_t sc) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);

    if (card == NULL) return -1;

    sndsb_main_idle(card);

    soundblaster_update_wav_dma_position(sc,card);
    soundblaster_update_wav_play_delay(sc,card);

    return 0;
}

static int dosamp_FAR soundblaster_irq_callback(soundcard_t sc) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);
    unsigned char c;

    if (card == NULL) return -1;

    card->irq_counter++;

    /* ack soundblaster DSP if DSP was the cause of the interrupt */
    /* NTS: Experience says if you ack the wrong event on DSP 4.xx it
       will just re-fire the IRQ until you ack it correctly...
       or until your program crashes from stack overflow, whichever
       comes first */
    c = sndsb_interrupt_reason(card);
    sndsb_interrupt_ack(card,c);

    /* FIXME: The sndsb library should NOT do anything in
       send_buffer_again() if it knows playback has not started! */
    /* for non-auto-init modes, start another buffer */
    if (sc->wav_state.playing) sndsb_irq_continue(card,c);

    return 0;
}

/* private */
static int soundblaster_get_autoinit(soundcard_t sc) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);

    if (card == NULL) return -1;

    return card->dsp_autoinit_dma && card->dsp_autoinit_command ? 1 : 0;
}

/* private */
static int soundblaster_set_autoinit(soundcard_t sc,uint8_t flag) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);

    /* not while prepared or playing!
     * assume: playing is not set unless prepared */
    if (sc->wav_state.prepared) return -1;

    if (card == NULL) return -1;

    card->dsp_autoinit_dma = flag;
    card->dsp_autoinit_command = flag;

    return 0;
}

/* private */
static int soundblaster_silence_buffer(soundcard_t sc) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);

    if (card == NULL) return -1;

    if (card->buffer_lin != NULL) {
#if TARGET_MSDOS == 16
        _fmemset(card->buffer_lin,sc->cur_codec.bits_per_sample == 8 ? 128 : 0,card->buffer_size);
#else
        memset(card->buffer_lin,sc->cur_codec.bits_per_sample == 8 ? 128 : 0,card->buffer_size);
#endif
    }

    return 0;
}

static int soundblaster_prepare_play(soundcard_t sc) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);

    if (card == NULL) return -1;

    /* format must be set */
    if (sc->cur_codec.sample_rate == 0)
        return -1;

    /* must be open */
    if (!sc->wav_state.is_open)
        return -1;

    if (sc->wav_state.prepared)
        return 0;

    if (card->dsp_play_method == SNDSB_DSPOUTMETHOD_DIRECT)
        return -1;

    /* prepare DSP */
    {
        uint32_t choice_rate = sc->cur_codec.sample_rate;

        if (sc->p.soundblaster.rate_rounding)
            choice_rate = sc->p.soundblaster.user_rate;

        if (!sndsb_prepare_dsp_playback(card,/*rate*/choice_rate,/*stereo*/sc->cur_codec.number_of_channels > 1,/*16-bit*/sc->cur_codec.bits_per_sample > 8))
            return -1;
    }

    wav_reset_state(&sc->wav_state);

    sndsb_setup_dma(card);
    soundblaster_update_wav_dma_position(sc,card);
    soundblaster_update_wav_play_delay(sc,card);
    sc->wav_state.prepared = 1;
    return 0;
}

static int soundblaster_unprepare_play(soundcard_t sc) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);

    if (card == NULL) return -1;
    if (sc->wav_state.playing) return -1;

    if (sc->wav_state.prepared) {
        sndsb_stop_dsp_playback(card);
        sc->wav_state.prepared = 0;
    }

    return 0;
}

static uint32_t soundblaster_play_buffer_size(soundcard_t sc) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);

    if (card == NULL) return 0;

    return card->buffer_size;
}

static uint32_t soundblaster_play_buffer_play_pos(soundcard_t sc) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);

    if (card == NULL) return 0;
    soundblaster_update_wav_dma_position(sc,card);

    return sc->wav_state.dma_position;
}

static uint32_t soundblaster_play_buffer_write_pos(soundcard_t sc) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);

    if (card == NULL) return 0;

    return card->buffer_last_io;
}

static uint32_t soundblaster_set_irq_interval(soundcard_t sc,uint32_t x) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);
    uint32_t t;

    if (card == NULL) return 0;

    /* you cannot set IRQ interval once prepared */
    if (sc->wav_state.prepared) return card->buffer_irq_interval;

    /* keep it sample aligned */
    x -= x % sc->cur_codec.bytes_per_block;

    if (x != 0UL) {
        /* minimum */
        t = ((sc->cur_codec.sample_rate + 127UL) / 128UL) * sc->cur_codec.bytes_per_block;
        if (x < t) x = t;
        if (x > card->buffer_size) x = card->buffer_size;
    }
    else {
        x = card->buffer_size;
    }

    card->buffer_irq_interval = x;
    return card->buffer_irq_interval;
}

static int soundblaster_start_playback(soundcard_t sc) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);

    if (card == NULL) return -1;
    if (!sc->wav_state.prepared) return -1;
    if (sc->wav_state.playing) return 0;

    if (!sndsb_begin_dsp_playback(card))
        return -1;

    _cli();
    sc->wav_state.playing = 1;
    _sti();

    return 0;
}

static int soundblaster_stop_playback(soundcard_t sc) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);

    if (card == NULL) return -1;
    if (!sc->wav_state.playing) return 0;

    _cli();
    soundblaster_update_wav_dma_position(sc,card);
    soundblaster_update_wav_play_delay(sc,card);
    _sti();

    sndsb_stop_dsp_playback(card);

    _cli();
    sc->wav_state.playing = 0;
    _sti();

    return 0;
}

static int8_t soundblaster_will_use_isa_dma_channel(soundcard_t sc) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);

    if (card == NULL) return -1;

    return sndsb_dsp_playback_will_use_dma_channel(card,sc->cur_codec.sample_rate,/*stereo*/sc->cur_codec.number_of_channels > 1,/*16-bit*/sc->cur_codec.bits_per_sample > 8);
}

static uint32_t soundblaster_recommended_isa_dma_buffer_size(soundcard_t sc,uint32_t limit/*no limit == 0*/) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);
    int8_t ch;

    if (card == NULL) return 0;

    ch = soundblaster_will_use_isa_dma_channel(sc);
    if (ch >= 4)
        return sndsb_recommended_16bit_dma_buffer_size(card,limit);
    else if (ch >= 0)
        return sndsb_recommended_dma_buffer_size(card,limit);

    return 0;
}

static int soundblaster_assign_isa_dma_buffer(soundcard_t sc,struct dma_8237_allocation dosamp_FAR *dma) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);

    /* not while prepared or playing!
     * assume: playing is not set unless prepared */
    if (sc->wav_state.prepared) return -1;

    if (card == NULL) return -1;

    /* NTS: We WANT to call sndsb_assign_dma_buffer with sb_dma == NULL if it happens because it tells the Sound Blaster library to cancel it's copy as well */
    if (dma != NULL) {
        struct dma_8237_allocation dma_tmp = *dma;

        if (!sndsb_assign_dma_buffer(card,&dma_tmp))
            return -1;

        /* we want the DMA buffer region actually used by the card to be a multiple of (2 x the block size we play audio with).
         * we can let that be less than the actual DMA buffer by some bytes, it's fine. */
        {
            uint32_t adj = 2UL * (unsigned long)sc->cur_codec.bytes_per_block;

            card->buffer_size -= card->buffer_size % adj;
            if (card->buffer_size == 0UL) return -1;
        }
    }
    else {
        if (!sndsb_assign_dma_buffer(card,NULL))
            return -1;
    }

    return 0;
}

static int soundblaster_set_play_format(soundcard_t sc,struct wav_cbr_t dosamp_FAR * const fmt) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);

    if (card == NULL) return -1;

    /* must be open */
    if (!sc->wav_state.is_open) return -1;

    /* not while prepared or playing!
     * assume: playing is not set unless prepared */
    if (sc->wav_state.prepared) return -1;

    /* stereo -> mono conversion if needed (if DSP doesn't support stereo) */
    if (fmt->number_of_channels == 2 && card->dsp_vmaj < 3) fmt->number_of_channels = 1;

    /* 16-bit -> 8-bit if needed (if DSP doesn't support 16-bit) */
    if (fmt->bits_per_sample == 16) {
        if (card->is_gallant_sc6600 && card->dsp_vmaj >= 3) /* SC400 and DSP version 3.xx or higher: OK */
            { }
        else if (card->ess_extensions && card->dsp_vmaj >= 2) /* ESS688 and DSP version 2.xx or higher: OK */
            { }
        else if (card->dsp_vmaj >= 4) /* DSP version 4.xx or higher (SB16): OK */
            { }
        else
            fmt->bits_per_sample = 8;
    }

    /* you need a Sound Blaster Pro or better for stereo */
	if (fmt->number_of_channels != 1 && card->dsp_vmaj < 3)
        fmt->number_of_channels = 1;

    /* Sound Blaster is mono or stereo, nothing else */
    if (fmt->number_of_channels < 1 || fmt->number_of_channels > 2) /* SB is mono or stereo, nothing else. */
        return -1;
    /* Sound Blaster is 8-bit or 16-bit, nothing else */
    if (fmt->bits_per_sample < 8 || fmt->bits_per_sample > 16) /* SB is 8-bit. SB16 and ESS688 is 16-bit. */
        return -1;

    /* sample rate limits */
    if (fmt->sample_rate < 4000)
        fmt->sample_rate = 4000;

    if (card->dsp_play_method >= SNDSB_DSPOUTMETHOD_4xx ||
        (card->ess_chipset != 0 && card->ess_extensions) ||
        card->is_gallant_sc6600) {
        if (fmt->sample_rate > card->max_sample_rate_dsp4xx)
            fmt->sample_rate = card->max_sample_rate_dsp4xx;

        sc->p.soundblaster.user_rate = fmt->sample_rate;

        /* ESS chipsets also have their fair share of rounding issues with sample rate */
        if (card->ess_chipset != 0 && card->ess_extensions) {
            if (sc->p.soundblaster.rate_rounding) {
                int b;

                /* this matches sndsb behavior */
                if (fmt->sample_rate > 22050UL) {
                    b = 256 - (795500UL / (unsigned long)fmt->sample_rate);
                    if (b < 0x80) b = 0x80;

                    fmt->sample_rate = 795500UL / (256 - b);
                }
                else {
                    b = 128 - (397700UL / (unsigned long)fmt->sample_rate);
                    if (b < 0) b = 0;

                    fmt->sample_rate = 397700UL / (128 - b);
                }
            }
        }
    }
    else {
        if (card->dsp_play_method >= SNDSB_DSPOUTMETHOD_201/*highspeed support*/) {
            const unsigned long max = card->max_sample_rate_sb_hispeed / fmt->number_of_channels;

            if (fmt->sample_rate > max)
                fmt->sample_rate = max;
        }
        else {
            const unsigned long max = card->max_sample_rate_sb_play / fmt->number_of_channels;

            if (fmt->sample_rate > max)
                fmt->sample_rate = max;
        }

        /* but if the time constant is involved, then the actual sample rate is slightly different */
        if (sc->p.soundblaster.rate_rounding) {
            uint8_t tc = sndsb_rate_to_time_constant(card,(unsigned long)fmt->sample_rate * (unsigned long)fmt->number_of_channels);

            /* we must keep a copy of the actual sample rate desired by the user.
             * remember we overwrite the sample_rate with the approximation offered by the time constant.
             * the sndsb library does the same.
             * the double conversion can cause this code and sndsb to come up with different (off by 1) time constants.
             *
             * we could reduce the overall complexity of this code by eventually sending these TC DSP commands ourselves
             * and computing the TC ourself. */
            sc->p.soundblaster.user_rate = fmt->sample_rate;

            fmt->sample_rate = (1000000UL / (unsigned long)(256 - tc)) / (unsigned long)fmt->number_of_channels;
        }
    }

    /* PCM recalc */
    fmt->bytes_per_block = ((fmt->bits_per_sample+7U)/8U) * fmt->number_of_channels;

    /* take it */
    sc->cur_codec = *fmt;

    return 0;
}

static int soundblaster_get_irq(soundcard_t sc) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);

    if (card == NULL) return -1;

    return card->irq;
}

static uint32_t soundblaster_read_irq_counter(soundcard_t sc) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);

    if (card == NULL) return -1;

    return card->irq_counter;
}

static int soundblaster_get_card_name(soundcard_t sc,void dosamp_FAR *data,unsigned int dosamp_FAR *len) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);
    const char *str;

    if (card == NULL) return -1;
    if (data == NULL || len == NULL) return -1;
    if (*len == 0U) return -1;

    if (card->pnp_name != NULL)
        str = card->pnp_name;
    else if (card->ess_chipset > 0) {
        str = sndsb_ess_chipset_str(card->ess_chipset);
    }
    else if (card->dsp_vmaj >= 4) {
        if (card->aweio != 0)
            str = "Sound Blaster 16 AWE";
        else
            str = "Sound Blaster 16";
    }
    else if (card->dsp_vmaj >= 3) {
        str = "Sound Blaster Pro";
    }
    else {
        str = "Sound Blaster";
    }

    soundcard_str_return_common((char dosamp_FAR*)data,len,str);
    return 0;
}

static int soundblaster_get_card_detail(soundcard_t sc,void dosamp_FAR *data,unsigned int dosamp_FAR *len) {
    struct sndsb_ctx *card = soundblaster_get_sndsb_ctx(sc);
    char *w;

    if (card == NULL) return -1;
    if (data == NULL || len == NULL) return -1;
    if (*len == 0U) return -1;

    w = soundcard_str_tmp;
    w += sprintf(w,"at %03Xh",card->baseio);

    if (card->irq >= 0)
        w += sprintf(w," IRQ %d",card->irq);

    if (card->dma8 >= 0)
        w += sprintf(w," DMA %d",card->dma8);

    if (card->dma16 >= 0)
        w += sprintf(w," HDMA %d",card->dma16);

    if (card->pnp_id != 0UL)
        w += sprintf(w," PnP");

    assert(w < (soundcard_str_tmp+sizeof(soundcard_str_tmp)));

    soundcard_str_return_common((char dosamp_FAR*)data,len,soundcard_str_tmp);
    return 0;
}

static int dosamp_FAR soundblaster_ioctl(soundcard_t sc,unsigned int cmd,void dosamp_FAR *data,unsigned int dosamp_FAR * len,int ival) {
    switch (cmd) {
        case soundcard_ioctl_set_rate_rounding:
            sc->p.soundblaster.rate_rounding = (ival > 0) ? 1 : 0;
            return 0;
        case soundcard_ioctl_get_card_name:
            return soundblaster_get_card_name(sc,data,len);
        case soundcard_ioctl_get_card_detail:
            return soundblaster_get_card_detail(sc,data,len);
        case soundcard_ioctl_get_irq:
            return soundblaster_get_irq(sc);
        case soundcard_ioctl_set_play_format:
            if (data == NULL || len == 0) return -1;
            if (*len < sizeof(struct wav_cbr_t)) return -1;
            return soundblaster_set_play_format(sc,(struct wav_cbr_t dosamp_FAR *)data);
        case soundcard_ioctl_read_irq_counter: {
            uint32_t dosamp_FAR *p = (uint32_t dosamp_FAR*)data;

            if (data == NULL || len == NULL) return -1;
            if (*len < sizeof(*p)) return -1;
            *p = soundblaster_read_irq_counter(sc);
            } return 0;
        case soundcard_ioctl_silence_buffer:
            return soundblaster_silence_buffer(sc);
        case soundcard_ioctl_isa_dma_channel:
            return soundblaster_will_use_isa_dma_channel(sc);
        case soundcard_ioctl_isa_dma_recommended_buffer_size: {
            uint32_t dosamp_FAR *p = (uint32_t dosamp_FAR*)data;

            if (data == NULL || len == NULL) return -1;
            if (*len < sizeof(*p)) return -1;
            *p = soundblaster_recommended_isa_dma_buffer_size(sc,*p);
            } return 0;
        case soundcard_ioctl_isa_dma_assign_buffer:
            if (data != NULL) {
                if (len == NULL) return -1;
                if (*len < sizeof(struct dma_8237_allocation)) return -1;
                return soundblaster_assign_isa_dma_buffer(sc,(struct dma_8237_allocation dosamp_FAR*)data);
            }
            else {
                return soundblaster_assign_isa_dma_buffer(sc,NULL);
            }
        case soundcard_ioctl_get_autoinit:
            return soundblaster_get_autoinit(sc);
        case soundcard_ioctl_set_autoinit:
            return soundblaster_set_autoinit(sc,(uint8_t)(ival > 0 ? 1 : 0));
        case soundcard_ioctl_prepare_play:
            return soundblaster_prepare_play(sc);
        case soundcard_ioctl_unprepare_play:
            return soundblaster_unprepare_play(sc);
        case soundcard_ioctl_start_play:
            return soundblaster_start_playback(sc);
        case soundcard_ioctl_stop_play:
            return soundblaster_stop_playback(sc);
        case soundcard_ioctl_get_buffer_write_position: {
            if (data == NULL || len == 0) return -1;
            if (*len < sizeof(uint32_t)) return -1;
            if ((*((uint32_t dosamp_FAR*)data) = soundblaster_play_buffer_write_pos(sc)) == 0) return -1;
            } return 0;
        case soundcard_ioctl_get_buffer_play_position: {
            if (data == NULL || len == 0) return -1;
            if (*len < sizeof(uint32_t)) return -1;
            if ((*((uint32_t dosamp_FAR*)data) = soundblaster_play_buffer_play_pos(sc)) == 0) return -1;
            } return 0;
        case soundcard_ioctl_get_buffer_size: {
            if (data == NULL || len == 0) return -1;
            if (*len < sizeof(uint32_t)) return -1;
            if ((*((uint32_t dosamp_FAR*)data) = soundblaster_play_buffer_size(sc)) == 0) return -1;
            } return 0;
        case soundcard_ioctl_set_irq_interval: {
            uint32_t dosamp_FAR *p = (uint32_t dosamp_FAR*)data;

            if (data == NULL || len == 0) return -1;
            if (*len < sizeof(uint32_t)) return -1;

            *p = soundblaster_set_irq_interval(sc,*p);
            return 0;
        }
    }

    return -1;
}

struct soundcard soundblaster_soundcard_template = {
    .driver =                                   soundcard_soundblaster,
    .capabilities =                             0,
    .requirements =                             0,
    .can_write =                                soundblaster_can_write,
    .open =                                     soundblaster_open,
    .close =                                    soundblaster_close,
    .poll =                                     soundblaster_poll,
    .clamp_if_behind =                          soundblaster_clamp_if_behind,
    .irq_callback =                             soundblaster_irq_callback,
    .write =                                    soundblaster_buffer_write,
    .mmap_write =                               soundblaster_mmap_write,
    .ioctl =                                    soundblaster_ioctl,
    .p.soundblaster.index =                     -1,
    .p.soundblaster.rate_rounding =             0
};

void free_sound_blaster_support(void) {
    unsigned int i;

    for (i=0;i < SNDSB_MAX_CARDS;i++) {
        struct sndsb_ctx *cx = sndsb_index_to_ctx(i);
        sndsb_free_card(cx);
    }

    free_sndsb(); /* will also de-ref/unhook the NMI reflection */
}

int probe_for_sound_blaster(void) {
    unsigned int i;

    if (!init_sndsb())
        return -1;

#if !defined(TARGET_PC98)
    /* we want to know if certain emulation TSRs exist */
    gravis_mega_em_detect(&megaem_info);
    gravis_sbos_detect();
#endif

    /* it's up to us now to tell it certain minor things */
    sndsb_detect_virtualbox();      // whether or not we're running in VirtualBox
    /* sndsb now allows us to keep the EXE small by not referring to extra sound card support */
    sndsb_enable_sb16_support();        // SB16 support
    sndsb_enable_sc400_support();       // SC400 support
    sndsb_enable_ess_audiodrive_support();  // ESS AudioDrive support

#if !defined(TARGET_PC98)
    /* Plug & Play scan */
    if (has_isa_pnp_bios()) {
        const unsigned int devnode_raw_sz = 4096U;
        unsigned char *devnode_raw = malloc(devnode_raw_sz);

        if (devnode_raw != NULL) {
            unsigned char csn,node=0,numnodes=0xFF,data[192];
            unsigned int j,nodesize=0;
            const char *whatis = NULL;

            memset(data,0,sizeof(data));
            if (isa_pnp_bios_get_pnp_isa_cfg(data) == 0) {
                struct isapnp_pnp_isa_cfg *nfo = (struct isapnp_pnp_isa_cfg*)data;
                isapnp_probe_next_csn = nfo->total_csn;
                isapnp_read_data = nfo->isa_pnp_port;
            }

            /* enumerate device nodes reported by the BIOS */
            if (isa_pnp_bios_number_of_sysdev_nodes(&numnodes,&nodesize) == 0 && numnodes != 0xFF && nodesize <= devnode_raw_sz) {
                for (node=0;node != 0xFF;) {
                    struct isa_pnp_device_node far *devn;
                    unsigned char this_node;

                    /* apparently, start with 0. call updates node to
                     * next node number, or 0xFF to signify end */
                    this_node = node;
                    if (isa_pnp_bios_get_sysdev_node(&node,devnode_raw,ISA_PNP_BIOS_GET_SYSDEV_NODE_CTRL_NOW) != 0) break;

                    devn = (struct isa_pnp_device_node far*)devnode_raw;
                    if (isa_pnp_is_sound_blaster_compatible_id(devn->product_id,&whatis)) {
                        if (sndsb_try_isa_pnp_bios(devn->product_id,this_node,devn,devnode_raw_sz) > 0)
                            printf("PnP: Found %s\n",whatis);
                    }
                }
            }

            /* enumerate the ISA bus directly */
            if (isapnp_read_data != 0) {
                for (csn=1;csn < 255;csn++) {
                    isa_pnp_init_key();
                    isa_pnp_wake_csn(csn);

                    isa_pnp_write_address(0x06); /* CSN */
                    if (isa_pnp_read_data() == csn) {
                        /* apparently doing this lets us read back the serial and vendor ID in addition to resource data */
                        /* if we don't, then we only read back the resource data */
                        isa_pnp_init_key();
                        isa_pnp_wake_csn(csn);

                        for (j=0;j < 9;j++) data[j] = isa_pnp_read_config();

                        if (isa_pnp_is_sound_blaster_compatible_id(*((uint32_t*)data),&whatis)) {
                            if (sndsb_try_isa_pnp(*((uint32_t*)data),csn) > 0)
                                printf("PnP: Found %s\n",whatis);
                        }
                    }

                    /* return back to "wait for key" state */
                    isa_pnp_write_data_register(0x02,0x02); /* bit 1: set -> return to Wait For Key state (or else a Pentium Pro system I own eventually locks up and hangs) */
                }
            }

            free(devnode_raw);
        }
    }
#endif

    /* Non-plug & play scan: BLASTER environment variable */
    if (sndsb_try_blaster_var() != NULL) {
        if (!sndsb_init_card(sndsb_card_blaster))
            sndsb_free_card(sndsb_card_blaster);
    }

#if defined(TARGET_PC98)
    /* Non-plug & play scan: Most SB cards exist at xxD2h or xxD4h */
    sndsb_try_base(0xD2);
    sndsb_try_base(0xD4);
#else
    /* Non-plug & play scan: Most SB cards exist at 220h or 240h */
    sndsb_try_base(0x220);
    sndsb_try_base(0x240);
#endif

    /* further probing, for IRQ and DMA and other capabilities */
    for (i=0;i < SNDSB_MAX_CARDS;i++) {
        struct sndsb_ctx *cx = sndsb_index_to_ctx(i);
        if (cx->baseio == 0) continue;

        if (cx->irq < 0)
            sndsb_probe_irq_F2(cx);
        if (cx->irq < 0)
            sndsb_probe_irq_80(cx);
        if (cx->dma8 < 0)
            sndsb_probe_dma8_E2(cx);
        if (cx->dma8 < 0)
            sndsb_probe_dma8_14(cx);

        // having IRQ and DMA changes the ideal playback method and capabilities
        sndsb_update_capabilities(cx);
        sndsb_determine_ideal_dsp_play_method(cx);
    }

    /* add detected cards to soundcard list */
    {
        soundcard_t sc;
        unsigned int i;

        for (i=0;i < SNDSB_MAX_CARDS;i++) {
            struct sndsb_ctx *cx = sndsb_index_to_ctx(i);
            if (cx->baseio == 0) continue;

            sc = soundcardlist_new(&soundblaster_soundcard_template);
            if (sc == NULL) continue;

            sc->p.soundblaster.index = i;
            sc->requirements = soundcard_requirements_isa_dma;
            sc->capabilities = soundcard_caps_mmap_write | soundcard_caps_8bit | soundcard_caps_isa_dma;
            if (cx->irq >= 0) {
                sc->requirements |= soundcard_requirements_irq;
                sc->capabilities |= soundcard_caps_irq;
            }
            if (cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_4xx)
                sc->capabilities |= soundcard_caps_16bit;
            else if (cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_3xx && cx->ess_extensions)
                sc->capabilities |= soundcard_caps_16bit;
        }
    }

    /* OK. done */
    return 0;
}

#endif /* defined(HAS_SNDSB) */

