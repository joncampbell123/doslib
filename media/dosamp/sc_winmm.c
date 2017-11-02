
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

#define MMSYSTEM_USERFL_VIRGIN      (1U << 0U)

int init_mmsystem(void);

#define WAVE_INVALID_HANDLE         ((HWAVEOUT) NULL)

static unsigned char                mmsystem_probed = 0;

UINT (WINAPI *__waveOutClose)(HWAVEOUT) = NULL;
UINT (WINAPI *__waveOutReset)(HWAVEOUT) = NULL;
UINT (WINAPI *__waveOutWrite)(HWAVEOUT,LPWAVEHDR,UINT) = NULL;
UINT (WINAPI *__waveOutOpen)(LPHWAVEOUT,UINT,LPWAVEFORMAT,DWORD,DWORD,DWORD) = NULL;
UINT (WINAPI *__waveOutPrepareHeader)(HWAVEOUT,LPWAVEHDR,UINT) = NULL;
UINT (WINAPI *__waveOutUnprepareHeader)(HWAVEOUT,LPWAVEHDR,UINT) = NULL;
UINT (WINAPI *__waveOutGetPosition)(HWAVEOUT,LPMMTIME,UINT) = NULL;
UINT (WINAPI *__waveOutGetDevCaps)(UINT,LPWAVEOUTCAPS,UINT) = NULL;
UINT (WINAPI *__waveOutGetNumDevs)(void) = NULL;

static void free_fragment_array(soundcard_t sc);

static int alloc_fragment_array(soundcard_t sc) {
    struct soundcard_priv_mmsystem_wavhdr *wh;
    unsigned int i;

    if (sc->wav_state.playing)
        return -1;
    if (sc->p.mmsystem.fragment_count == 0 || sc->p.mmsystem.fragment_size <= 16)
        return -1;
    if (sc->p.mmsystem.fragment_count >= 100)
        return -1;
    if (sc->p.mmsystem.fragments != NULL)
        return 0;

    /* allocate array. use calloc so we don't have to memset() ourselves */
    sc->p.mmsystem.fragments =
        (struct soundcard_priv_mmsystem_wavhdr*)
        calloc(sc->p.mmsystem.fragment_count,sizeof(struct soundcard_priv_mmsystem_wavhdr));
    if (sc->p.mmsystem.fragments == NULL)
        return -1;

    /* then allocate memory for each fragment */
    for (i=0;i < sc->p.mmsystem.fragment_count;i++) {
        wh = sc->p.mmsystem.fragments + i;

#if TARGET_MSDOS == 16
        wh->hdr.lpData = _fmalloc(sc->p.mmsystem.fragment_size);
#else
        wh->hdr.lpData = malloc(sc->p.mmsystem.fragment_size);
#endif
        if (wh->hdr.lpData == NULL) goto fail;
        wh->hdr.dwBufferLength = sc->p.mmsystem.fragment_size;
    }

    sc->p.mmsystem.fragment_next = 0;
    return 0;
fail:
    free_fragment_array(sc);
    return -1;
}

static void free_fragment(struct soundcard_priv_mmsystem_wavhdr *wh) {
    /* NTS: We trust Windows not to clear lpData! I will rewrite this code if Windows violates that trust. */
    if (wh->hdr.lpData != NULL) {
#if TARGET_MSDOS == 16
        _ffree(wh->hdr.lpData);
#else
        free(wh->hdr.lpData);
#endif
        wh->hdr.lpData = NULL;
    }

    memset(wh,0,sizeof(*wh));
}

static void free_fragment_array(soundcard_t sc) {
    unsigned int i;

    /* never while playing! otherwise, fragments are "unprepared" and can be freed. */
    if (sc->wav_state.playing)
        return;

    if (sc->p.mmsystem.fragments != NULL) {
        for (i=0;i < sc->p.mmsystem.fragment_count;i++)
            free_fragment(sc->p.mmsystem.fragments+i);

        free(sc->p.mmsystem.fragments);
        sc->p.mmsystem.fragments = NULL;
    }
}

static void unprepare_fragment(soundcard_t sc,struct soundcard_priv_mmsystem_wavhdr *wh) {
    /* NTS: It would be sanest to check if WHDR_DONE, and the driver is supposed to set it
     *      upon completion of playing the block, but we can't assume the driver will set
     *      it if we call something like waveOutReset, so we don't check. */
    if (wh->hdr.dwFlags & WHDR_PREPARED)
        __waveOutUnprepareHeader(sc->p.mmsystem.handle, &wh->hdr, sizeof(wh->hdr));

    wh->write_pos = 0;
}

static void unprepare_fragment_array(soundcard_t sc) {
    unsigned int i;

    /* never while playing! */
    if (sc->wav_state.playing)
        return;

    if (sc->p.mmsystem.fragments != NULL) {
        for (i=0;i < sc->p.mmsystem.fragment_count;i++)
            unprepare_fragment(sc,sc->p.mmsystem.fragments+i);
    }
}

static int prepare_fragment(soundcard_t sc,struct soundcard_priv_mmsystem_wavhdr *wh) {
    if (wh->hdr.dwFlags & WHDR_PREPARED)
        return 0; /* already prepared */

    if (__waveOutPrepareHeader(sc->p.mmsystem.handle, &wh->hdr, sizeof(wh->hdr)) != 0)
        return -1;

    /* we can't assume the MMSYSTEM driver will tolerate us setting WHDR_DONE before it's done,
     * so use the dwUser field for our use instead */
    wh->hdr.dwUser = MMSYSTEM_USERFL_VIRGIN;
    wh->write_pos = 0;
    return 0;
}

static int prepare_fragment_array(soundcard_t sc) {
    unsigned int i;

    /* never while playing! */
    if (sc->wav_state.playing)
        return -1;

    /* allocate the fragments first, idiot */
    if (sc->p.mmsystem.fragments == NULL)
        return -1;

    for (i=0;i < sc->p.mmsystem.fragment_count;i++) {
        if (prepare_fragment(sc,sc->p.mmsystem.fragments+i) < 0)
            goto fail;
    }

    return 0;
fail:
    unprepare_fragment_array(sc);
    return -1;
}

static int dosamp_FAR mmsystem_poll(soundcard_t sc);

/* this depends on keeping the "play delay" up to date */
static uint32_t dosamp_FAR mmsystem_can_write_inner(soundcard_t sc) { /* in bytes */
    struct soundcard_priv_mmsystem_wavhdr *wh;
    uint32_t count = 0;
    unsigned int i,c;

    if (!sc->wav_state.playing)
        return 0;

    /* assume fragments != NULL.
     * we shouldn't be in playing state unless the fragment array was allocated */

    i = sc->p.mmsystem.fragment_next;
    for (c=0;c < sc->p.mmsystem.fragment_count;c++) {
        wh = sc->p.mmsystem.fragments + i;

        /* assume lpData != NULL, deBufferLength != 0, buffer prepared.
         * we shouldn't be here unless the fragment array was allocated and each fragment allocated and prepared. */
        /* we can use the buffer if the driver marked it as done (completing playback) OR we marked it ourself
         * as "new" and not yet sent to the driver. when we sent it to the driver we clear WHDR_DONE and
         * MMSYSTEM_USERFL_VIRGIN right before we do so. */
        if ((wh->hdr.dwFlags & WHDR_DONE) || (wh->hdr.dwUser & MMSYSTEM_USERFL_VIRGIN))
            count += wh->hdr.dwBufferLength - wh->write_pos;
        else
            break;

        if ((++i) >= sc->p.mmsystem.fragment_count)
            i = 0;
    }

    return count;
}

static uint32_t dosamp_FAR mmsystem_can_write(soundcard_t sc) { /* in bytes */
    uint32_t r;

    r = mmsystem_can_write_inner(sc);
    mmsystem_poll(sc);
    return r;
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

static int submit_fragment(soundcard_t sc,struct soundcard_priv_mmsystem_wavhdr *wh) {
    if (!(wh->hdr.dwFlags & WHDR_PREPARED))
        return -1;
    if (wh->hdr.lpData == NULL || wh->hdr.dwBufferLength == 0 || wh->hdr.dwBufferLength > sc->p.mmsystem.fragment_size)
        return -1;

    /* prepare to send to driver */
    wh->hdr.dwFlags &= ~WHDR_DONE;
    wh->hdr.dwUser &= ~MMSYSTEM_USERFL_VIRGIN;

    /* send */
    if (__waveOutWrite(sc->p.mmsystem.handle, &wh->hdr, sizeof(wh->hdr)) != 0)
        return -1;

    wh->write_pos = 0;
    return 0;
}

/* non-mmap write (much like OSS or ALSA in Linux where you do not have direct access to the hardware buffer) */
static unsigned int dosamp_FAR mmsystem_buffer_write(soundcard_t sc,const unsigned char dosamp_FAR * buf,unsigned int len) {
    struct soundcard_priv_mmsystem_wavhdr *wh;
    unsigned int count = 0;
    unsigned int i,c;

    if (!sc->wav_state.playing)
        return 0;

    /* assume fragments != NULL.
     * we shouldn't be in playing state unless the fragment array was allocated */

    /* we MUST use the same algorithm as can_write() to ensure the caller can write what they actually intend to
     * write. normal audio playback should do nothing but increase the amount of data the caller can write. */

    i = sc->p.mmsystem.fragment_next;
    for (c=0;len != 0U && c < sc->p.mmsystem.fragment_count;c++) {
        wh = sc->p.mmsystem.fragments + i;

        /* assume lpData != NULL, deBufferLength != 0, fragment prepared.
         * we shouldn't be here unless the fragment array was allocated and each fragment allocated and prepared. */
        /* we can use the buffer if the driver marked it as done (completing playback) OR we marked it ourself
         * as "new" and not yet sent to the driver. when we sent it to the driver we clear WHDR_DONE and
         * MMSYSTEM_USERFL_VIRGIN right before we do so. */
        if ((wh->hdr.dwFlags & WHDR_DONE) || (wh->hdr.dwUser & MMSYSTEM_USERFL_VIRGIN)) {
            unsigned int howmuch = (unsigned int)wh->hdr.dwBufferLength - wh->write_pos;

            if (howmuch > len) howmuch = len;

            /* track write count */
            if (wh->write_pos == 0)
                wh->write_counter_base = sc->wav_state.write_counter;

#if TARGET_MSDOS == 16
            _fmemcpy(wh->hdr.lpData + wh->write_pos,buf,howmuch);
#else
            memcpy(wh->hdr.lpData + wh->write_pos,buf,howmuch);
#endif
            sc->wav_state.write_counter += howmuch;
            wh->write_pos += howmuch;
            count += howmuch;
            buf += howmuch;
            len -= howmuch;

            /* if the fragment is full, send it to the driver and move on */
            if (wh->write_pos >= wh->hdr.dwBufferLength) {
                if (submit_fragment(sc,wh) < 0)
                    break;

                if ((++i) >= sc->p.mmsystem.fragment_count)
                    i = 0;

                sc->p.mmsystem.fragment_next = i;
                continue;
            }
            else {
                break;
            }
        }
        else {
            break;
        }
    }

    return count;
}

static int dosamp_FAR mmsystem_open(soundcard_t sc) {
    if (sc->wav_state.is_open) return -1; /* already open! */

    sc->wav_state.is_open = 1;
    return 0;
}

static int mmsystem_unprepare_play(soundcard_t sc);
static int mmsystem_stop_playback(soundcard_t sc);

static int dosamp_FAR mmsystem_close(soundcard_t sc) {
    if (!sc->wav_state.is_open) return 0;

    mmsystem_unprepare_play(sc);
    mmsystem_stop_playback(sc);

    sc->wav_state.is_open = 0;
    return 0;
}

static uint32_t mmsystem_play_buffer_size(soundcard_t sc);

static int dosamp_FAR mmsystem_poll(soundcard_t sc) {
    uint64_t minn;
    MMTIME mm;

    if (!sc->wav_state.playing) return 0;

    memset(&mm,0,sizeof(mm));
    mm.wType = TIME_BYTES;

    if (__waveOutGetPosition(sc->p.mmsystem.handle, &mm, sizeof(mm)) == 0) {
        /* FIXME:
         *
         *   The "Tandy SBP16" driver in Windows 3.0 is laughably lame about the "current position".
         *   Apparently they took a shortcut where, instead of actually tracking the DMA pointer
         *   and playback position, they read once and do not update the playback count until
         *   either buffers are unprepared or until all buffers finish playing.
         *
         *   So under Windows 3.0, this code will see a play counter that jumps forward every so
         *   often as determined by our buffer size or half of it (hit '2' in DOSAMP to see what
         *   I mean from the buffer statistics).
         *
         *   This is the reason why DOSAMP playback in Windows 3.0 with the SBP16 driver is so
         *   prone to halt and stutter every so often.
         *
         *   The fix:
         *
         *   We need to scan the fragment list (with a tracking index) to watch each fragment
         *   finish playing (dwFlags & WHDR_DONE) and track playback from that in addition
         *   to the waveOutGetPosition call, then use the two values (one from waveOutGetPosition
         *   and the other from our fragment tracking) to come up with the final play counter
         *   to return to the host application.
         *
         *   I can only hope that the SBP16 driver is just a bad apple and that not all Windows 3.x
         *   sound drivers are that shitty. --J.C.
         *
         */
        /* count the play counter.
         * we do this so that we could (theoretically) count properly even beyond
         * the limits of the 32-bit byte counter provided by the driver. */
        sc->wav_state.play_counter_prev = sc->wav_state.play_counter;
        sc->wav_state.play_counter += (uint64_t)(mm.u.cb - sc->p.mmsystem.p_cb);
        sc->p.mmsystem.p_cb = mm.u.cb;

        /* sanity check, in case of errant drivers (as was common in the Windows 3.x era) */
        if (sc->wav_state.play_counter > sc->wav_state.write_counter)
            sc->wav_state.play_counter = sc->wav_state.write_counter;

        /* another, in case the driver can't count properly */
        {
            uint32_t sz = mmsystem_play_buffer_size(sc) - mmsystem_can_write_inner(sc);

            minn = sc->wav_state.write_counter;
            if (minn >= (uint64_t)sz) minn -= sz;
            else minn = 0;
        }
        if (sc->wav_state.play_counter < minn)
            sc->wav_state.play_counter = minn;

        /* now we can provide play delay */
        /* assume play_counter <= write_counter */
        sc->wav_state.play_delay_bytes = (uint32_t)(sc->wav_state.write_counter - sc->wav_state.play_counter);
        sc->wav_state.play_delay = sc->wav_state.play_delay_bytes / sc->cur_codec.bytes_per_block;
    }

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

    sc->p.mmsystem.p_cb = 0;
    sc->wav_state.play_counter = 0;
    sc->wav_state.write_counter = 0;

    /* set up the fragment array */
    if (alloc_fragment_array(sc) < 0)
        return -1;

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

        r = __waveOutOpen(&sc->p.mmsystem.handle, sc->p.mmsystem.device_id, (LPWAVEFORMAT)(&pcm), (DWORD)NULL, (DWORD)NULL, 0);
        if (r != 0) return -1;
    }

    sc->wav_state.prepared = 1;
    sc->p.mmsystem.fragment_next = 0;
    return 0;
}

static int mmsystem_unprepare_play(soundcard_t sc) {
    if (sc->wav_state.playing) return -1;

    if (sc->wav_state.prepared) {
        /* close the sound device */
        if (sc->p.mmsystem.handle != WAVE_INVALID_HANDLE) {
            while (__waveOutReset(sc->p.mmsystem.handle) == WAVERR_STILLPLAYING);
            unprepare_fragment_array(sc);
            while (__waveOutClose(sc->p.mmsystem.handle) == WAVERR_STILLPLAYING);
            sc->p.mmsystem.handle = WAVE_INVALID_HANDLE;
        }

        sc->wav_state.prepared = 0;
        free_fragment_array(sc);
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
    return  (uint32_t)sc->p.mmsystem.fragment_size *
            (uint32_t)sc->p.mmsystem.fragment_count;
}

static int mmsystem_start_playback(soundcard_t sc) {
    if (!sc->wav_state.prepared) return -1;
    if (sc->wav_state.playing) return 0;
    if (sc->p.mmsystem.fragments == NULL) return -1;

    sc->p.mmsystem.p_cb = 0;
    sc->wav_state.play_counter = 0;
    sc->wav_state.write_counter = 0;
    sc->wav_state.play_counter_prev = 0;

    while (__waveOutReset(sc->p.mmsystem.handle) == WAVERR_STILLPLAYING);
    unprepare_fragment_array(sc);

    if (prepare_fragment_array(sc) < 0)
        return -1;

    sc->wav_state.playing = 1;
    return 0;
}

static int mmsystem_stop_playback(soundcard_t sc) {
    if (!sc->wav_state.playing) return 0;

    while (__waveOutReset(sc->p.mmsystem.handle) == WAVERR_STILLPLAYING);
    unprepare_fragment_array(sc);
    sc->wav_state.playing = 0;
    return 0;
}

static unsigned short try_rates[] = {
    44100,
    22050,
    11025,
    8000,
    6000,
    5512,
    4000,

    0
};

/* Windows 3.0 drivers do not fill in the WAVECAPS flags regarding supported formats,
 * so to work with these drivers we have to play "20 questions" with the sample rate. */
static int mmsystem_sample_rate_20_questions(soundcard_t sc,struct wav_cbr_t dosamp_FAR * const fmt,PCMWAVEFORMAT *pcm) {
    uint32_t old_rate = fmt->sample_rate;
    int r = WAVERR_BADFORMAT;
    unsigned int i = 0;

    /* Windows 3.0 makes us guess. Play 20 questions. */
    while (r == WAVERR_BADFORMAT && try_rates[i] != 0) {
        if (fmt->sample_rate > try_rates[i]) {
            fmt->sample_rate = try_rates[i];
            pcm->wf.nSamplesPerSec = fmt->sample_rate;
            pcm->wf.nAvgBytesPerSec = fmt->sample_rate * fmt->bytes_per_block;

            r = __waveOutOpen(NULL, sc->p.mmsystem.device_id, (LPWAVEFORMAT)pcm, (DWORD)NULL, (DWORD)NULL, WAVE_FORMAT_QUERY);
        }

        i++;
    }

    /* couldn't negotiate. restore original sample rate */
    if (r == WAVERR_BADFORMAT) {
        fmt->sample_rate = old_rate;
        pcm->wf.nSamplesPerSec = fmt->sample_rate;
        pcm->wf.nAvgBytesPerSec = fmt->sample_rate * fmt->bytes_per_block;
    }

    return r;
}

static int mmsystem_set_play_format(soundcard_t sc,struct wav_cbr_t dosamp_FAR * const fmt) {
    PCMWAVEFORMAT pcm;
    WAVEOUTCAPS caps;
    UINT r;

    /* FIXME:
     *
     *    Windows 3.0 / Tandy SBP16 driver:
     *
     *    The driver seems to be written against Sound Blaster Pro hardware.
     *    However, it's capabilities seem to imply it can do 16-bit when it can't (or, DOSBox
     *    doesn't emulate how it does it?)
     *
     *    In any case, whenever the driver takes on a format it can't actually play, it
     *    flat out just doesn't do anything when you send audio. That includes out of
     *    spec DSP speeds like 44100Hz stereo.
     *
     *    How do we resolve this? --J.C. */

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
        if (sc->p.mmsystem.hacks.never_16bit)
            fmt->bits_per_sample = 8;
        else if ((caps.dwFormats & (WAVE_FORMAT_1M16|WAVE_FORMAT_1S16|WAVE_FORMAT_2M16|WAVE_FORMAT_2S16|WAVE_FORMAT_4M16|WAVE_FORMAT_4S16)) == 0)
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
    r = __waveOutOpen(NULL, sc->p.mmsystem.device_id, (LPWAVEFORMAT)(&pcm), (DWORD)NULL, (DWORD)NULL, WAVE_FORMAT_QUERY);
    if (r == WAVERR_BADFORMAT && pcm.wBitsPerSample == 16) {
        /* drop to 8-bit. is OK? */
        fmt->bits_per_sample = 8;
        fmt->bytes_per_block = ((fmt->bits_per_sample+7U)/8U) * fmt->number_of_channels;

        pcm.wf.nAvgBytesPerSec = fmt->sample_rate * fmt->bytes_per_block;
        pcm.wf.nBlockAlign = fmt->bytes_per_block;
        pcm.wBitsPerSample = fmt->bits_per_sample;

        r = __waveOutOpen(NULL, sc->p.mmsystem.device_id, (LPWAVEFORMAT)(&pcm), (DWORD)NULL, (DWORD)NULL, WAVE_FORMAT_QUERY);
    }

    if (r == WAVERR_BADFORMAT)
        r = mmsystem_sample_rate_20_questions(sc,fmt,&pcm);

    if (r == WAVERR_BADFORMAT && pcm.wf.nChannels > 1) {
        /* drop to mono. is OK? */
        fmt->number_of_channels = 1;
        fmt->bytes_per_block = ((fmt->bits_per_sample+7U)/8U) * fmt->number_of_channels;

        pcm.wf.nAvgBytesPerSec = fmt->sample_rate * fmt->bytes_per_block;
        pcm.wf.nBlockAlign = fmt->bytes_per_block;
        pcm.wBitsPerSample = fmt->bits_per_sample;

        r = __waveOutOpen(NULL, sc->p.mmsystem.device_id, (LPWAVEFORMAT)(&pcm), (DWORD)NULL, (DWORD)NULL, WAVE_FORMAT_QUERY);
    }

    if (r == WAVERR_BADFORMAT)
        r = mmsystem_sample_rate_20_questions(sc,fmt,&pcm);

    if (r == WAVERR_BADFORMAT) {
        printf("FORMAT FAIL\n");
        return -1;
    }

    /* PCM recalc */
    fmt->bytes_per_block = ((fmt->bits_per_sample+7U)/8U) * fmt->number_of_channels;

    /* take it */
    sc->cur_codec = *fmt;

    /* compute fragment size and count for the format.
     * set one fragment to 1/10th of a second.
     * 20 fragments (2 seconds). */
    free_fragment_array(sc);

    if (sc->p.mmsystem.hacks.min_4kbperchannel) {
        /* alternative algorithm for old shitty Windows 3.0 drivers that like to
         * group audio playback around 4KB/8KB blocks */
        uint32_t sz = (uint32_t)4096 * (uint32_t)fmt->bytes_per_block;
        uint32_t final_sz = (uint32_t)fmt->sample_rate * (uint32_t)2 * (uint32_t)fmt->bytes_per_block;

        if (final_sz < (sz * 4UL))
            final_sz = (sz * 4UL);
        else
            final_sz -= final_sz % sz;

        sc->p.mmsystem.fragment_size = (unsigned int)sz;
        sc->p.mmsystem.fragment_count = (unsigned int)(final_sz / sz);
    }
    else {
        uint32_t sz = (fmt->sample_rate / (uint32_t)10) * (uint32_t)fmt->bytes_per_block;
        if (sz > 32768UL) sz = 32768UL;
        sc->p.mmsystem.fragment_size = (unsigned int)sz;
        sc->p.mmsystem.fragment_count = 20;
    }

    return 0;
}

static int mmsystem_get_card_name(soundcard_t sc,void dosamp_FAR *data,unsigned int dosamp_FAR *len) {
    const char *str;

    (void)sc;

    if (data == NULL || len == NULL) return -1;
    if (*len == 0U) return -1;

    str = "Windows Sound System";

    soundcard_str_return_common((char dosamp_FAR*)data,len,str);
    return 0;
}

static int mmsystem_get_card_detail(soundcard_t sc,void dosamp_FAR *data,unsigned int dosamp_FAR *len) {
    WAVEOUTCAPS caps;
    char *w;

    if (data == NULL || len == NULL) return -1;
    if (*len == 0U) return -1;

    memset(&caps,0,sizeof(caps));
    __waveOutGetDevCaps(sc->p.mmsystem.device_id, &caps, sizeof(caps));

    w = soundcard_str_tmp;
    if (caps.szPname[0] != 0)
        w += sprintf(w,"%s",caps.szPname);
    else
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

static int mmsystem_device_exists(soundcard_t sc,const UINT dev_id,WAVEOUTCAPS *caps) {
    (void)sc;

    memset(caps,0,sizeof(*caps));
    if (__waveOutGetDevCaps(dev_id, caps, sizeof(*caps)) != 0)
        return 0;

    return 1;
}

int probe_for_mmsystem(void) {
    UINT devs;

    if (mmsystem_probed) return 0;

    if (!init_mmsystem()) return 0;

#if TARGET_MSDOS == 16
    if ((__waveOutGetPosition=((UINT (WINAPI *)(HWAVEOUT,LPMMTIME,UINT))GetProcAddress(mmsystem_dll,"WAVEOUTGETPOSITION"))) == NULL)
        return 0;
    if ((__waveOutGetDevCaps=((UINT (WINAPI *)(UINT,LPWAVEOUTCAPS,UINT))GetProcAddress(mmsystem_dll,"WAVEOUTGETDEVCAPS"))) == NULL)
        return 0;
    if ((__waveOutOpen=((UINT (WINAPI *)(LPHWAVEOUT,UINT,LPWAVEFORMAT,DWORD,DWORD,DWORD))GetProcAddress(mmsystem_dll,"WAVEOUTOPEN"))) == NULL)
        return 0;
    if ((__waveOutClose=((UINT (WINAPI *)(HWAVEOUT))GetProcAddress(mmsystem_dll,"WAVEOUTCLOSE"))) == NULL)
        return 0;
    if ((__waveOutReset=((UINT (WINAPI *)(HWAVEOUT))GetProcAddress(mmsystem_dll,"WAVEOUTRESET"))) == NULL)
        return 0;
    if ((__waveOutWrite=((UINT (WINAPI *)(HWAVEOUT,LPWAVEHDR,UINT))GetProcAddress(mmsystem_dll,"WAVEOUTWRITE"))) == NULL)
        return 0;
    if ((__waveOutGetNumDevs=((UINT (WINAPI *)(void))GetProcAddress(mmsystem_dll,"WAVEOUTGETNUMDEVS"))) == NULL)
        return 0;
    if ((__waveOutPrepareHeader=((UINT (WINAPI *)(HWAVEOUT,LPWAVEHDR,UINT))GetProcAddress(mmsystem_dll,"WAVEOUTPREPAREHEADER"))) == NULL)
        return 0;
    if ((__waveOutUnprepareHeader=((UINT (WINAPI *)(HWAVEOUT,LPWAVEHDR,UINT))GetProcAddress(mmsystem_dll,"WAVEOUTUNPREPAREHEADER"))) == NULL)
        return 0;
#else
    if ((__waveOutGetPosition=((UINT (WINAPI *)(HWAVEOUT,LPMMTIME,UINT))GetProcAddress(mmsystem_dll,"waveOutGetPosition"))) == NULL)
        return 0;
    if ((__waveOutGetDevCaps=((UINT (WINAPI *)(UINT,LPWAVEOUTCAPS,UINT))GetProcAddress(mmsystem_dll,"waveOutGetDevCapsA"))) == NULL)
        return 0;
    if ((__waveOutOpen=((UINT (WINAPI *)(LPHWAVEOUT,UINT,LPWAVEFORMAT,DWORD,DWORD,DWORD))GetProcAddress(mmsystem_dll,"waveOutOpen"))) == NULL)
        return 0;
    if ((__waveOutClose=((UINT (WINAPI *)(HWAVEOUT))GetProcAddress(mmsystem_dll,"waveOutClose"))) == NULL)
        return 0;
    if ((__waveOutReset=((UINT (WINAPI *)(HWAVEOUT))GetProcAddress(mmsystem_dll,"waveOutReset"))) == NULL)
        return 0;
    if ((__waveOutWrite=((UINT (WINAPI *)(HWAVEOUT,LPWAVEHDR,UINT))GetProcAddress(mmsystem_dll,"waveOutWrite"))) == NULL)
        return 0;
    if ((__waveOutGetNumDevs=((UINT (WINAPI *)(void))GetProcAddress(mmsystem_dll,"waveOutGetNumDevs"))) == NULL)
        return 0;
    if ((__waveOutPrepareHeader=((UINT (WINAPI *)(HWAVEOUT,LPWAVEHDR,UINT))GetProcAddress(mmsystem_dll,"waveOutPrepareHeader"))) == NULL)
        return 0;
    if ((__waveOutUnprepareHeader=((UINT (WINAPI *)(HWAVEOUT,LPWAVEHDR,UINT))GetProcAddress(mmsystem_dll,"waveOutUnprepareHeader"))) == NULL)
        return 0;
#endif

    devs = __waveOutGetNumDevs();
    if (devs != 0U) {
        /* just point at the wave mapper and call it good */
        unsigned char dont;
        WAVEOUTCAPS caps;
        soundcard_t sc;
        UINT dev_id;

        dont = 0;
        /* Windows 3.1 and higher have a wave mapper (usually), and we can just use that.
         * Windows 3.0 does NOT have a wave mapper, and we'll have to use the first sound card instead. */
        if (mmsystem_device_exists(sc,WAVE_MAPPER,&caps))
            dev_id = WAVE_MAPPER;
        else if (mmsystem_device_exists(sc,0,&caps))
            dev_id = 0;
        else
            dont = 1;

        if (!dont) {
            sc = soundcardlist_new(&mmsystem_soundcard_template);
            if (sc != NULL) {
                sc->p.mmsystem.device_id = dev_id;

                /* Hacks collection.
                 *
                 * MMSYSTEM drivers will be identified here who have issues and hacks will be enabled for them. */

                /* Tandy SBP16 Windows 3.0 drivers like to collect and group buffers into 4KB mono 8KB stereo buffers
                 * before playing. If we try to use small fragments we get audio that starts, pauses at startup, and
                 * just barely avoids underrun during playback. */
                if (caps.wMid == 0x0002 && caps.wPid == 0x0068) { /* Tandy SBP16 */
                    /* NTS: I will add a check for vDriverVersion if later revisions fix the issue. */
                    sc->p.mmsystem.hacks.min_4kbperchannel = 1;
                    sc->p.mmsystem.hacks.never_16bit = 1; /* does not support 16-bit PCM */
                }
            }
        }
    }

    mmsystem_probed = 1;
    return 0;
}

void free_mmsystem_support(void) {
    mmsystem_probed = 0;
}

#endif /* defined(HAS_OSS) */

