
#include <stdio.h>
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
#include <dos.h>

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

/* this code won't work with the TINY memory model for awhile. sorry. */
#ifdef __TINY__
# error Open Watcom C tiny memory model not supported
#endif

/* file source */
dosamp_file_source_t dosamp_file_source_file_fd_open(const char * const path);

/* DOSAMP state and user state */
static unsigned long                            prefer_rate = 0;
static unsigned char                            prefer_channels = 0;
static unsigned char                            prefer_bits = 0;
static unsigned char                            prefer_no_clamp = 0;

/* DOSAMP debug state */
static char                                     stuck_test = 0;
static unsigned char                            use_mmap_write = 1;

/* chosen time source.
 * NTS: Don't forget that by design, some time sources (8254 for example)
 *      require you to poll often enough to get a correct count of time. */
static dosamp_time_source_t                     time_source = NULL;

/* ISA DMA buffer */
static struct dma_8237_allocation*              isa_dma = NULL;

/* sound card list */
struct soundcard                                soundcardlist[SOUNDCARDLIST_MAX];
unsigned int                                    soundcardlist_count;
unsigned int                                    soundcardlist_alloc;

/* chosen file to play */
static dosamp_file_source_t                     wav_source = NULL;
static char*                                    wav_file = NULL;

/* convert/read buffer */
struct convert_rdbuf_t                          convert_rdbuf = {NULL,0,0,0};

struct wav_cbr_t                                file_codec;
struct wav_cbr_t                                play_codec;

int soundcardlist_init(void) {
    soundcardlist_count = 0;
    soundcardlist_alloc = 0;
    return 0;
}

void soundcardlist_close(void) {
    soundcardlist_count = 0;
    soundcardlist_alloc = 0;
}

soundcard_t soundcardlist_new(const soundcard_t template) {
    while (soundcardlist_alloc < soundcardlist_count) {
        if (soundcardlist[soundcardlist_alloc].driver == soundcard_none) {
            soundcardlist[soundcardlist_alloc] = *template;
            return &soundcardlist[soundcardlist_alloc++];
        }
    }

    if (soundcardlist_count < SOUNDCARDLIST_MAX) {
        soundcardlist[soundcardlist_count] = *template;
        return &soundcardlist[soundcardlist_count++];
    }

    return NULL;
}

soundcard_t soundcardlist_free(const soundcard_t sc) {
    if (sc != NULL) {
        soundcardlist_alloc = 0;
        sc->driver = soundcard_none;
    }

    return NULL;
}

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
    if (!sndsb_prepare_dsp_playback(card,/*rate*/sc->cur_codec.sample_rate,/*stereo*/sc->cur_codec.number_of_channels > 1,/*16-bit*/sc->cur_codec.bits_per_sample > 8))
        return -1;

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
    uint32_t oph,osz;
    int r;

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

    if (fmt->number_of_channels < 1 || fmt->number_of_channels > 2) /* SB is mono or stereo, nothing else. */
        return -1;
    if (fmt->bits_per_sample < 8 || fmt->bits_per_sample > 16) /* SB is 8-bit. SB16 and ESS688 is 16-bit. */
        return -1;

    /* HACK! */
    osz = card->buffer_size; card->buffer_size = 1;
    oph = card->buffer_phys; card->buffer_phys = 0;

    /* SB specific: I know from experience and calculations that Sound Blaster cards don't go below 4000Hz */
    if (fmt->sample_rate < 4000)
        fmt->sample_rate = 4000;

    /* we'll follow the recommendations on what is supported by the DSP. no hacks. */
    r = sndsb_dsp_out_method_supported(card,fmt->sample_rate,/*stereo*/fmt->number_of_channels > 1 ? 1 : 0,/*16-bit*/fmt->bits_per_sample > 8 ? 1 : 0);
    if (!r) {
        /* we already made concessions for channels/bits, so try sample rate */
        fmt->sample_rate = 44100;
        r = sndsb_dsp_out_method_supported(card,fmt->sample_rate,/*stereo*/fmt->number_of_channels > 1 ? 1 : 0,/*16-bit*/fmt->bits_per_sample > 8 ? 1 : 0);
    }
    if (!r) {
        /* we already made concessions for channels/bits, so try sample rate */
        fmt->sample_rate = 22050;
        r = sndsb_dsp_out_method_supported(card,fmt->sample_rate,/*stereo*/fmt->number_of_channels > 1 ? 1 : 0,/*16-bit*/fmt->bits_per_sample > 8 ? 1 : 0);
    }
    if (!r) {
        /* we already made concessions for channels/bits, so try sample rate */
        fmt->sample_rate = 11025;
        r = sndsb_dsp_out_method_supported(card,fmt->sample_rate,/*stereo*/fmt->number_of_channels > 1 ? 1 : 0,/*16-bit*/fmt->bits_per_sample > 8 ? 1 : 0);
    }

    /* HACK! */
    card->buffer_size = osz;
    card->buffer_phys = oph;

    if (!r) {
        if (card->reason_not_supported != NULL && *(card->reason_not_supported) != 0)
            printf("Negotiation failed (SB) even with %luHz %u-channel %u-bit:\n    %s\n",
                fmt->sample_rate,
                fmt->number_of_channels,
                fmt->bits_per_sample,
                card->reason_not_supported);

        return -1;
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

static int dosamp_FAR soundblaster_ioctl(soundcard_t sc,unsigned int cmd,void dosamp_FAR *data,unsigned int dosamp_FAR * len,int ival) {
    switch (cmd) {
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
    .p.soundblaster.index =                     -1
};

soundcard_t                                     soundcard = NULL;

void wav_reset_state(struct wav_state_t dosamp_FAR * const w) {
    w->play_counter_prev = 0;
    w->write_counter = 0;
    w->play_counter = 0;
    w->play_delay = 0;
    w->play_empty = 1;
}

/* WAV file data chunk info */
static unsigned long                    wav_data_offset = 44;
static unsigned long                    wav_data_length_bytes = 0;
static unsigned long                    wav_data_length = 0;/* in samples */;

/* WAV playback state */
static unsigned long                    wav_position = 0;/* in samples. read pointer. after reading, points to next sample to read. */
static unsigned long                    wav_play_position = 0L;

/* buffering threshholds */
static unsigned long                    wav_play_load_block_size = 0;/*max load per call*/
static unsigned long                    wav_play_min_load_size = 0;/*minimum "can write" threshhold to load more*/

/* WARNING!!! This interrupt handler calls subroutines. To avoid system
 * instability in the event the IRQ fires while, say, executing a routine
 * in the DOS kernel, you must compile this code with the -zu flag in
 * 16-bit real mode Large and Compact memory models! Without -zu, minor
 * memory corruption in the DOS kernel will result and the system will
 * hang and/or crash. */

static void interrupt soundcard_irq_handler() {
    /* assume:
     *  - this IRQ handler will not be called UNLESS the IRQ has been hooked.
     *  - this IRQ will not be hooked unless the sound card uses an IRQ as part of the protocol.
     *  - this IRQ handler will not be called unless irq_number has been filled in.
     *  - old_handler contains the prior IRQ handler.
     *  - chain_irq has been filled in */
    soundcard->irq_callback(soundcard);

    if (soundcard_irq.chain_irq) {
        /* chain to the previous IRQ, who will acknowledge the interrupt */
        soundcard_irq.old_handler();
    }
    else {
        /* ack the interrupt ourself, do not chain */
        if (soundcard_irq.irq_number >= 8) p8259_OCW2(8,P8259_OCW2_NON_SPECIFIC_EOI);
        p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
    }
}

int wav_rewind(void) {
    wav_position = 0;
    if (wav_source->seek(wav_source,(dosamp_file_off_t)wav_data_offset) != (dosamp_file_off_t)wav_data_offset) return -1;
    return 0;
}

int wav_file_pointer_to_position(void) {
    if (wav_source->file_pos >= wav_data_offset) {
        wav_position  = wav_source->file_pos - wav_data_offset;
        wav_position += convert_rdbuf.pos - convert_rdbuf.len;
        wav_position /= file_codec.bytes_per_block;
    }
    else {
        wav_position = 0;
    }

    return 0;
}

int wav_position_to_file_pointer(void) {
    off_t ofs = (off_t)wav_data_offset + ((off_t)wav_position * (off_t)file_codec.bytes_per_block);

    if (wav_source->seek(wav_source,(dosamp_file_off_t)ofs) != (dosamp_file_off_t)ofs)
        return wav_rewind();

    return 0;
}

void wav_rebase_position_event(void) {
    /* make a rebase event */
    struct audio_playback_rebase_t *r = rebase_add();

    if (r != NULL) {
        r->event_at = soundcard->wav_state.write_counter;
        r->wav_position = wav_position;
    }
}

int convert_rdbuf_fill(void) {
    unsigned char dosamp_FAR * buf;
    dosamp_file_off_t rem;
    uint32_t towrite,xx;
    uint32_t bufsz;

    if (convert_rdbuf.pos >= convert_rdbuf.len) {
        uint32_t samples;
        size_t of;

        convert_rdbuf.pos = convert_rdbuf.len = 0;

        buf = convert_rdbuf_get(&bufsz);
        if (buf == NULL) return -1;
        of = bufsz;

        /* factor buffer size into upconversion: mono to stereo */
        if (play_codec.number_of_channels > file_codec.number_of_channels) {
            bufsz *= file_codec.number_of_channels;
            bufsz /= play_codec.number_of_channels;
        }

        /* factor buffer size into upconversion: 8 to 16 bit */
        if (play_codec.bits_per_sample > file_codec.bits_per_sample) {
            bufsz *= file_codec.bits_per_sample;
            bufsz /= play_codec.bits_per_sample;
        }

        /* sample align */
        bufsz -= bufsz % file_codec.bytes_per_block;
        if (bufsz == 0) return -1;

        /* make sure we did not INCREASE bufsz */
        assert(bufsz <= of);

        /* read and fill */
        while (convert_rdbuf.len < bufsz) {
            rem = wav_data_length_bytes + wav_data_offset;
            if (wav_source->file_pos <= rem)
                rem -= wav_source->file_pos;
            else
                rem = 0;

            /* if we're at the end, seek back around and start again */
            if (rem == 0UL) {
                if (wav_rewind() < 0) return -1;
                wav_rebase_position_event();
                continue;
            }

            xx = bufsz - convert_rdbuf.len;
            if (rem > xx) rem = xx;
            towrite = rem;

            /* read */
            rem = wav_source->file_pos + towrite; /* expected result pos */
            if (wav_source->read(wav_source,dosamp_ptr_add_normalize(buf,convert_rdbuf.len),towrite) != towrite) {
                if (wav_source->seek(wav_source,rem) != rem)
                    return -1;
                if (wav_file_pointer_to_position() < 0)
                    return -1;
                wav_rebase_position_event();
                if (wav_position_to_file_pointer() < 0)
                    return -1;
            }

            wav_file_pointer_to_position();
            convert_rdbuf.len += towrite;
        }

        assert(convert_rdbuf.len <= bufsz);

        samples = (uint32_t)convert_rdbuf.len / (uint32_t)file_codec.bytes_per_block;

        /* channel conversion */
        if (file_codec.number_of_channels == 2 && play_codec.number_of_channels == 1)
            convert_rdbuf.len = convert_ip_stereo2mono(samples,convert_rdbuf.buffer,of,file_codec.bits_per_sample);
        else if (file_codec.number_of_channels == 1 && play_codec.number_of_channels == 2)
            convert_rdbuf.len = convert_ip_mono2stereo(samples,convert_rdbuf.buffer,of,file_codec.bits_per_sample);

        /* bit conversion */
        if (file_codec.bits_per_sample == 16 && play_codec.bits_per_sample == 8)
            convert_rdbuf.len = convert_ip_16_to_8(samples * play_codec.number_of_channels,convert_rdbuf.buffer,of);
        else if (file_codec.bits_per_sample == 8 && play_codec.bits_per_sample == 16)
            convert_rdbuf.len = convert_ip_8_to_16(samples * play_codec.number_of_channels,convert_rdbuf.buffer,of);

        assert(convert_rdbuf.len <= of);
    }

    return 0;
}

static void load_audio_convert(uint32_t howmuch/*in bytes*/) {
    unsigned char dosamp_FAR * ptr;
    uint32_t dop,bsz;
    uint32_t avail;

    avail = soundcard->can_write(soundcard);

    if (howmuch > avail) howmuch = avail;
    if (howmuch < wav_play_min_load_size) return; /* don't want to incur too much DOS I/O */

    while (howmuch > 0) {
        dop = 0;
        ptr = tmpbuffer_get(&bsz);
        if (ptr == NULL || bsz == 0) break;
        if (convert_rdbuf_fill() < 0) break;
        if (bsz > howmuch) bsz = howmuch;
        bsz -= bsz % play_codec.bytes_per_block;

        if (resample_state.step == resample_100) {
            /* don't do full resampling if no resampling needed */
            if (convert_rdbuf.pos < convert_rdbuf.len)
                dop = convert_rdbuf.len - convert_rdbuf.pos;
            else
                dop = 0;

            if (dop > bsz) dop = bsz;

            dop -= dop % play_codec.bytes_per_block;
            if (dop == 0)
                break;

            if (soundcard->write(soundcard,dosamp_ptr_add_normalize(convert_rdbuf.buffer,convert_rdbuf.pos),dop) != dop)
                break;

            convert_rdbuf.pos += dop;
            assert(convert_rdbuf.pos <= convert_rdbuf.len);
            howmuch -= dop;
            avail -= dop;
        }
        else {
            if (resample_state.resample_mode == resample_fast) {
                if (play_codec.bits_per_sample > 8) {
                    if (play_codec.number_of_channels == 2)
                        dop = convert_rdbuf_resample_fast_to_16_stereo((int16_t dosamp_FAR*)ptr,bsz / 4UL);
                    else
                        dop = convert_rdbuf_resample_fast_to_16_mono((int16_t dosamp_FAR*)ptr,bsz / 2UL);
                }
                else {
                    if (play_codec.number_of_channels == 2)
                        dop = convert_rdbuf_resample_fast_to_8_stereo((uint8_t dosamp_FAR*)ptr,bsz / 2UL);
                    else
                        dop = convert_rdbuf_resample_fast_to_8_mono((uint8_t dosamp_FAR*)ptr,bsz);
                }
            }
            else if (resample_state.resample_mode == resample_good) {
                if (play_codec.bits_per_sample > 8) {
                    if (play_codec.number_of_channels == 2)
                        dop = convert_rdbuf_resample_to_16_stereo((int16_t dosamp_FAR*)ptr,bsz / 4UL);
                    else
                        dop = convert_rdbuf_resample_to_16_mono((int16_t dosamp_FAR*)ptr,bsz / 2UL);
                }
                else {
                    if (play_codec.number_of_channels == 2)
                        dop = convert_rdbuf_resample_to_8_stereo((uint8_t dosamp_FAR*)ptr,bsz / 2UL);
                    else
                        dop = convert_rdbuf_resample_to_8_mono((uint8_t dosamp_FAR*)ptr,bsz);
                }
            }
            else if (resample_state.resample_mode == resample_best) {
                if (play_codec.bits_per_sample > 8) {
                    if (play_codec.number_of_channels == 2)
                        dop = convert_rdbuf_resample_best_to_16_stereo((int16_t dosamp_FAR*)ptr,bsz / 4UL);
                    else
                        dop = convert_rdbuf_resample_best_to_16_mono((int16_t dosamp_FAR*)ptr,bsz / 2UL);
                }
                else {
                    if (play_codec.number_of_channels == 2)
                        dop = convert_rdbuf_resample_best_to_8_stereo((uint8_t dosamp_FAR*)ptr,bsz / 2UL);
                    else
                        dop = convert_rdbuf_resample_best_to_8_mono((uint8_t dosamp_FAR*)ptr,bsz);
                }
            }
            else {
                dop = 0;
            }

            assert(convert_rdbuf.pos <= convert_rdbuf.len);

            if (dop != 0) {
                dop *= play_codec.bytes_per_block;
                if (soundcard->write(soundcard,ptr,dop) != dop)
                    break;

                howmuch -= dop;
                avail -= dop;
            }
            else {
                break;
            }
        }
    }

    if (!prefer_no_clamp)
        soundcard->clamp_if_behind(soundcard,wav_play_min_load_size);
}

static void load_audio_copy(uint32_t howmuch/*in bytes*/) { /* load audio up to point or max */
    unsigned char dosamp_FAR * ptr;
    dosamp_file_off_t rem;
    uint32_t towrite;
    uint32_t avail;

    avail = soundcard->can_write(soundcard);

    if (howmuch > avail) howmuch = avail;
    if (howmuch < wav_play_min_load_size) return; /* don't want to incur too much DOS I/O */

    while (howmuch > 0) {
        rem = wav_data_length_bytes + wav_data_offset;
        if (wav_source->file_pos <= rem)
            rem -= wav_source->file_pos;
        else
            rem = 0;

        /* if we're at the end, seek back around and start again */
        if (rem == 0UL) {
            if (wav_rewind() < 0) break;
            wav_rebase_position_event();
            continue;
        }

        /* limit to how much we need */
        if (rem > howmuch) rem = howmuch;

        if (use_mmap_write) {
            /* get the write pointer. towrite is guaranteed to be block aligned */
            ptr = soundcard->mmap_write(soundcard,&towrite,rem);
            if (ptr == NULL || towrite == 0) break;
        }
        else {
            /* prepare the temp buffer, limit ourself to it */
            ptr = tmpbuffer_get(&towrite);
            if (ptr == NULL) break;

            if (rem > towrite) rem = towrite;
            rem -= rem % play_codec.bytes_per_block;
            if (rem == 0) break;
            towrite = rem;
        }

        /* read */
        rem = wav_source->file_pos + towrite; /* expected result pos */
        if (wav_source->read(wav_source,ptr,towrite) != towrite) {
            if (wav_source->seek(wav_source,rem) != rem)
                break;
            if (wav_file_pointer_to_position() < 0)
                break;
            wav_rebase_position_event();
            if (wav_position_to_file_pointer() < 0)
                break;
        }

        /* non-mmap write: send temp buffer to sound card */
        if (!use_mmap_write) {
            if (soundcard->write(soundcard,ptr,towrite) != towrite)
                break;
        }

        /* adjust */
        wav_file_pointer_to_position();
        howmuch -= towrite;
    }

    if (!prefer_no_clamp)
        soundcard->clamp_if_behind(soundcard,wav_play_min_load_size);
}

static void load_audio(uint32_t howmuch/*in bytes*/) { /* load audio up to point or max */
    if (resample_on)
        load_audio_convert(howmuch);
    else
        load_audio_copy(howmuch);
}

void update_play_position(void) {
    struct audio_playback_rebase_t *r;

    r = rebase_find(soundcard->wav_state.play_counter);

    if (r != NULL) {
        wav_play_position =
            ((unsigned long long)(((soundcard->wav_state.play_counter - r->event_at) / play_codec.bytes_per_block) *
                (unsigned long long)resample_state.step) >> (unsigned long long)resample_100_shift) + r->wav_position;
    }
    else if (soundcard->wav_state.playing)
        wav_play_position = 0;
    else
        wav_play_position = wav_position;
}

void clear_remapping(void) {
    wav_rebase_clear();
}

void move_pos(signed long adj) {
    if (soundcard->wav_state.playing)
        return;

    clear_remapping();
    if (adj < 0L) {
        adj = -adj;
        if (wav_position >= (unsigned long)adj)
            wav_position -= (unsigned long)adj;
        else
            wav_position = 0;
    }
    else if (adj > 0L) {
        wav_position += (unsigned long)adj;
        if (wav_position >= wav_data_length)
            wav_position = wav_data_length;
    }

    update_play_position();
}

static void wav_idle() {
    if (!soundcard->wav_state.playing || wav_source == NULL)
        return;

    /* debug */
    convert_rdbuf_check();

    /* update card state */
    soundcard->poll(soundcard);

    /* load more from disk */
    if (!stuck_test) load_audio(wav_play_load_block_size);

    /* update info */
    update_play_position();
}

static void close_wav() {
    if (wav_source != NULL) {
        dosamp_file_source_release(wav_source);
        wav_source->close(wav_source);
        wav_source->free(wav_source);
        wav_source = NULL;
    }
}

static int open_wav() {
    char tmp[64];

    if (wav_source == NULL) {
        uint32_t riff_length,scan,len;

        wav_position = 0;
        wav_data_offset = 0;
        wav_data_length = 0;
        if (wav_file == NULL) return -1;
        if (strlen(wav_file) < 1) return -1;

        wav_source = dosamp_file_source_file_fd_open(wav_file);
        if (wav_source == NULL) return -1;
        dosamp_file_source_addref(wav_source);

        /* first, the RIFF:WAVE chunk */
        /* 3 DWORDS: 'RIFF' <length> 'WAVE' */
        if (wav_source->read(wav_source,tmp,12) != 12) goto fail;
        if (memcmp(tmp+0,"RIFF",4) || memcmp(tmp+8,"WAVE",4)) goto fail;

        scan = 12;
        riff_length = *((uint32_t*)(tmp+4));
        if (riff_length <= 44) goto fail;
        riff_length -= 4; /* the length includes the 'WAVE' marker */

        while ((scan+8UL) <= riff_length) {
            /* RIFF chunks */
            /* 2 WORDS: <fourcc> <length> */
            if (wav_source->seek(wav_source,scan) != scan) goto fail;
            if (wav_source->read(wav_source,tmp,8) != 8) goto fail;
            len = *((uint32_t*)(tmp+4));

            /* process! */
            if (!memcmp(tmp,"fmt ",4)) {
                if (len >= sizeof(windows_WAVEFORMATPCM)/*16*/ && len <= sizeof(tmp)) {
                    if (wav_source->read(wav_source,tmp,len) == len) {
                        windows_WAVEFORMATPCM *wfx = (windows_WAVEFORMATPCM*)tmp;

                        if (le16toh(wfx->nChannels) < 256U && le16toh(wfx->wBitsPerSample) < 256U) {
                            file_codec.number_of_channels = (uint8_t)le16toh(wfx->nChannels);
                            file_codec.bits_per_sample = (uint8_t)le16toh(wfx->wBitsPerSample);
                            file_codec.sample_rate = le32toh(wfx->nSamplesPerSec);
                            file_codec.bytes_per_block = le16toh(wfx->nBlockAlign);
                            file_codec.samples_per_block = 1;

                            if (file_codec.sample_rate >= 1000UL && file_codec.sample_rate <= 96000UL) {
                                if (le16toh(wfx->wFormatTag) == windows_WAVE_FORMAT_PCM) {
                                    if ((file_codec.bits_per_sample >= 8U && file_codec.bits_per_sample <= 16U) &&
                                        (file_codec.number_of_channels >= 1U && file_codec.number_of_channels <= 2U)) {
                                        file_codec.bytes_per_block =
                                            ((file_codec.bits_per_sample + 7U) >> 3U) *
                                            file_codec.number_of_channels;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else if (!memcmp(tmp,"data",4)) {
                wav_data_offset = scan + 8UL;
                wav_data_length_bytes = len;
                wav_data_length = len;
            }

            /* next! */
            scan += len + 8UL;
        }

        if (file_codec.sample_rate == 0UL || wav_data_length == 0UL || wav_data_length_bytes == 0UL) goto fail;
    }

    /* convert length to samples */
    wav_data_length /= file_codec.bytes_per_block;
    wav_data_length *= file_codec.samples_per_block;

    /* tell the user */
    printf("WAV file source: %luHz %u-channel %u-bit\n",
        (unsigned long)file_codec.sample_rate,
        (unsigned int)file_codec.number_of_channels,
        (unsigned int)file_codec.bits_per_sample);

    /* done */
    return 0;
fail:
    dosamp_file_source_release(wav_source);
    dosamp_file_source_autofree(&wav_source);
    wav_source = NULL;
    return -1;
}

static void free_dma_buffer() {
    if (isa_dma != NULL) {
        soundcard->ioctl(soundcard,soundcard_ioctl_isa_dma_assign_buffer,NULL,NULL,0); /* disassociate DMA buffer from sound card */
        dma_8237_free_buffer(isa_dma);
        isa_dma = NULL;
    }
}

static int alloc_dma_buffer(uint32_t choice,int8_t ch) {
    if (ch < 0)
        return 0;
    if (isa_dma != NULL)
        return 0;

    {
        const unsigned char isa_width = (ch >= 4 ? 16 : 8);

        do {
            isa_dma = dma_8237_alloc_buffer_dw(choice,isa_width);

            if (isa_dma != NULL) {
                break;
            }
            else {
                if (choice >= 8192UL)
                    choice -= 4096UL;
                else
                    break;
            }
        } while (1);
    }

    if (isa_dma == NULL)
        return -1;

    return 0;
}

static uint32_t soundcard_isa_dma_recommended_buffer_size(soundcard_t sc,uint32_t limit) {
    unsigned int sz = sizeof(uint32_t);
    uint32_t p = 0;

    if (soundcard->ioctl(soundcard,soundcard_ioctl_isa_dma_recommended_buffer_size,&p,&sz,0) < 0)
        return 0;

    return p;
}

static int realloc_isa_dma_buffer() {
    uint32_t choice;
    int8_t ch;

    free_dma_buffer();

    ch = soundcard->ioctl(soundcard,soundcard_ioctl_isa_dma_channel,NULL,NULL,0);
    if (ch < 0)
        return 0; /* nothing to do */

    choice = soundcard_isa_dma_recommended_buffer_size(soundcard,/*no limit*/0);
    if (choice == 0)
        return -1;

    if (alloc_dma_buffer(choice,ch) < 0)
        return -1;

    {
        unsigned int sz = sizeof(*isa_dma);

        if (soundcard->ioctl(soundcard,soundcard_ioctl_isa_dma_assign_buffer,(void dosamp_FAR*)isa_dma,&sz,0) < 0) {
            free_dma_buffer();
            return -1;
        }
    }

    return 0;
}

int check_dma_buffer(void) {
    if ((soundcard->requirements & soundcard_requirements_isa_dma) ||
        (soundcard->capabilities & soundcard_caps_isa_dma)) {
        int8_t ch;

        /* alloc DMA buffer.
         * if already allocated, then realloc if changing from 8-bit to 16-bit DMA */
        if (isa_dma == NULL)
            realloc_isa_dma_buffer();
        else {
            ch = soundcard->ioctl(soundcard,soundcard_ioctl_isa_dma_channel,NULL,NULL,0);
            if (ch >= 0 && isa_dma->dma_width != (ch >= 4 ? 16 : 8))
                realloc_isa_dma_buffer();
        }

        if (isa_dma == NULL)
            return -1;
    }

    return 0;
}

int prepare_buffer(void) {
    if (check_dma_buffer() < 0)
        return -1;

    return 0;
}

static int set_play_format(struct wav_cbr_t dosamp_FAR * const d,const struct wav_cbr_t dosamp_FAR * const s) {
    if (d == NULL || s == NULL) return -1;
    if (d == s) return -1;

    /* default to match format */
    *d = *s;

    /* allow override */
    if (prefer_rate != 0) d->sample_rate = prefer_rate;
    if (prefer_bits != 0) d->bits_per_sample = prefer_bits;
    if (prefer_channels != 0) d->number_of_channels = prefer_channels;

    /* PCM recalc */
    d->bytes_per_block = ((d->bits_per_sample+7U)/8U) * d->number_of_channels;

    /* call out to soundcard */
    {
        unsigned int sz = sizeof(*d);

        return soundcard->ioctl(soundcard,soundcard_ioctl_set_play_format,(void dosamp_FAR*)d,&sz,0);
    }
}

static int begin_play() {
    if (soundcard->wav_state.playing)
        return 0;

    /* reset state */
    convert_rdbuf_clear();
    wav_rebase_clear();

    if (wav_source == NULL)
        return -1;

    /* choose output vs input */
    if (set_play_format(&play_codec,&file_codec) < 0)
        goto error_out;

    /* based on sound card's choice vs source format, reconfigure resampler */
    if (resampler_init(&resample_state,&play_codec,&file_codec) < 0)
        goto error_out;

    /* prepare buffer */
    if (prepare_buffer() < 0)
        goto error_out;

    /* zero the buffer */
    soundcard->ioctl(soundcard,soundcard_ioctl_silence_buffer,NULL,NULL,0);

    /* set IRQ interval (card will pick closest and sanitize it) */
    {
        unsigned int sz = sizeof(uint32_t);
        uint32_t bufsz = 0;

        if (soundcard->ioctl(soundcard,soundcard_ioctl_get_buffer_size,&bufsz,&sz,0) < 0)
            goto error_out;

        /* might fail, sound card might not have IRQs, don't care. */
        soundcard->ioctl(soundcard,soundcard_ioctl_set_irq_interval,&bufsz,&sz,0);

        /* minimum buffer until loading again (100ms) */
        wav_play_min_load_size = (play_codec.sample_rate / 10 / play_codec.samples_per_block) * play_codec.bytes_per_block;

        /* no more than 1/4 the buffer */
        if (wav_play_min_load_size > (bufsz / 4UL)) wav_play_min_load_size = (bufsz / 4UL);

        /* how much to load per call (100ms per call) */
        wav_play_load_block_size = (play_codec.sample_rate / 10 / play_codec.samples_per_block) * play_codec.bytes_per_block;

        /* no more than half the buffer */
        if (wav_play_load_block_size > (bufsz / 2UL)) wav_play_load_block_size = (bufsz / 2UL);
    }

    /* hook IRQ */
    if ((soundcard->requirements & soundcard_requirements_irq) ||
        (soundcard->capabilities & soundcard_caps_irq)) {
        int irq = soundcard->ioctl(soundcard,soundcard_ioctl_get_irq,NULL,NULL,0);
        if (irq >= 0) {
            if (hook_irq(irq,soundcard_irq_handler) < 0)
                goto error_out;
        }
    }

    /* unmask and reinit IRQ */
    if (init_prepare_irq() < 0)
        goto error_out;

    /* prepare the sound card (buffer, DMA, etc.) */
    if (soundcard->ioctl(soundcard,soundcard_ioctl_prepare_play,NULL,NULL,0) < 0)
        goto error_out;

    /* prepare */
    resampler_state_reset(&resample_state);

    /* preroll */
    wav_position_to_file_pointer();
    wav_rebase_position_event();
    load_audio(wav_play_load_block_size);
    update_play_position();

    /* go! */
    if (soundcard->ioctl(soundcard,soundcard_ioctl_start_play,NULL,NULL,0) < 0)
        goto error_out;

    return 0;
error_out:
    soundcard->ioctl(soundcard,soundcard_ioctl_stop_play,NULL,NULL,0);
    soundcard->ioctl(soundcard,soundcard_ioctl_unprepare_play,NULL,NULL,0);
    unhook_irq();
    return -1;
}

static void stop_play() {
    if (!soundcard->wav_state.playing) return;

    /* stop */
    soundcard->ioctl(soundcard,soundcard_ioctl_stop_play,NULL,NULL,0);
    soundcard->ioctl(soundcard,soundcard_ioctl_unprepare_play,NULL,NULL,0);
    unhook_irq();

    wav_position = wav_play_position;
    update_play_position();
}

static void help() {
    printf("dosamp [options] <file>\n");
    printf(" /h /help             This help\n");
}

static int parse_argv(int argc,char **argv) {
    int i;

    for (i=1;i < argc;) {
        char *a = argv[i++];

        if (*a == '-' || *a == '/') {
            unsigned char m = *a++;
            while (*a == m) a++;

            if (!strcmp(a,"h") || !strcmp(a,"help")) {
                help();
                return 0;
            }
            else if (!strcmp(a,"ac")) {
                a = argv[i++];
                if (a == NULL) return 1;
                prefer_channels = atoi(a);
            }
            else if (!strcmp(a,"ab")) {
                a = argv[i++];
                if (a == NULL) return 1;
                prefer_bits = atoi(a);
            }
            else if (!strcmp(a,"ar")) {
                a = argv[i++];
                if (a == NULL) return 1;
                prefer_rate = atol(a);
            }
            else if (!strcmp(a,"nc")) {
                prefer_no_clamp = 1;
            }
            else {
                return 0;
            }
        }
        else {
            if (wav_file != NULL) return 0;
            wav_file = strdup(a);
        }
    }

    if (wav_file == NULL) {
        printf("You must specify a file to play\n");
        return 0;
    }

    return 1;
}

void display_idle_timesource(void) {
    printf("\x0D");

    if (time_source == &dosamp_time_source_8254)
        printf("8254-PIT ");
    else if (time_source == &dosamp_time_source_rdtsc)
        printf("RDTSC ");
    else
        printf("? ");

    time_source->poll(time_source);

    printf("counter=%-10llu (%lu) rate=%lu req=%lu  ",
        (unsigned long long)time_source->counter,
        (unsigned long)(time_source->counter / (unsigned long long)time_source->clock_rate),
        (unsigned long)time_source->clock_rate,
        (unsigned long)time_source->poll_requirement);

    fflush(stdout);
}

static unsigned long display_time_wait_next = 0;

void display_idle_time(void) {
    unsigned long w,f;
    unsigned char hour,min,sec,centisec;
    unsigned short percent;

    /* display time, but not too often. keep the rate limited. */
    time_source->poll(time_source);

    /* update only if 1/10th of a second has passed */
    if (time_source->counter >= display_time_wait_next) {
        display_time_wait_next += time_source->clock_rate / 20UL; /* 20fps rate (approx, I'm not picky) */

        if (display_time_wait_next < time_source->counter)
            display_time_wait_next = time_source->counter;

        /* to stay within numerical limits of unsigned long, divide into whole and fraction by sample rate */
        w = wav_play_position / (unsigned long)file_codec.sample_rate;
        f = wav_play_position % (unsigned long)file_codec.sample_rate;

        /* we can then turn the whole part into HH:MM:SS */
        sec = (unsigned char)(w % 60UL); w /= 60UL;
        min = (unsigned char)(w % 60UL); w /= 60UL;
        hour = (unsigned char)w; /* don't bother modulus, WAV files couldn't possibly represent things that long */

        /* and then use the more limited numeric range to turn the fractional part into centiseconds */
        centisec = (unsigned char)((f * 100UL) / (unsigned long)file_codec.sample_rate);

        /* also provide the user some information on the length of the WAV */
        {
            unsigned long m,d;

            m = wav_play_position;
            d = wav_data_length;

            /* it's a percentage from 0 to 99, we don't need high precision for large files */
            while ((m|d) >= 0x10000UL) {
                m >>= 4UL;
                d >>= 4UL;
            }

            /* we don't want divide by zero */
            if (d == 0UL) d = 1UL;

            percent = (unsigned short)((m * 1000UL) / d);
        }

        printf("\x0D");
        printf("%02u:%02u:%02u.%02u %%%02u.%u %lu/%lu as %lu-Hz %u-ch %u-bit ",
            hour,min,sec,centisec,percent/10U,percent%10U,wav_play_position,wav_data_length,
            (unsigned long)play_codec.sample_rate,
            (unsigned int)play_codec.number_of_channels,
            (unsigned int)play_codec.bits_per_sample);
        fflush(stdout);
    }
}

void display_idle_buffer(void) {
    signed long pos = -1;
    signed long apos = -1;
    signed long buffersz = -1;
    signed long irq_counter = -1;

    {
        unsigned int sz = sizeof(uint32_t);
        uint32_t bufsz = 0;

        if (soundcard->ioctl(soundcard,soundcard_ioctl_read_irq_counter,&bufsz,&sz,0) >= 0)
            irq_counter = (signed long)bufsz;
        if (soundcard->ioctl(soundcard,soundcard_ioctl_get_buffer_size,&bufsz,&sz,0) >= 0)
            buffersz = (signed long)bufsz;
        if (soundcard->ioctl(soundcard,soundcard_ioctl_get_buffer_write_position,&bufsz,&sz,0) >= 0)
            apos = (signed long)bufsz;
        if (soundcard->ioctl(soundcard,soundcard_ioctl_get_buffer_play_position,&bufsz,&sz,0) >= 0)
            pos = (signed long)bufsz;
    }

    printf("\x0D");

    printf("a=%6ld/p=%6ld/b=%6ld/d=%6lu/cw=%6lu/wp=%8lu/pp=%8lu/irq=%ld",
        apos,
        pos,
        (signed long)buffersz,
        (unsigned long)soundcard->wav_state.play_delay_bytes,
        (unsigned long)soundcard->can_write(soundcard),
        (unsigned long)soundcard->wav_state.write_counter,
        (unsigned long)soundcard->wav_state.play_counter,
        (unsigned long)irq_counter);

    fflush(stdout);
}

void display_idle(unsigned char disp) {
    switch (disp) {
        case 1:     display_idle_time(); break;
        case 2:     display_idle_buffer(); break;
        case 3:     display_idle_timesource(); break;
    }
}

void free_sound_blaster_support(void) {
    unsigned int i;

    free_dma_buffer();

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

    /* we want to know if certain emulation TSRs exist */
    gravis_mega_em_detect(&megaem_info);
    gravis_sbos_detect();

    /* it's up to us now to tell it certain minor things */
    sndsb_detect_virtualbox();      // whether or not we're running in VirtualBox
    /* sndsb now allows us to keep the EXE small by not referring to extra sound card support */
    sndsb_enable_sb16_support();        // SB16 support
    sndsb_enable_sc400_support();       // SC400 support
    sndsb_enable_ess_audiodrive_support();  // ESS AudioDrive support

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

    /* Non-plug & play scan: BLASTER environment variable */
    if (sndsb_try_blaster_var() != NULL) {
        if (!sndsb_init_card(sndsb_card_blaster))
            sndsb_free_card(sndsb_card_blaster);
    }

    /* Non-plug & play scan: Most SB cards exist at 220h or 240h */
    sndsb_try_base(0x220);
    sndsb_try_base(0x240);

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

int main(int argc,char **argv) {
    unsigned char disp=1;
    int i,loop;

    if (!parse_argv(argc,argv))
        return 1;

    /* default good resampler */
    /* TODO: If we detect the CPU is slow enough, default to "fast" */
    resample_state.resample_mode = resample_good;

    cpu_probe();
    probe_8237();
    if (!probe_8259()) {
        printf("Cannot init 8259 PIC\n");
        return 1;
    }
    if (!probe_8254()) {
        printf("Cannot init 8254 timer\n");
        return 1;
    }
    if (!init_isa_pnp_bios()) {
        printf("Cannot init ISA PnP\n");
        return 1;
    }
    find_isa_pnp_bios();

    /* pick initial time source. */
    time_source = &dosamp_time_source_8254;

    /* NTS: When using the PIT source we NEED to do this, or else DOSBox-X (and possibly
     *      many other motherboards) will leave the PIT in a mode that produces the correct
     *      IRQ 0 rate but counts down twice as fast. We need the PIT to count down by 1,
     *      not by 2, in order to keep time. MS-DOS only. */
    write_8254_system_timer(0); /* 18.2 tick/sec on our terms (proper PIT mode) */

    /* we can use the Time Stamp Counter on Pentium or higher systems that offer it */
    if (dosamp_time_source_rdtsc_available(time_source))
        time_source = &dosamp_time_source_rdtsc;

    /* open the time source */
    if (time_source->open(time_source) < 0) {
        printf("Cannot open time source\n");
        return 1;
    }

    if (soundcardlist_init() < 0)
        return 1;

    /* PROBE: Sound Blaster.
     * Will return 0 if scan done, -1 if a serious problem happened.
     * A return value of 0 doesn't mean any cards were found. */
    if (probe_for_sound_blaster() < 0) {
        printf("Serious error while probing for Sound Blaster\n");
        return 1;
    }

    /* now let the user choose. */
    {
        unsigned char count = 0;
        int sc_idx = -1;
        soundcard_t sc;

        for (i=0;i < soundcardlist_count;i++) {
            sc = &soundcardlist[i];
            if (sc->driver == soundcard_none) continue;
            count++;
        }

        if (count == 0) {
            printf("No cards found.\n");
            return 1;
        }
        else if (count > 1) {
            printf("-----------\n");
            printf("Which card?: "); fflush(stdout);

            i = getch();
            printf("\n");
            if (i == 27) return 0;
            if (i == 13 || i == 10) i = '1';
            sc_idx = i - '1';

            if (sc_idx < 0 || sc_idx >= soundcardlist_count) {
                printf("Sound card index out of range\n");
                return 1;
            }
        }
        else { /* count == 1 */
            sc_idx = 0;
        }

        soundcard = &soundcardlist[sc_idx];
    }

    loop = 1;
    if (soundcard->open(soundcard) < 0)
        printf("Failed to open soundcard\n");
    else if (open_wav() < 0)
        printf("Failed to open file\n");
    else if (begin_play() < 0)
        printf("Failed to start playback\n");
    else {
        printf("Hit ESC to stop playback. Use 1-9 keys for other status.\n");

        while (loop) {
            wav_idle();
            display_idle(disp);

            if (kbhit()) {
                i = getch();
                if (i == 0) i = getch() << 8;

                if (i == 27) {
                    loop = 0;
                    break;
                }
                else if (i == 'S') {
                    stuck_test = !stuck_test;
                    printf("Stuck test %s\n",stuck_test?"on":"off");
                }
                else if (i == 'A') {
                    unsigned char wp = soundcard->wav_state.playing;
                    int r;

                    if (wp) stop_play();

                    r = soundcard->ioctl(soundcard,soundcard_ioctl_get_autoinit,NULL,NULL,0);
                    if (r >= 0) {
                        if (soundcard->ioctl(soundcard,soundcard_ioctl_set_autoinit,NULL,NULL,!r) >= 0)
                            printf("%sabled auto-init\n",!r ? "En" : "Dis");
                        else
                            printf("Unable to change auto-init\n");
                    }
                    else {
                        printf("Auto-init not supported\n");
                    }

                    if (wp) begin_play();
                }
                else if (i == 'M') {
                    use_mmap_write = !use_mmap_write;
                    printf("%s mmap write\n",use_mmap_write?"Using":"Not using");
                }
                else if (i >= '0' && i <= '9') {
                    unsigned char nd = (unsigned char)(i-'0');

                    if (disp != nd) {
                        printf("\n");
                        disp = nd;
                    }
                }
                else if (i == '^') { /* shift-6 */
                    unsigned char wp = soundcard->wav_state.playing;

                    if (wp) stop_play();

                    if (prefer_rate < 4000)
                        prefer_rate = 4000;
                    else if (prefer_rate < 6000)
                        prefer_rate = 6000;
                    else if (prefer_rate < 8000)
                        prefer_rate = 8000;
                    else if (prefer_rate < 11025)
                        prefer_rate = 11025;
                    else if (prefer_rate < 22050)
                        prefer_rate = 22050;
                    else if (prefer_rate < 44100)
                        prefer_rate = 44100;
                    else
                        prefer_rate = 0;

                    if (wp) begin_play();
                }
                else if (i == '&') { /* shift-7 */
                    unsigned char wp = soundcard->wav_state.playing;

                    if (wp) stop_play();

                    if (prefer_channels < 1)
                        prefer_channels = 1;
                    else if (prefer_channels < 2)
                        prefer_channels = 2;
                    else
                        prefer_channels = 0;

                    if (wp) begin_play();
                }
                else if (i == '*') { /* shift-8 */
                    unsigned char wp = soundcard->wav_state.playing;

                    if (wp) stop_play();

                    if (prefer_bits < 8)
                        prefer_bits = 8;
                    else if (prefer_bits < 16)
                        prefer_bits = 16;
                    else
                        prefer_bits = 0;

                    if (wp) begin_play();
                }
                else if (i == 'R') {
                    unsigned char wp = soundcard->wav_state.playing;

                    if (wp) stop_play();

                    if ((++resample_state.resample_mode) >= resample_MAX)
                        resample_state.resample_mode = resample_fast;

                    if (wp) begin_play();
                }
                else if (i == '[') {
                    unsigned char wp = soundcard->wav_state.playing;

                    if (wp) stop_play();
                    move_pos(-((signed long)file_codec.sample_rate)); /* -1 sec */
                    if (wp) begin_play();
                }
                else if (i == ']') {
                    unsigned char wp = soundcard->wav_state.playing;

                    if (wp) stop_play();
                    move_pos(file_codec.sample_rate); /* 1 sec */
                    if (wp) begin_play();
                }
                else if (i == '{') {
                    unsigned char wp = soundcard->wav_state.playing;

                    if (wp) stop_play();
                    move_pos(-((signed long)file_codec.sample_rate * 30L)); /* -30 sec */
                    if (wp) begin_play();
                }
                else if (i == '}') {
                    unsigned char wp = soundcard->wav_state.playing;

                    if (wp) stop_play();
                    move_pos(file_codec.sample_rate * 30L); /* 30 sec */
                    if (wp) begin_play();
                }
                else if (i == ' ') {
                    if (soundcard->wav_state.playing) stop_play();
                    else begin_play();
                }
            }
        }
    }

    convert_rdbuf_check();

    _sti();
    stop_play();
    close_wav();
    tmpbuffer_free();
    convert_rdbuf_free();
    soundcard->close(soundcard);

    free_sound_blaster_support();

    soundcardlist_close();

    time_source->close(time_source);
    return 0;
}

