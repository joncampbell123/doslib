
#include <windows.h>
#include <mmsystem.h>

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

#include "sc_winmm.h"

#if defined(TARGET_WINDOWS)

int init_mmsystem(void);

#define WAVE_INVALID_HANDLE         ((HWAVEOUT) NULL)

static unsigned char                mmsystem_probed = 0;

UINT (WINAPI *__waveOutClose)(HWAVEOUT) = NULL;
UINT (WINAPI *__waveOutReset)(HWAVEOUT) = NULL;
UINT (WINAPI *__waveOutOpen)(LPHWAVEOUT,UINT,LPWAVEFORMAT,DWORD,DWORD,DWORD) = NULL;
UINT (WINAPI *__waveOutGetDevCaps)(UINT,LPWAVEOUTCAPS,UINT) = NULL;
UINT (WINAPI *__waveOutGetNumDevs)(void) = NULL;

/* this depends on keeping the "play delay" up to date */
static uint32_t dosamp_FAR mmsystem_can_write(soundcard_t sc) { /* in bytes */
    return 0;
}

static int dosamp_FAR mmsystem_clamp_if_behind(soundcard_t sc,uint32_t ahead_in_bytes) {
    (void)sc;
    (void)ahead_in_bytes;

    return 0;
}

static unsigned char dosamp_FAR * dosamp_FAR mmsystem_mmap_write(soundcard_t sc,uint32_t dosamp_FAR * const howmuch,uint32_t want) {
    (void)sc;
    (void)howmuch;
    (void)want;

    return NULL;
}

static int dosamp_FAR mmsystem_poll(soundcard_t sc);

/* non-mmap write (much like OSS or ALSA in Linux where you do not have direct access to the hardware buffer) */
static unsigned int dosamp_FAR mmsystem_buffer_write(soundcard_t sc,const unsigned char dosamp_FAR * buf,unsigned int len) {
    (void)sc;
    (void)buf;
    (void)len;

    return 0;
}

static int dosamp_FAR mmsystem_open(soundcard_t sc) {
    if (sc->wav_state.is_open) return -1; /* already open! */

    sc->wav_state.is_open = 1;
    return 0;
}

static int dosamp_FAR mmsystem_close(soundcard_t sc) {
    if (!sc->wav_state.is_open) return 0;

    sc->wav_state.is_open = 0;
    return 0;
}

static int dosamp_FAR mmsystem_poll(soundcard_t sc) {
    (void)sc;

    return 0;
}

static int dosamp_FAR mmsystem_irq_callback(soundcard_t sc) {
    (void)sc;

    return 0;
}

static int mmsystem_prepare_play(soundcard_t sc) {
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

    /* open the sound device */
    {
        PCMWAVEFORMAT pcm;
        UINT r;

        memset(&pcm,0,sizeof(pcm));
        pcm.wf.wFormatTag = WAVE_FORMAT_PCM;
        pcm.wf.nChannels = sc->cur_codec.number_of_channels;
        pcm.wf.nSamplesPerSec = sc->cur_codec.sample_rate;
        pcm.wf.nAvgBytesPerSec = sc->cur_codec.sample_rate * sc->cur_codec.bytes_per_block;
        pcm.wf.nBlockAlign = sc->cur_codec.bytes_per_block;
        pcm.wBitsPerSample = sc->cur_codec.bits_per_sample;

        r = __waveOutOpen(&sc->p.mmsystem.handle, sc->p.mmsystem.device_id, (LPWAVEFORMAT)(&pcm), NULL, NULL, 0);
        if (r != 0) return -1;
    }

    sc->wav_state.prepared = 1;
    return 0;
}

static int mmsystem_unprepare_play(soundcard_t sc) {
    if (sc->wav_state.playing) return -1;

    if (sc->wav_state.prepared) {
        /* close the sound device */
        if (sc->p.mmsystem.handle != WAVE_INVALID_HANDLE) {
            __waveOutReset(sc->p.mmsystem.handle);
            __waveOutClose(sc->p.mmsystem.handle);
            sc->p.mmsystem.handle = WAVE_INVALID_HANDLE;
        }

        sc->wav_state.prepared = 0;
    }

    return 0;
}

static uint32_t mmsystem_play_buffer_play_pos(soundcard_t sc) {
    return sc->wav_state.play_counter;
}

static uint32_t mmsystem_play_buffer_write_pos(soundcard_t sc) {
    return sc->wav_state.write_counter;
}

static uint32_t mmsystem_play_buffer_size(soundcard_t sc) {
    return 0;
}

static int mmsystem_start_playback(soundcard_t sc) {
    if (!sc->wav_state.prepared) return -1;
    if (sc->wav_state.playing) return 0;

    sc->wav_state.play_counter = 0;
    sc->wav_state.write_counter = 0;
    sc->wav_state.play_counter_prev = 0;

    sc->wav_state.playing = 1;
    return 0;
}

static int mmsystem_stop_playback(soundcard_t sc) {
    if (!sc->wav_state.playing) return 0;

    sc->wav_state.playing = 0;
    return 0;
}

static int mmsystem_set_play_format(soundcard_t sc,struct wav_cbr_t dosamp_FAR * const fmt) {
    PCMWAVEFORMAT pcm;
    WAVEOUTCAPS caps;
    UINT r;

    /* must be open */
    if (!sc->wav_state.is_open) return -1;

    /* not while prepared or playing!
     * assume: playing is not set unless prepared */
    if (sc->wav_state.prepared) return -1;

    /* sane limits */
    if (fmt->bits_per_sample < 8) fmt->bits_per_sample = 8;
    else if (fmt->bits_per_sample > 8) fmt->bits_per_sample = 16;

    if (fmt->number_of_channels < 1) fmt->number_of_channels = 1;
    else if (fmt->number_of_channels > 2) fmt->number_of_channels = 2;

    /* ask the driver */
    memset(&caps,0,sizeof(caps));
    if (__waveOutGetDevCaps(sc->p.mmsystem.device_id, &caps, sizeof(caps)) != 0)
        return -1;

    if (fmt->bits_per_sample == 8) {
        if ((caps.dwFormats & (WAVE_FORMAT_1M08|WAVE_FORMAT_1S08|WAVE_FORMAT_2M08|WAVE_FORMAT_2S08|WAVE_FORMAT_4M08|WAVE_FORMAT_4S08)) == 0)
            fmt->bits_per_sample = 16;
    }
    if (fmt->bits_per_sample > 8) {
        if ((caps.dwFormats & (WAVE_FORMAT_1M16|WAVE_FORMAT_1S16|WAVE_FORMAT_2M16|WAVE_FORMAT_2S16|WAVE_FORMAT_4M16|WAVE_FORMAT_4S16)) == 0)
            fmt->bits_per_sample = 8;
    }
    if (fmt->sample_rate > 22050) {
        if ((caps.dwFormats & (WAVE_FORMAT_4M08|WAVE_FORMAT_4S08|WAVE_FORMAT_4M16|WAVE_FORMAT_4S16)) == 0)
            fmt->sample_rate = 22050;
    }
    if (fmt->sample_rate > 11025) {
        if ((caps.dwFormats & (WAVE_FORMAT_2M08|WAVE_FORMAT_2S08|WAVE_FORMAT_2M16|WAVE_FORMAT_2S16)) == 0)
            fmt->sample_rate = 11025;
    }
    if (fmt->sample_rate > 8000) {
        if ((caps.dwFormats & (WAVE_FORMAT_1M08|WAVE_FORMAT_1S08|WAVE_FORMAT_1M16|WAVE_FORMAT_1S16)) == 0)
            fmt->sample_rate = 8000;
    }

    /* PCM recalc */
    fmt->bytes_per_block = ((fmt->bits_per_sample+7U)/8U) * fmt->number_of_channels;

    memset(&pcm,0,sizeof(pcm));
    pcm.wf.wFormatTag = WAVE_FORMAT_PCM;
    pcm.wf.nChannels = fmt->number_of_channels;
    pcm.wf.nSamplesPerSec = fmt->sample_rate;
    pcm.wf.nAvgBytesPerSec = fmt->sample_rate * fmt->bytes_per_block;
    pcm.wf.nBlockAlign = fmt->bytes_per_block;
    pcm.wBitsPerSample = fmt->bits_per_sample;

    /* is OK? */
    r = __waveOutOpen(NULL, sc->p.mmsystem.device_id, (LPWAVEFORMAT)(&pcm), NULL, NULL, WAVE_FORMAT_QUERY);
    if (r == WAVERR_BADFORMAT && pcm.wBitsPerSample == 16) {
        /* drop to 8-bit. is OK? */
        fmt->bits_per_sample = 8;
        fmt->bytes_per_block = ((fmt->bits_per_sample+7U)/8U) * fmt->number_of_channels;

        pcm.wf.nAvgBytesPerSec = fmt->sample_rate * fmt->bytes_per_block;
        pcm.wf.nBlockAlign = fmt->bytes_per_block;
        pcm.wBitsPerSample = fmt->bits_per_sample;

        r = __waveOutOpen(NULL, sc->p.mmsystem.device_id, (LPWAVEFORMAT)(&pcm), NULL, NULL, WAVE_FORMAT_QUERY);
    }
    if (r == WAVERR_BADFORMAT && pcm.wf.nChannels > 1) {
        /* drop to mono. is OK? */
        fmt->number_of_channels = 1;
        fmt->bytes_per_block = ((fmt->bits_per_sample+7U)/8U) * fmt->number_of_channels;

        pcm.wf.nAvgBytesPerSec = fmt->sample_rate * fmt->bytes_per_block;
        pcm.wf.nBlockAlign = fmt->bytes_per_block;
        pcm.wBitsPerSample = fmt->bits_per_sample;

        r = __waveOutOpen(NULL, sc->p.mmsystem.device_id, (LPWAVEFORMAT)(&pcm), NULL, NULL, WAVE_FORMAT_QUERY);
    }

    if (r == WAVERR_BADFORMAT)
        return -1;

    /* PCM recalc */
    fmt->bytes_per_block = ((fmt->bits_per_sample+7U)/8U) * fmt->number_of_channels;

    /* take it */
    sc->cur_codec = *fmt;

    return 0;
}

static int mmsystem_get_card_name(soundcard_t sc,void dosamp_FAR *data,unsigned int dosamp_FAR *len) {
    const char *str;

    (void)sc;

    if (data == NULL || len == NULL) return -1;
    if (*len == 0U) return -1;

    str = "Open Sound System";

    soundcard_str_return_common((char dosamp_FAR*)data,len,str);
    return 0;
}

static int mmsystem_get_card_detail(soundcard_t sc,void dosamp_FAR *data,unsigned int dosamp_FAR *len) {
    char *w;

    if (data == NULL || len == NULL) return -1;
    if (*len == 0U) return -1;

    w = soundcard_str_tmp;
    w += sprintf(w,"MMSYSTEM device");

    assert(w < (soundcard_str_tmp+sizeof(soundcard_str_tmp)));

    soundcard_str_return_common((char dosamp_FAR*)data,len,soundcard_str_tmp);
    return 0;
}

static int dosamp_FAR mmsystem_ioctl(soundcard_t sc,unsigned int cmd,void dosamp_FAR *data,unsigned int dosamp_FAR * len,int ival) {
    (void)ival;

    switch (cmd) {
        case soundcard_ioctl_get_card_name:
            return mmsystem_get_card_name(sc,data,len);
        case soundcard_ioctl_get_card_detail:
            return mmsystem_get_card_detail(sc,data,len);
        case soundcard_ioctl_set_play_format:
            if (data == NULL || len == 0) return -1;
            if (*len < sizeof(struct wav_cbr_t)) return -1;
            return mmsystem_set_play_format(sc,(struct wav_cbr_t dosamp_FAR *)data);
        case soundcard_ioctl_prepare_play:
            return mmsystem_prepare_play(sc);
        case soundcard_ioctl_unprepare_play:
            return mmsystem_unprepare_play(sc);
        case soundcard_ioctl_start_play:
            return mmsystem_start_playback(sc);
        case soundcard_ioctl_stop_play:
            return mmsystem_stop_playback(sc);
        case soundcard_ioctl_get_buffer_write_position: {
            if (data == NULL || len == 0) return -1;
            if (*len < sizeof(uint32_t)) return -1;
            if ((*((uint32_t dosamp_FAR*)data) = mmsystem_play_buffer_write_pos(sc)) == 0) return -1;
            } return 0;
        case soundcard_ioctl_get_buffer_play_position: {
            if (data == NULL || len == 0) return -1;
            if (*len < sizeof(uint32_t)) return -1;
            if ((*((uint32_t dosamp_FAR*)data) = mmsystem_play_buffer_play_pos(sc)) == 0) return -1;
            } return 0;
        case soundcard_ioctl_get_buffer_size: {
            if (data == NULL || len == 0) return -1;
            if (*len < sizeof(uint32_t)) return -1;
            if ((*((uint32_t dosamp_FAR*)data) = mmsystem_play_buffer_size(sc)) == 0) return -1;
            } return 0;
    }

    return -1;
}

struct soundcard mmsystem_soundcard_template = {
    .driver =                                   soundcard_mmsystem,
    .capabilities =                             0,
    .requirements =                             0,
    .can_write =                                mmsystem_can_write,
    .open =                                     mmsystem_open,
    .close =                                    mmsystem_close,
    .poll =                                     mmsystem_poll,
    .clamp_if_behind =                          mmsystem_clamp_if_behind,
    .irq_callback =                             mmsystem_irq_callback,
    .write =                                    mmsystem_buffer_write,
    .mmap_write =                               mmsystem_mmap_write,
    .ioctl =                                    mmsystem_ioctl,
    .p.mmsystem.device_id =                     WAVE_MAPPER,
    .p.mmsystem.handle =                        WAVE_INVALID_HANDLE
};

int probe_for_mmsystem(void) {
    unsigned int didx;
    struct stat st;
    char path[128];
    UINT devs;

    if (mmsystem_probed) return 0;

    if (!init_mmsystem()) return 0;

    if ((__waveOutGetDevCaps=((UINT (WINAPI *)(UINT,LPWAVEOUTCAPS,UINT))GetProcAddress(mmsystem_dll,"WAVEOUTGETDEVCAPS"))) == NULL)
        return 0;
    if ((__waveOutOpen=((UINT (WINAPI *)(LPHWAVEOUT,UINT,LPWAVEFORMAT,DWORD,DWORD,DWORD))GetProcAddress(mmsystem_dll,"WAVEOUTOPEN"))) == NULL)
        return 0;
    if ((__waveOutClose=((UINT (WINAPI *)(HWAVEOUT))GetProcAddress(mmsystem_dll,"WAVEOUTCLOSE"))) == NULL)
        return 0;
    if ((__waveOutReset=((UINT (WINAPI *)(HWAVEOUT))GetProcAddress(mmsystem_dll,"WAVEOUTRESET"))) == NULL)
        return 0;
    if ((__waveOutGetNumDevs=((UINT (WINAPI *)(void))GetProcAddress(mmsystem_dll,"WAVEOUTGETNUMDEVS"))) == NULL)
        return 0;

    devs = __waveOutGetNumDevs();
    if (devs != 0U) {
        /* just point at the wave mapper and call it good */
        soundcard_t sc;

        sc = soundcardlist_new(&mmsystem_soundcard_template);
        if (sc != NULL) {
            sc->p.mmsystem.device_id = WAVE_MAPPER;
        }
    }

    mmsystem_probed = 1;
    return 0;
}

void free_mmsystem_support(void) {
    mmsystem_probed = 0;
}

#endif /* defined(HAS_OSS) */

