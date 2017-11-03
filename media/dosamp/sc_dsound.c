
#include <windows.h>

#include <stdio.h>
#include <stdint.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <direct.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <malloc.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>

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

#include "sc_dsound.h"

#if defined(HAS_DSOUND)
#include <dsound.h>

int init_dsound(void);

static unsigned char                dsound_probed = 0;
static GUID                         zero_guid = {0,0,0,0};

extern HRESULT (WINAPI *__DirectSoundCreate)(LPGUID lpGuid,LPDIRECTSOUND* ppDS,LPUNKNOWN pUnkOuter);

static uint32_t dosamp_FAR dsound_can_write(soundcard_t sc) { /* in bytes */
    (void)sc;

    return 0;
}

static int dosamp_FAR dsound_clamp_if_behind(soundcard_t sc,uint32_t ahead_in_bytes) {
    (void)sc;
    (void)ahead_in_bytes;

    return 0;
}

static unsigned char dosamp_FAR * dosamp_FAR dsound_mmap_write(soundcard_t sc,uint32_t dosamp_FAR * const howmuch,uint32_t want) {
    (void)sc;
    (void)howmuch;
    (void)want;

    return NULL;
}

/* non-mmap write (much like OSS or ALSA in Linux where you do not have direct access to the hardware buffer) */
static unsigned int dosamp_FAR dsound_buffer_write(soundcard_t sc,const unsigned char dosamp_FAR * buf,unsigned int len) {
    (void)sc;
    (void)buf;
    (void)len;

    return 0;
}

static int dosamp_FAR dsound_close(soundcard_t sc);

static int dosamp_FAR dsound_open(soundcard_t sc) {
    HWND hwnd = NULL;
    HRESULT hr;

    if (sc->wav_state.is_open) return -1; /* already open! */

    assert(sc->p.dsound.dsound == NULL);
    assert(sc->p.dsound.dsbuffer == NULL);

    if (memcmp(&sc->p.dsound.device_id, &zero_guid, sizeof(zero_guid)) == 0)
        hr = __DirectSoundCreate(NULL,&sc->p.dsound.dsound,NULL);
    else
        hr = __DirectSoundCreate(&sc->p.dsound.device_id,&sc->p.dsound.dsound,NULL);

    if (sc->p.dsound.dsound == NULL)
        return -1;

    sc->wav_state.is_open = 1;

    if (hwnd == NULL) hwnd = GetForegroundWindow();
    if (hwnd == NULL) hwnd = GetDesktopWindow();

    hr = IDirectSound_SetCooperativeLevel(sc->p.dsound.dsound, hwnd, DSSCL_PRIORITY);
    if (hr != DS_OK || sc->p.dsound.dsound == NULL) goto fail;

    return 0;
fail:
    dsound_close(sc);
    return -1;
}

static int dosamp_FAR dsound_close(soundcard_t sc) {
    if (!sc->wav_state.is_open) return 0;

    if (sc->p.dsound.dsbuffer != NULL) {
        IDirectSoundBuffer_Stop(sc->p.dsound.dsbuffer);
        IDirectSoundBuffer_Release(sc->p.dsound.dsbuffer);
        sc->p.dsound.dsbuffer = NULL;
    }

    if (sc->p.dsound.dsound != NULL) {
        IDirectSound_Release(sc->p.dsound.dsound);
        sc->p.dsound.dsound = NULL;
    }

    sc->wav_state.is_open = 0;
    return 0;
}

static int dosamp_FAR dsound_poll(soundcard_t sc) {
    (void)sc;

    return 0;
}

static int dosamp_FAR dsound_irq_callback(soundcard_t sc) {
    (void)sc;

    return 0;
}

static int dsound_prepare_play(soundcard_t sc) {
    /* format must be set */
    if (sc->cur_codec.sample_rate == 0)
        return -1;

    /* must be open */
    if (!sc->wav_state.is_open)
        return -1;

    if (sc->wav_state.prepared)
        return 0;

    IDirectSoundBuffer_Restore(sc->p.dsound.dsbuffer);

    sc->wav_state.prepared = 1;
    return 0;
}

static int dsound_unprepare_play(soundcard_t sc) {
    if (sc->wav_state.playing) return -1;

    if (sc->wav_state.prepared) {
        sc->wav_state.prepared = 0;
    }

    return 0;
}

static uint32_t dsound_play_buffer_play_pos(soundcard_t sc) {
    return sc->wav_state.play_counter;
}

static uint32_t dsound_play_buffer_write_pos(soundcard_t sc) {
    return sc->wav_state.write_counter;
}

static uint32_t dsound_play_buffer_size(soundcard_t sc) {
    DSBCAPS dsb;

    if (sc->p.dsound.dsbuffer == NULL) return 0;

    memset(&dsb, 0, sizeof(dsb));
    dsb.dwSize = sizeof(dsb);

    if (IDirectSoundBuffer_GetCaps(sc->p.dsound.dsbuffer, &dsb) != DS_OK)
        return 0;

    return dsb.dwBufferBytes;
}

static int dsound_start_playback(soundcard_t sc) {
    if (!sc->wav_state.prepared) return -1;
    if (sc->wav_state.playing) return 0;

    sc->wav_state.play_counter = 0;
    sc->wav_state.write_counter = 0;
    sc->wav_state.play_counter_prev = 0;

    IDirectSoundBuffer_Stop(sc->p.dsound.dsbuffer);

    sc->wav_state.playing = 1;
    return 0;
}

static int dsound_stop_playback(soundcard_t sc) {
    if (!sc->wav_state.playing) return 0;

    IDirectSoundBuffer_Stop(sc->p.dsound.dsbuffer);

    sc->wav_state.playing = 0;
    return 0;
}

static int dsound_set_play_format(soundcard_t sc,struct wav_cbr_t dosamp_FAR * const fmt) {
    DSCAPS dscaps;

    /* must be open */
    if (!sc->wav_state.is_open) return -1;

    /* not while prepared or playing!
     * assume: playing is not set unless prepared */
    if (sc->wav_state.prepared) return -1;

    /* close */
    if (sc->p.dsound.dsbuffer != NULL) {
        IDirectSoundBuffer_Release(sc->p.dsound.dsbuffer);
        sc->p.dsound.dsbuffer = NULL;
    }

    if (sc->p.dsound.dsound == NULL) return -1;

    /* sane limits */
    if (fmt->bits_per_sample < 8) fmt->bits_per_sample = 8;
    else if (fmt->bits_per_sample > 8) fmt->bits_per_sample = 16;

    if (fmt->number_of_channels < 1) fmt->number_of_channels = 1;
    else if (fmt->number_of_channels > 2) fmt->number_of_channels = 2;

    /* caps? */
    memset(&dscaps,0,sizeof(dscaps));
    dscaps.dwSize = sizeof(dscaps);
    if (IDirectSound_GetCaps(sc->p.dsound.dsound, &dscaps) != DS_OK) return -1;

    /* NTS: We're using a secondary buffer. That's what we check. */
    if (dscaps.dwMinSecondarySampleRate != 0) {
        if (fmt->sample_rate < dscaps.dwMinSecondarySampleRate)
            fmt->sample_rate = dscaps.dwMinSecondarySampleRate;
    }
    if (dscaps.dwMaxSecondarySampleRate != 0) {
        if (fmt->sample_rate > dscaps.dwMaxSecondarySampleRate)
            fmt->sample_rate = dscaps.dwMaxSecondarySampleRate;
    }

    if (fmt->bits_per_sample == 16 && !(dscaps.dwFlags & DSCAPS_SECONDARY16BIT))
        fmt->bits_per_sample = 8;
    else if (fmt->bits_per_sample == 8 && !(dscaps.dwFlags & DSCAPS_SECONDARY8BIT))
        fmt->bits_per_sample = 16;

    if (fmt->number_of_channels == 2 && !(dscaps.dwFlags & DSCAPS_SECONDARYSTEREO))
        fmt->number_of_channels = 1;
    else if (fmt->number_of_channels == 1 && !(dscaps.dwFlags & DSCAPS_SECONDARYMONO))
        fmt->number_of_channels = 2;

    /* PCM recalc */
    fmt->bytes_per_block = ((fmt->bits_per_sample+7U)/8U) * fmt->number_of_channels;

    /* WAVEFORMATEX */
    memset(&sc->p.dsound.dsbufferfmt,0,sizeof(sc->p.dsound.dsbufferfmt));
    sc->p.dsound.dsbufferfmt.wFormatTag = WAVE_FORMAT_PCM;
    sc->p.dsound.dsbufferfmt.nChannels = fmt->number_of_channels;
    sc->p.dsound.dsbufferfmt.nSamplesPerSec = fmt->sample_rate;
    sc->p.dsound.dsbufferfmt.nBlockAlign = fmt->bytes_per_block;
    sc->p.dsound.dsbufferfmt.wBitsPerSample = fmt->bits_per_sample;
    sc->p.dsound.dsbufferfmt.nAvgBytesPerSec = sc->p.dsound.dsbufferfmt.nSamplesPerSec * sc->p.dsound.dsbufferfmt.nBlockAlign;

    /* create secondary buffer */
    {
        DSBUFFERDESC dsd;
        uint32_t sz;
        HRESULT hr;

        sz = (uint32_t)fmt->sample_rate * (uint32_t)2 * (uint32_t)fmt->bytes_per_block;

        memset(&dsd,0,sizeof(dsd));
        dsd.dwSize = sizeof(dsd);
        dsd.dwFlags = DSBCAPS_GLOBALFOCUS;
        dsd.dwBufferBytes = sz;
        dsd.lpwfxFormat = &sc->p.dsound.dsbufferfmt;

        hr = IDirectSound_CreateSoundBuffer(sc->p.dsound.dsound, &dsd, &sc->p.dsound.dsbuffer, NULL);
        if (!SUCCEEDED(hr))
            goto fail;
        if (sc->p.dsound.dsbuffer == NULL)
            goto fail;
    }

    /* take it */
    sc->cur_codec = *fmt;

    return 0;
fail:
    if (sc->p.dsound.dsbuffer != NULL) {
        IDirectSoundBuffer_Release(sc->p.dsound.dsbuffer);
        sc->p.dsound.dsbuffer = NULL;
    }
    return -1;
}

static int dsound_get_card_name(soundcard_t sc,void dosamp_FAR *data,unsigned int dosamp_FAR *len) {
    const char *str;

    (void)sc;

    if (data == NULL || len == NULL) return -1;
    if (*len == 0U) return -1;

    str = "DirectSound";

    soundcard_str_return_common((char dosamp_FAR*)data,len,str);
    return 0;
}

static int dsound_get_card_detail(soundcard_t sc,void dosamp_FAR *data,unsigned int dosamp_FAR *len) {
    char *w;

    if (data == NULL || len == NULL) return -1;
    if (*len == 0U) return -1;

    w = soundcard_str_tmp;
    w += sprintf(w,"DirectSound device");

    assert(w < (soundcard_str_tmp+sizeof(soundcard_str_tmp)));

    soundcard_str_return_common((char dosamp_FAR*)data,len,soundcard_str_tmp);
    return 0;
}

static int dosamp_FAR dsound_ioctl(soundcard_t sc,unsigned int cmd,void dosamp_FAR *data,unsigned int dosamp_FAR * len,int ival) {
    (void)ival;

    switch (cmd) {
        case soundcard_ioctl_get_card_name:
            return dsound_get_card_name(sc,data,len);
        case soundcard_ioctl_get_card_detail:
            return dsound_get_card_detail(sc,data,len);
        case soundcard_ioctl_set_play_format:
            if (data == NULL || len == 0) return -1;
            if (*len < sizeof(struct wav_cbr_t)) return -1;
            return dsound_set_play_format(sc,(struct wav_cbr_t dosamp_FAR *)data);
        case soundcard_ioctl_prepare_play:
            return dsound_prepare_play(sc);
        case soundcard_ioctl_unprepare_play:
            return dsound_unprepare_play(sc);
        case soundcard_ioctl_start_play:
            return dsound_start_playback(sc);
        case soundcard_ioctl_stop_play:
            return dsound_stop_playback(sc);
        case soundcard_ioctl_get_buffer_write_position: {
            if (data == NULL || len == 0) return -1;
            if (*len < sizeof(uint32_t)) return -1;
            if ((*((uint32_t dosamp_FAR*)data) = dsound_play_buffer_write_pos(sc)) == 0) return -1;
            } return 0;
        case soundcard_ioctl_get_buffer_play_position: {
            if (data == NULL || len == 0) return -1;
            if (*len < sizeof(uint32_t)) return -1;
            if ((*((uint32_t dosamp_FAR*)data) = dsound_play_buffer_play_pos(sc)) == 0) return -1;
            } return 0;
        case soundcard_ioctl_get_buffer_size: {
            if (data == NULL || len == 0) return -1;
            if (*len < sizeof(uint32_t)) return -1;
            if ((*((uint32_t dosamp_FAR*)data) = dsound_play_buffer_size(sc)) == 0) return -1;
            } return 0;
    }

    return -1;
}

struct soundcard dsound_soundcard_template = {
    .driver =                                   soundcard_dsound,
    .capabilities =                             0,
    .requirements =                             0,
    .can_write =                                dsound_can_write,
    .open =                                     dsound_open,
    .close =                                    dsound_close,
    .poll =                                     dsound_poll,
    .clamp_if_behind =                          dsound_clamp_if_behind,
    .irq_callback =                             dsound_irq_callback,
    .write =                                    dsound_buffer_write,
    .mmap_write =                               dsound_mmap_write,
    .ioctl =                                    dsound_ioctl
};

static IDirectSound *dsound_test_create(GUID *guid) {
    IDirectSound *ds = NULL;
    HRESULT hr;

    if (__DirectSoundCreate == NULL) return NULL;

    if (memcmp(guid, &zero_guid, sizeof(zero_guid)) == 0)
        hr = __DirectSoundCreate(NULL/*default*/,&ds,NULL);
    else
        hr = __DirectSoundCreate(guid,&ds,NULL);

    return ds;
}

static void dsound_add(soundcard_t sc,IDirectSound *ds,GUID *guid) {
    (void)ds;

    sc->p.dsound.device_id = *guid;
}

int probe_for_dsound(void) {
    if (dsound_probed) return 0;

    if (!init_dsound()) return 0;
    if (__DirectSoundCreate == NULL) return 0;

    /* just go with the default, call it good */
    {
        IDirectSound *ds;
        soundcard_t sc;

        if ((ds=dsound_test_create(&zero_guid)) != NULL) {
            sc = soundcardlist_new(&dsound_soundcard_template);
            if (sc != NULL) dsound_add(sc, ds, &zero_guid);
            IDirectSound_Release(ds);
        }
    }

    dsound_probed = 1;
    return 0;
}

void free_dsound_support(void) {
    dsound_probed = 0;
}

#endif /* defined(HAS_OSS) */

