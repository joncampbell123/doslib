
#include <stdio.h>
#include <stdint.h>
#ifdef LINUX
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <endian.h>
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

#include "sc_oss.h"

#if defined(HAS_OSS)

static unsigned char                oss_probed = 0;

static int oss_get_path(char *dst,size_t dst_max,unsigned int i) {
    if (i == 0)
        snprintf(dst,dst_max,"/dev/dsp");
    else
        snprintf(dst,dst_max,"/dev/dsp%u",i);

    return 0;
}

/* this depends on keeping the "play delay" up to date */
static uint32_t dosamp_FAR oss_can_write(soundcard_t sc) { /* in bytes */
    audio_buf_info ai;

    if (sc->p.oss.fd < 0) return 0;

    memset(&ai,0,sizeof(ai));
    if (ioctl(sc->p.oss.fd,SNDCTL_DSP_GETOSPACE,&ai) >= 0)
        return ai.bytes;

    return 0;
}

/* detect playback underruns, and force write pointer forward to compensate if so.
 * caller will direct us how many bytes into the future to set the write pointer,
 * since nobody can INSTANTLY write audio to the playback pointer.
 *
 * This way, the playback program can use this to force the write pointer forward
 * if underrun to at least ensure the next audio written will be immediately audible.
 * Without this, upon underrun, the written audio may not be heard until the play
 * pointer has gone through the entire buffer again. */
static int dosamp_FAR oss_clamp_if_behind(soundcard_t sc,uint32_t ahead_in_bytes) {
    (void)sc;
    (void)ahead_in_bytes;

    return 0;
}

static unsigned char dosamp_FAR * dosamp_FAR oss_mmap_write(soundcard_t sc,uint32_t dosamp_FAR * const howmuch,uint32_t want) {
    (void)sc;
    (void)howmuch;
    (void)want;

    return NULL;
}

static int dosamp_FAR oss_poll(soundcard_t sc);

/* non-mmap write (much like OSS or ALSA in Linux where you do not have direct access to the hardware buffer) */
static unsigned int dosamp_FAR oss_buffer_write(soundcard_t sc,const unsigned char dosamp_FAR * buf,unsigned int len) {
    int w;

    if (sc->p.oss.fd < 0) return 0;

    w = write(sc->p.oss.fd,buf,len);
    if (w <= 0) return 0;

    sc->wav_state.write_counter += (uint64_t)w;

    oss_poll(sc);

    return (unsigned int)w;
}

static int dosamp_FAR oss_open(soundcard_t sc) {
    char path[128];

    if (sc->wav_state.is_open) return -1; /* already open! */

    assert(sc->p.oss.fd == -1);

    if (oss_get_path(path,sizeof(path),sc->p.oss.index) != 0)
        return -1;

    sc->p.oss.fd = open(path,O_WRONLY | O_NONBLOCK);
    if (sc->p.oss.fd < 0) return -1;

    sc->wav_state.is_open = 1;
    return 0;
}

static int dosamp_FAR oss_close(soundcard_t sc) {
    if (!sc->wav_state.is_open) return 0;

    if (sc->p.oss.fd >= 0) {
        close(sc->p.oss.fd);
        sc->p.oss.fd = -1;
    }

    sc->wav_state.is_open = 0;
    return 0;
}

static int dosamp_FAR oss_poll(soundcard_t sc) {
    count_info ci;
    int delay = 0;

    if (sc->p.oss.fd < 0) return 0;

    /* WARNING: OSS considers the sound card's FIFO as part of the delay */
    memset(&ci,0,sizeof(ci));
    ioctl(sc->p.oss.fd,SNDCTL_DSP_GETOPTR,&ci);

    sc->wav_state.play_counter_prev = sc->wav_state.play_counter;

    sc->wav_state.play_counter += ci.bytes - sc->p.oss.oss_p_pcount;
    sc->p.oss.oss_p_pcount = ci.bytes;

    if (sc->wav_state.play_counter <= sc->wav_state.write_counter)
        sc->wav_state.play_delay_bytes = sc->wav_state.write_counter - sc->wav_state.play_counter;
    else
        sc->wav_state.play_delay_bytes = 0;

    sc->wav_state.play_delay = delay / sc->cur_codec.bytes_per_block;
    return 0;
}

static int dosamp_FAR oss_irq_callback(soundcard_t sc) {
    (void)sc;

    return 0;
}

static int oss_prepare_play(soundcard_t sc) {
    /* format must be set */
    if (sc->cur_codec.sample_rate == 0)
        return -1;

    /* must be open */
    if (!sc->wav_state.is_open)
        return -1;

    if (sc->wav_state.prepared)
        return 0;

    sc->wav_state.play_counter = 0;
    sc->wav_state.write_counter = 0;

    sc->wav_state.prepared = 1;
    return 0;
}

static int oss_unprepare_play(soundcard_t sc) {
    if (sc->wav_state.playing) return -1;

    if (sc->wav_state.prepared) {
        sc->wav_state.prepared = 0;
    }

    return 0;
}

static uint32_t oss_play_buffer_play_pos(soundcard_t sc) {
    return sc->wav_state.play_counter;
}

static uint32_t oss_play_buffer_write_pos(soundcard_t sc) {
    return sc->wav_state.write_counter;
}

static uint32_t oss_play_buffer_size(soundcard_t sc) {
    if (sc->p.oss.fd < 0) return 0;

    /* store buffer size NOW because it will fall to zero later */
    if (sc->p.oss.buffer_size == 0) {
        audio_buf_info ai;

        sc->p.oss.buffer_size = 0;
        memset(&ai,0,sizeof(ai));
        if (ioctl(sc->p.oss.fd,SNDCTL_DSP_GETOSPACE,&ai) >= 0)
            sc->p.oss.buffer_size = ai.fragments * ai.fragsize;
    }

    return sc->p.oss.buffer_size;
}

static int oss_start_playback(soundcard_t sc) {
    if (!sc->wav_state.prepared) return -1;
    if (sc->wav_state.playing) return 0;

    sc->wav_state.play_counter = 0;
    sc->wav_state.write_counter = 0;
    sc->wav_state.play_counter_prev = 0;

    {
        count_info ci;

        /* WARNING: OSS considers the sound card's FIFO as part of the delay */
        memset(&ci,0,sizeof(ci));
        ioctl(sc->p.oss.fd,SNDCTL_DSP_GETOPTR,&ci);

        sc->p.oss.oss_p_pcount = ci.bytes;
    }

    sc->wav_state.playing = 1;
    return 0;
}

static int oss_stop_playback(soundcard_t sc) {
    if (!sc->wav_state.playing) return 0;

    ioctl(sc->p.oss.fd,SNDCTL_DSP_RESET,NULL); /* STOP! */

    sc->wav_state.playing = 0;
    return 0;
}

static int oss_set_play_format(soundcard_t sc,struct wav_cbr_t dosamp_FAR * const fmt) {
    int r;

    /* must be open */
    if (!sc->wav_state.is_open) return -1;

    /* not while prepared or playing!
     * assume: playing is not set unless prepared */
    if (sc->wav_state.prepared) return -1;

    /* channel count */
    r = fmt->number_of_channels;
    if (ioctl(sc->p.oss.fd,SNDCTL_DSP_CHANNELS,&r) < 0) return -1;
    fmt->number_of_channels = r;

    /* bits per sample */
    if (fmt->bits_per_sample == 8)
        r = AFMT_U8;
    else if (fmt->bits_per_sample == 16)
        r = AFMT_S16_LE;
    else
        return -1;

    if (ioctl(sc->p.oss.fd,SNDCTL_DSP_SETFMT,&r) < 0) return -1;

    if (r == AFMT_U8)
        fmt->bits_per_sample = 8;
    else if (r == AFMT_S16_LE)
        fmt->bits_per_sample = 16;
    else
        return -1;

    /* sample rate */
    r = fmt->sample_rate;
    if (ioctl(sc->p.oss.fd,SNDCTL_DSP_SPEED,&r) < 0) return -1;
    fmt->sample_rate = r;

    /* PCM recalc */
    fmt->bytes_per_block = ((fmt->bits_per_sample+7U)/8U) * fmt->number_of_channels;

    /* take it */
    sc->cur_codec = *fmt;

    return 0;
}

static int oss_get_card_name(soundcard_t sc,void dosamp_FAR *data,unsigned int dosamp_FAR *len) {
    const char *str;

    (void)sc;

    if (data == NULL || len == NULL) return -1;
    if (*len == 0U) return -1;

    str = "Open Sound System";

    soundcard_str_return_common((char dosamp_FAR*)data,len,str);
    return 0;
}

static int oss_get_card_detail(soundcard_t sc,void dosamp_FAR *data,unsigned int dosamp_FAR *len) {
    char tx[128];
    char *w;

    if (data == NULL || len == NULL) return -1;
    if (*len == 0U) return -1;

    tx[0] = 0;
    oss_get_path(tx,sizeof(tx),sc->p.oss.index);

    w = soundcard_str_tmp;
    w += sprintf(w,"DSP device %s",tx);

    assert(w < (soundcard_str_tmp+sizeof(soundcard_str_tmp)));

    soundcard_str_return_common((char dosamp_FAR*)data,len,soundcard_str_tmp);
    return 0;
}

static int dosamp_FAR oss_ioctl(soundcard_t sc,unsigned int cmd,void dosamp_FAR *data,unsigned int dosamp_FAR * len,int ival) {
    (void)ival;

    switch (cmd) {
        case soundcard_ioctl_get_card_name:
            return oss_get_card_name(sc,data,len);
        case soundcard_ioctl_get_card_detail:
            return oss_get_card_detail(sc,data,len);
        case soundcard_ioctl_set_play_format:
            if (data == NULL || len == 0) return -1;
            if (*len < sizeof(struct wav_cbr_t)) return -1;
            return oss_set_play_format(sc,(struct wav_cbr_t dosamp_FAR *)data);
        case soundcard_ioctl_prepare_play:
            return oss_prepare_play(sc);
        case soundcard_ioctl_unprepare_play:
            return oss_unprepare_play(sc);
        case soundcard_ioctl_start_play:
            return oss_start_playback(sc);
        case soundcard_ioctl_stop_play:
            return oss_stop_playback(sc);
        case soundcard_ioctl_get_buffer_write_position: {
            if (data == NULL || len == 0) return -1;
            if (*len < sizeof(uint32_t)) return -1;
            if ((*((uint32_t dosamp_FAR*)data) = oss_play_buffer_write_pos(sc)) == 0) return -1;
            } return 0;
        case soundcard_ioctl_get_buffer_play_position: {
            if (data == NULL || len == 0) return -1;
            if (*len < sizeof(uint32_t)) return -1;
            if ((*((uint32_t dosamp_FAR*)data) = oss_play_buffer_play_pos(sc)) == 0) return -1;
            } return 0;
        case soundcard_ioctl_get_buffer_size: {
            if (data == NULL || len == 0) return -1;
            if (*len < sizeof(uint32_t)) return -1;
            if ((*((uint32_t dosamp_FAR*)data) = oss_play_buffer_size(sc)) == 0) return -1;
            } return 0;
    }

    return -1;
}

struct soundcard oss_soundcard_template = {
    .driver =                                   soundcard_oss,
    .capabilities =                             0,
    .requirements =                             0,
    .can_write =                                oss_can_write,
    .open =                                     oss_open,
    .close =                                    oss_close,
    .poll =                                     oss_poll,
    .clamp_if_behind =                          oss_clamp_if_behind,
    .irq_callback =                             oss_irq_callback,
    .write =                                    oss_buffer_write,
    .mmap_write =                               oss_mmap_write,
    .ioctl =                                    oss_ioctl,
    .p.oss.fd =                                 -1
};

int probe_for_oss(void) {
    unsigned int didx;
    struct stat st;
    char path[128];

    if (oss_probed) return 0;

    for (didx=0;didx < 128;didx++) {
        if (oss_get_path(path,sizeof(path),didx) == 0) {
            if (stat(path,&st) == 0 && S_ISCHR(st.st_mode)) {
                soundcard_t sc;

                sc = soundcardlist_new(&oss_soundcard_template);
                if (sc == NULL) continue;

                sc->p.oss.index = didx;

                /* TODO: capabilities */
            }
        }
    }

    return 0;
}

void free_oss_support(void) {
    oss_probed = 0;
}

#endif /* defined(HAS_OSS) */

