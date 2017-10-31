
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

/* this depends on keeping the "play delay" up to date */
static uint32_t dosamp_FAR alsa_can_write(soundcard_t sc) { /* in bytes */
    (void)sc;

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

static int dosamp_FAR alsa_poll(soundcard_t sc);

/* non-mmap write (much like OSS or ALSA in Linux where you do not have direct access to the hardware buffer) */
static unsigned int dosamp_FAR alsa_buffer_write(soundcard_t sc,const unsigned char dosamp_FAR * buf,unsigned int len) {
    (void)sc;
    (void)buf;
    (void)len;

    return 0;
}

static int dosamp_FAR alsa_open(soundcard_t sc) {
    if (sc->wav_state.is_open) return -1; /* already open! */

    sc->wav_state.is_open = 1;
    return 0;
}

static int dosamp_FAR alsa_close(soundcard_t sc) {
    if (!sc->wav_state.is_open) return 0;

    sc->wav_state.is_open = 0;
    return 0;
}

static int dosamp_FAR alsa_poll(soundcard_t sc) {
    (void)sc;

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

    sc->wav_state.playing = 1;
    return 0;
}

static int alsa_stop_playback(soundcard_t sc) {
    if (!sc->wav_state.playing) return 0;

    sc->wav_state.playing = 0;
    return 0;
}

static int alsa_set_play_format(soundcard_t sc,struct wav_cbr_t dosamp_FAR * const fmt) {
    /* must be open */
    if (!sc->wav_state.is_open) return -1;

    /* not while prepared or playing!
     * assume: playing is not set unless prepared */
    if (sc->wav_state.prepared) return -1;

    /* PCM recalc */
    fmt->bytes_per_block = ((fmt->bits_per_sample+7U)/8U) * fmt->number_of_channels;

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

int probe_for_alsa(void) {
    if (alsa_probed) return 0;

    alsa_probed = 1;
    return 0;
}

void free_alsa_support(void) {
    alsa_probed = 0;
}

#endif /* defined(HAS_OSS) */

