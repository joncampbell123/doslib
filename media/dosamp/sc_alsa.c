
#include <stdio.h>
#include <stdint.h>
#ifdef LINUX
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
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

#include "sc_alsa.h"

#if defined(HAS_OSS)

static unsigned char                alsa_probed = 0;

static int dosamp_FAR alsa_poll(soundcard_t sc);

/* this depends on keeping the "play delay" up to date */
static uint32_t dosamp_FAR alsa_can_write(soundcard_t sc) { /* in bytes */
    snd_pcm_sframes_t avail=0,delay=0;

    if (sc->p.alsa.handle == NULL) return 0;

    snd_pcm_avail_delay(sc->p.alsa.handle, &avail, &delay);
    return avail * sc->cur_codec.bytes_per_block;
}

/* detect playback underruns, and force write pointer forward to compensate if so.
 * caller will direct us how many bytes into the future to set the write pointer,
 * since nobody can INSTANTLY write audio to the playback pointer.
 *
 * This way, the playback program can use this to force the write pointer forward
 * if underrun to at least ensure the next audio written will be immediately audible.
 * Without this, upon underrun, the written audio may not be heard until the play
 * pointer has gone through the entire buffer again. */
static int dosamp_FAR alsa_clamp_if_behind(soundcard_t sc,uint32_t ahead_in_bytes) {
    (void)sc;
    (void)ahead_in_bytes;

    return 0;
}

static unsigned char dosamp_FAR * dosamp_FAR alsa_mmap_write(soundcard_t sc,uint32_t dosamp_FAR * const howmuch,uint32_t want) {
    (void)sc;
    (void)howmuch;
    (void)want;

    return NULL;
}

/* non-mmap write (much like OSS or ALSA in Linux where you do not have direct access to the hardware buffer) */
static unsigned int dosamp_FAR alsa_buffer_write(soundcard_t sc,const unsigned char dosamp_FAR * buf,unsigned int len) {
    int r;

    if (sc->p.alsa.handle == NULL) return 0;

    /* ALSA can only represent in "frames" not bytes */
    if (len < sc->cur_codec.bytes_per_block) return 0;

    r = snd_pcm_writei(sc->p.alsa.handle, buf, len / sc->cur_codec.bytes_per_block);
    if (r == -EPIPE) {
        /* underrun */
        snd_pcm_prepare(sc->p.alsa.handle);
    }
    else if (r < 0) {
        r = 0;
    }

    sc->wav_state.write_counter += r * sc->cur_codec.bytes_per_block;
    alsa_poll(sc);

    return r * sc->cur_codec.bytes_per_block;
}

static int dosamp_FAR alsa_open(soundcard_t sc) {
    if (sc->wav_state.is_open) return -1; /* already open! */
    if (sc->p.alsa.device == NULL) return -1;

    assert(sc->p.alsa.handle == NULL);
    assert(sc->p.alsa.param == NULL);

    if (snd_pcm_open(&sc->p.alsa.handle, sc->p.alsa.device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK | SND_PCM_NO_AUTO_RESAMPLE) < 0)
        goto fail;

    if (snd_pcm_hw_params_malloc(&sc->p.alsa.param))
        goto fail;

    snd_pcm_hw_params_any(sc->p.alsa.handle, sc->p.alsa.param);
    snd_pcm_hw_params_set_access(sc->p.alsa.handle, sc->p.alsa.param, SND_PCM_ACCESS_RW_INTERLEAVED);

    sc->wav_state.is_open = 1;
    return 0;
fail:
    if (sc->p.alsa.param != NULL) {
        snd_pcm_hw_params_free(sc->p.alsa.param);
        sc->p.alsa.param = NULL;
    }
    if (sc->p.alsa.handle != NULL) {
        snd_pcm_close(sc->p.alsa.handle);
        sc->p.alsa.handle = NULL;
    }

    return -1;
}

static int dosamp_FAR alsa_close(soundcard_t sc) {
    if (!sc->wav_state.is_open) return 0;

    if (sc->p.alsa.param != NULL) {
        snd_pcm_hw_params_free(sc->p.alsa.param);
        sc->p.alsa.param = NULL;
    }
    if (sc->p.alsa.handle != NULL) {
        snd_pcm_close(sc->p.alsa.handle);
        sc->p.alsa.handle = NULL;
    }

    sc->wav_state.is_open = 0;
    return 0;
}

static int dosamp_FAR alsa_poll(soundcard_t sc) {
    snd_pcm_sframes_t avail=0,delay=0;

    if (sc->p.alsa.handle == NULL) return 0;

    snd_pcm_avail_delay(sc->p.alsa.handle, &avail, &delay);
    sc->wav_state.play_delay = delay;
    delay *= sc->cur_codec.bytes_per_block;
    sc->wav_state.play_delay_bytes = delay;

    sc->wav_state.play_counter_prev = sc->wav_state.play_counter;

    sc->wav_state.play_counter = sc->wav_state.write_counter;
    if (sc->wav_state.play_counter >= sc->wav_state.play_delay_bytes)
        sc->wav_state.play_counter -= sc->wav_state.play_delay_bytes;
    else
        sc->wav_state.play_counter = 0;

    return 0;
}

static int dosamp_FAR alsa_irq_callback(soundcard_t sc) {
    (void)sc;

    return 0;
}

static int alsa_prepare_play(soundcard_t sc) {
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

static int alsa_unprepare_play(soundcard_t sc) {
    if (sc->wav_state.playing) return -1;

    if (sc->wav_state.prepared) {
        sc->wav_state.prepared = 0;
    }

    return 0;
}

static uint32_t alsa_play_buffer_play_pos(soundcard_t sc) {
    return sc->wav_state.play_counter;
}

static uint32_t alsa_play_buffer_write_pos(soundcard_t sc) {
    return sc->wav_state.write_counter;
}

static uint32_t alsa_play_buffer_size(soundcard_t sc) {
    return sc->p.alsa.buffer_size;
}

static int alsa_start_playback(soundcard_t sc) {
    if (!sc->wav_state.prepared) return -1;
    if (sc->wav_state.playing) return 0;

    sc->wav_state.play_counter = 0;
    sc->wav_state.write_counter = 0;
    sc->wav_state.play_counter_prev = 0;

    if (sc->p.alsa.handle != NULL)
        snd_pcm_drop(sc->p.alsa.handle);

    snd_pcm_prepare(sc->p.alsa.handle);
    sc->wav_state.playing = 1;
    return 0;
}

static int alsa_stop_playback(soundcard_t sc) {
    if (!sc->wav_state.playing) return 0;

    if (sc->p.alsa.handle != NULL)
        snd_pcm_drop(sc->p.alsa.handle);

    sc->wav_state.playing = 0;
    return 0;
}

static int alsa_set_play_format(soundcard_t sc,struct wav_cbr_t dosamp_FAR * const fmt) {
    /* must be open */
    if (!sc->wav_state.is_open) return -1;

    /* not while prepared or playing!
     * assume: playing is not set unless prepared */
    if (sc->wav_state.prepared) return -1;

    /* pass it through to ALSA, see what happens */
    if (fmt->bits_per_sample == 8)
        snd_pcm_hw_params_set_format(sc->p.alsa.handle, sc->p.alsa.param, SND_PCM_FORMAT_U8);
    else if (fmt->bits_per_sample == 16)
        snd_pcm_hw_params_set_format(sc->p.alsa.handle, sc->p.alsa.param, SND_PCM_FORMAT_S16_LE);
    else
        return -1;

    snd_pcm_hw_params_set_channels(sc->p.alsa.handle, sc->p.alsa.param, fmt->number_of_channels);
    {
        unsigned int v = fmt->number_of_channels;
        snd_pcm_hw_params_get_channels(sc->p.alsa.param, &v);
        fmt->number_of_channels = v;
    }

    {
        int dir = 0;
        unsigned int v = fmt->sample_rate;
        snd_pcm_hw_params_set_rate_near(sc->p.alsa.handle, sc->p.alsa.param, &v, &dir);
        fmt->sample_rate = v;
    }

    /* reasonable buffer max */
    {
        snd_pcm_uframes_t uft;

        uft = (snd_pcm_uframes_t)fmt->sample_rate * 2;
        snd_pcm_hw_params_set_buffer_size_max(sc->p.alsa.handle, sc->p.alsa.param, &uft);
    }

    /* apply to hardware */
    if (snd_pcm_hw_params(sc->p.alsa.handle, sc->p.alsa.param) < 0)
        return -1;

    /* so what actually took? */
    {
        int dir = 0;
        unsigned int val = fmt->sample_rate;

        snd_pcm_hw_params_get_rate(sc->p.alsa.param, &val, &dir);
        fmt->sample_rate = val;
    }
    {
        unsigned int v = fmt->number_of_channels;
        snd_pcm_hw_params_get_channels(sc->p.alsa.param, &v);
        fmt->number_of_channels = v;
    }
    {
        snd_pcm_format_t f;

        if (snd_pcm_hw_params_get_format(sc->p.alsa.param, &f) >= 0) {
            if (f == SND_PCM_FORMAT_U8)
                fmt->bits_per_sample = 8;
            else if (f == SND_PCM_FORMAT_S16_LE)
                fmt->bits_per_sample = 16;
            else
                return -1;
        }
    }

    /* PCM recalc */
    fmt->bytes_per_block = ((fmt->bits_per_sample+7U)/8U) * fmt->number_of_channels;

    /* update buffer size */
    {
        snd_pcm_uframes_t uft;

        uft = 0;
        snd_pcm_hw_params_get_buffer_size(sc->p.alsa.param, &uft);
        sc->p.alsa.buffer_size = uft * fmt->bytes_per_block;
    }

    /* take it */
    sc->cur_codec = *fmt;

    return 0;
}

static int alsa_get_card_name(soundcard_t sc,void dosamp_FAR *data,unsigned int dosamp_FAR *len) {
    const char *str;

    (void)sc;

    if (data == NULL || len == NULL) return -1;
    if (*len == 0U) return -1;

    str = "Advanced Linux Sound Architecture";

    soundcard_str_return_common((char dosamp_FAR*)data,len,str);
    return 0;
}

static int alsa_get_card_detail(soundcard_t sc,void dosamp_FAR *data,unsigned int dosamp_FAR *len) {
    char *w;

    if (data == NULL || len == NULL) return -1;
    if (*len == 0U) return -1;

    w = soundcard_str_tmp;
    w += sprintf(w,"PCM device %s",sc->p.alsa.device);

    assert(w < (soundcard_str_tmp+sizeof(soundcard_str_tmp)));

    soundcard_str_return_common((char dosamp_FAR*)data,len,soundcard_str_tmp);
    return 0;
}

static int dosamp_FAR alsa_ioctl(soundcard_t sc,unsigned int cmd,void dosamp_FAR *data,unsigned int dosamp_FAR * len,int ival) {
    (void)ival;

    switch (cmd) {
        case soundcard_ioctl_get_card_name:
            return alsa_get_card_name(sc,data,len);
        case soundcard_ioctl_get_card_detail:
            return alsa_get_card_detail(sc,data,len);
        case soundcard_ioctl_set_play_format:
            if (data == NULL || len == 0) return -1;
            if (*len < sizeof(struct wav_cbr_t)) return -1;
            return alsa_set_play_format(sc,(struct wav_cbr_t dosamp_FAR *)data);
        case soundcard_ioctl_prepare_play:
            return alsa_prepare_play(sc);
        case soundcard_ioctl_unprepare_play:
            return alsa_unprepare_play(sc);
        case soundcard_ioctl_start_play:
            return alsa_start_playback(sc);
        case soundcard_ioctl_stop_play:
            return alsa_stop_playback(sc);
        case soundcard_ioctl_get_buffer_write_position: {
            if (data == NULL || len == 0) return -1;
            if (*len < sizeof(uint32_t)) return -1;
            if ((*((uint32_t dosamp_FAR*)data) = alsa_play_buffer_write_pos(sc)) == 0) return -1;
            } return 0;
        case soundcard_ioctl_get_buffer_play_position: {
            if (data == NULL || len == 0) return -1;
            if (*len < sizeof(uint32_t)) return -1;
            if ((*((uint32_t dosamp_FAR*)data) = alsa_play_buffer_play_pos(sc)) == 0) return -1;
            } return 0;
        case soundcard_ioctl_get_buffer_size: {
            if (data == NULL || len == 0) return -1;
            if (*len < sizeof(uint32_t)) return -1;
            if ((*((uint32_t dosamp_FAR*)data) = alsa_play_buffer_size(sc)) == 0) return -1;
            } return 0;
    }

    return -1;
}

struct soundcard alsa_soundcard_template = {
    .driver =                                   soundcard_alsa,
    .capabilities =                             0,
    .requirements =                             0,
    .can_write =                                alsa_can_write,
    .open =                                     alsa_open,
    .close =                                    alsa_close,
    .poll =                                     alsa_poll,
    .clamp_if_behind =                          alsa_clamp_if_behind,
    .irq_callback =                             alsa_irq_callback,
    .write =                                    alsa_buffer_write,
    .mmap_write =                               alsa_mmap_write,
    .ioctl =                                    alsa_ioctl,
    .p.alsa.handle =                            NULL,
    .p.alsa.device =                            NULL
};

void alsa_check(const char *devname) {
    snd_pcm_t *handle;
    int r;

    r = snd_pcm_open(&handle, devname, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK | SND_PCM_NO_AUTO_RESAMPLE);
    if (r < 0) return;

    {
        soundcard_t sc;

        sc = soundcardlist_new(&alsa_soundcard_template);
        if (sc != NULL) {
            sc->p.alsa.device = strdup(devname);
            printf("ALSA: Found '%s'\n",sc->p.alsa.device);
        }
    }

    if (handle != NULL) snd_pcm_close(handle);
}

int probe_for_alsa(void) {
    if (alsa_probed) return 0;

    /* if ALSA has a "pulseaudio" plugin, use it first.
     * desktops today might like us better if we tie into the same sound system
     * everyone else on the desktop is using. */
    alsa_check("pulse");

    /* then, the "default" device for the user's default ALSA device */
    alsa_check("default");

    alsa_probed = 1;
    return 0;
}

void free_alsa_support(void) {
    alsa_probed = 0;
}

#endif /* defined(HAS_OSS) */

