
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

/*====================Sound Blaster Specific=======================*/
/* section off from main dosamp.c.
 * will eventually become it's own module of code
 * through a more generic interface that would enable the use of
 * other sound cards. */

/* Sound Blaster sound card */
static struct sndsb_ctx*                        sb_card = NULL;

/*====================End Sound Blaster Specific=======================*/

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
static unsigned char                            resample_on = 0;

/* chosen time source.
 * NTS: Don't forget that by design, some time sources (8254 for example)
 *      require you to poll often enough to get a correct count of time. */
static dosamp_time_source_t                     time_source = NULL;

/* ISA DMA buffer */
static struct dma_8237_allocation*              isa_dma = NULL;

/* chosen file to play */
static dosamp_file_source_t                     wav_source = NULL;
static char*                                    wav_file = NULL;

/* convert/read buffer */
struct convert_rdbuf_t                          convert_rdbuf = {NULL,0,0,0};

struct wav_cbr_t {
    uint32_t                            sample_rate;
    uint16_t                            bytes_per_block;
    uint16_t                            samples_per_block;
    uint8_t                             number_of_channels; /* nobody's going to ask us to play 4096 channel-audio! */
    uint8_t                             bits_per_sample;    /* nor will they ask us to play 512-bit PCM audio! */
};

struct wav_cbr_t                        file_codec;
struct wav_cbr_t                        play_codec;

struct wav_state_t {
    uint32_t                            dma_position;
    uint32_t                            play_delay_bytes;/* in bytes. delay from wav_position to where sound card is playing now. */
    uint32_t                            play_delay;/* in samples. delay from wav_position to where sound card is playing now. */
    uint64_t                            write_counter;
    uint64_t                            play_counter;
    uint64_t                            play_counter_prev;
    unsigned int                        play_empty:1;
    unsigned int                        prepared:1;
    unsigned int                        playing:1;
};

static struct wav_state_t               wav_state;

void wav_state_init(void) {
    memset(&wav_state,0,sizeof(wav_state));
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

void resampler_state_reset(struct resampler_state_t *r) {
    r->init = 0;
    r->p[0] = 0;
    r->p[1] = 0;
    r->c[0] = 0;
    r->c[1] = 0;
}

struct resampler_state_t                resample_state;

void update_wav_dma_position(void) {
    _cli();
    wav_state.dma_position = sndsb_read_dma_buffer_position(sb_card);
    _sti();
}

/* WARNING!!! This interrupt handler calls subroutines. To avoid system
 * instability in the event the IRQ fires while, say, executing a routine
 * in the DOS kernel, you must compile this code with the -zu flag in
 * 16-bit real mode Large and Compact memory models! Without -zu, minor
 * memory corruption in the DOS kernel will result and the system will
 * hang and/or crash. */
unsigned char               old_irq_masked = 0;
static void                 (interrupt *old_irq)() = NULL;

static void interrupt sb_irq() {
    unsigned char c;

    sb_card->irq_counter++;

    /* ack soundblaster DSP if DSP was the cause of the interrupt */
    /* NTS: Experience says if you ack the wrong event on DSP 4.xx it
       will just re-fire the IRQ until you ack it correctly...
       or until your program crashes from stack overflow, whichever
       comes first */
    c = sndsb_interrupt_reason(sb_card);
    sndsb_interrupt_ack(sb_card,c);

    /* FIXME: The sndsb library should NOT do anything in
       send_buffer_again() if it knows playback has not started! */
    /* for non-auto-init modes, start another buffer */
    if (wav_state.playing) sndsb_irq_continue(sb_card,c);

    /* NTS: we assume that if the IRQ was masked when we took it, that we must not
     *      chain to the previous IRQ handler. This is very important considering
     *      that on most DOS systems an IRQ is masked for a very good reason---the
     *      interrupt handler doesn't exist! In fact, the IRQ vector could easily
     *      be unitialized or 0000:0000 for it! CALLing to that address is obviously
     *      not advised! */
    if (old_irq_masked || old_irq == NULL) {
        /* ack the interrupt ourself, do not chain */
        if (sb_card->irq >= 8) p8259_OCW2(8,P8259_OCW2_NON_SPECIFIC_EOI);
        p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
    }
    else {
        /* chain to the previous IRQ, who will acknowledge the interrupt */
        old_irq();
    }
}

void update_play_position();
void update_wav_play_delay();

/* detect playback underruns, and force write pointer forward to compensate if so.
 * caller will direct us how many bytes into the future to set the write pointer,
 * since nobody can INSTANTLY write audio to the playback pointer.
 *
 * This way, the playback program can use this to force the write pointer forward
 * if underrun to at least ensure the next audio written will be immediately audible.
 * Without this, upon underrun, the written audio may not be heard until the play
 * pointer has gone through the entire buffer again. */
int clamp_if_behind(uint32_t ahead_in_bytes) {
    int res = 0;

    update_play_position();
    update_wav_play_delay();

    if (wav_state.play_counter < wav_state.play_counter_prev) {
        sb_card->buffer_last_io  = wav_state.dma_position;
        sb_card->buffer_last_io += ahead_in_bytes;
        sb_card->buffer_last_io -= sb_card->buffer_last_io % play_codec.bytes_per_block;
        if (sb_card->buffer_last_io >= sb_card->buffer_size)
            sb_card->buffer_last_io -= sb_card->buffer_size;
        if (sb_card->buffer_last_io >= sb_card->buffer_size)
            sb_card->buffer_last_io  = 0;

        res = 1;
        update_play_position();
        update_wav_play_delay();
    }

    return res;
}

/* this depends on keeping the "play delay" up to date */
uint32_t can_write(void) { /* in bytes */
    uint32_t x;

    update_wav_dma_position();
    update_wav_play_delay();

    x = sb_card->buffer_size;
    if (x >= wav_state.play_delay_bytes) x -= wav_state.play_delay_bytes;
    else x = 0;

    return x;
}

unsigned char dosamp_FAR * mmap_write(uint32_t * const howmuch,uint32_t want) {
    uint32_t ret_pos;
    uint32_t m;

    /* simpify I/O by only allowing a write that does not wrap around */

    /* determine how much can be written */
    if (sb_card->buffer_last_io > sb_card->buffer_size) return NULL;

    m = sb_card->buffer_size - sb_card->buffer_last_io;
    if (want > m) want = m;

    /* but, must be aligned to the block alignment of the format */
    want -= want % play_codec.bytes_per_block;

    /* this is what we will return */
    *howmuch = want;
    ret_pos = sb_card->buffer_last_io;

    if (want != 0) {
        /* advance I/O. caller MUST fill in the buffer. */
        wav_state.write_counter += want;
        sb_card->buffer_last_io += want;
        if (sb_card->buffer_last_io >= sb_card->buffer_size)
            sb_card->buffer_last_io = 0;

        /* now that audio has been written, buffer is no longer empty */
        wav_state.play_empty = 0;

        /* update */
        update_wav_dma_position();
        update_wav_play_delay();
    }

    /* return ptr */
    return dosamp_ptr_add_normalize(sb_card->buffer_lin,ret_pos);
}

/* non-mmap write (much like OSS or ALSA in Linux where you do not have direct access to the hardware buffer) */
unsigned int buffer_write(const unsigned char dosamp_FAR * buf,unsigned int len) {
    unsigned char dosamp_FAR * dst;
    unsigned int r = 0;
    uint32_t todo;

    while (len > 0) {
        dst = mmap_write(&todo,(uint32_t)len);
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
        r->event_at = wav_state.write_counter;
        r->wav_position = wav_position;
    }
}

void card_poll(void) {
    sndsb_main_idle(sb_card);
    update_wav_dma_position();
    update_wav_play_delay();
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

    avail = can_write();

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

            if (buffer_write(dosamp_ptr_add_normalize(convert_rdbuf.buffer,convert_rdbuf.pos),dop) != dop)
                break;

            convert_rdbuf.pos += dop;
            assert(convert_rdbuf.pos <= convert_rdbuf.len);
            howmuch -= dop;
            avail -= dop;
        }
        else {
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

            assert(convert_rdbuf.pos <= convert_rdbuf.len);

            if (dop != 0) {
                dop *= play_codec.bytes_per_block;
                if (buffer_write(ptr,dop) != dop)
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
        clamp_if_behind(wav_play_min_load_size);
}

static void load_audio_copy(uint32_t howmuch/*in bytes*/) { /* load audio up to point or max */
    unsigned char dosamp_FAR * ptr;
    dosamp_file_off_t rem;
    uint32_t towrite;
    uint32_t avail;

    avail = can_write();

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
            ptr = mmap_write(&towrite,rem);
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
            if (buffer_write(ptr,towrite) != towrite)
                break;
        }

        /* adjust */
        wav_file_pointer_to_position();
        howmuch -= towrite;
    }

    if (!prefer_no_clamp)
        clamp_if_behind(wav_play_min_load_size);
}

static void load_audio(uint32_t howmuch/*in bytes*/) { /* load audio up to point or max */
    if (resample_on)
        load_audio_convert(howmuch);
    else
        load_audio_copy(howmuch);
}

void update_wav_play_delay() {
    signed long delay;

    /* DMA trails our "last IO" pointer */
    delay  = (signed long)sb_card->buffer_last_io;
    delay -= (signed long)wav_state.dma_position;

    /* delay == 0 is a special case.
     * if wav_state.play_empty, then it means there's no delay.
     * else, it means there's one whole buffer's worth delay.
     * we HAVE to make this distinction because this code is
     * written to load new audio data RIGHT BEHIND the DMA position
     * which could easily lead to buffer_last_io == DMA position! */
    if (delay < 0L) delay += (signed long)sb_card->buffer_size;
    else if (delay == 0L && !wav_state.play_empty) delay = (signed long)sb_card->buffer_size;

    /* guard against inconcievable cases */
    if (delay < 0L) delay = 0L;
    if (delay >= (signed long)sb_card->buffer_size) delay = (signed long)sb_card->buffer_size;

    /* convert to samples */
    wav_state.play_delay_bytes = (unsigned long)delay;
    wav_state.play_delay = ((unsigned long)delay / play_codec.bytes_per_block) * play_codec.samples_per_block;

    /* play position is calculated here */
    wav_state.play_counter = wav_state.write_counter;
    if (wav_state.play_counter >= wav_state.play_delay_bytes) wav_state.play_counter -= wav_state.play_delay_bytes;
    else wav_state.play_counter = 0;

    if (wav_state.play_counter_prev < wav_state.play_counter)
        wav_state.play_counter_prev = wav_state.play_counter;
}

void update_play_position(void) {
    struct audio_playback_rebase_t *r;

    r = rebase_find(wav_state.play_counter);

    if (r != NULL) {
        wav_play_position =
            ((unsigned long long)(((wav_state.play_counter - r->event_at) / play_codec.bytes_per_block) *
                (unsigned long long)resample_state.step) >> (unsigned long long)resample_100_shift) + r->wav_position;
    }
    else if (wav_state.playing)
        wav_play_position = 0;
    else
        wav_play_position = wav_position;
}

void clear_remapping(void) {
    wav_rebase_clear();
}

void move_pos(signed long adj) {
    if (wav_state.playing)
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
    if (!wav_state.playing || wav_source == NULL)
        return;

    /* update card state */
    card_poll();

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

int soundcard_assign_isa_dma_buffer(struct dma_8237_allocation *dma) {
    /* NTS: We WANT to call sndsb_assign_dma_buffer with sb_dma == NULL if it happens because it tells the Sound Blaster library to cancel it's copy as well */
    if (!sndsb_assign_dma_buffer(sb_card,dma))
        return -1;

    /* we want the DMA buffer region actually used by the card to be a multiple of (2 x the block size we play audio with).
     * we can let that be less than the actual DMA buffer by some bytes, it's fine. */
    {
        uint32_t adj = 2UL * (unsigned long)play_codec.bytes_per_block;

        sb_card->buffer_size -= sb_card->buffer_size % adj;
        if (sb_card->buffer_size == 0UL) return -1;
    }

    return 0;
}

int8_t soundcard_will_use_isa_dma_channel(void) {
    return sndsb_dsp_playback_will_use_dma_channel(sb_card,play_codec.sample_rate,/*stereo*/play_codec.number_of_channels > 1,/*16-bit*/play_codec.bits_per_sample > 8);
}

static void free_dma_buffer() {
    if (isa_dma != NULL) {
        soundcard_assign_isa_dma_buffer(NULL); /* disassociate DMA buffer from sound card */
        dma_8237_free_buffer(isa_dma);
        isa_dma = NULL;
    }
}

static int alloc_dma_buffer(uint32_t choice,int8_t ch) {
    if (ch < 0)
        return 0;
    if (isa_dma != NULL)
        return 0;

    do {
        if (ch >= 4)
            isa_dma = dma_8237_alloc_buffer_dw(choice,16);
        else
            isa_dma = dma_8237_alloc_buffer_dw(choice,8);

        if (isa_dma == NULL) choice -= 4096UL;
    } while (isa_dma == NULL && choice >= 4096UL);

    if (isa_dma == NULL)
        return -1;

    return 0;
}

uint32_t soundcard_recommended_isa_dma_buffer_size(uint32_t limit/*no limit == 0*/) {
    if (soundcard_will_use_isa_dma_channel() >= 4)
        return sndsb_recommended_16bit_dma_buffer_size(sb_card,limit);
    else
        return sndsb_recommended_dma_buffer_size(sb_card,limit);
}

static int realloc_dma_buffer() {
    uint32_t choice;
    int8_t ch;

    free_dma_buffer();

    ch = soundcard_will_use_isa_dma_channel();
    if (ch < 0)
        return 0; /* nothing to do */

    choice = soundcard_recommended_isa_dma_buffer_size(/*no limit*/0);
    if (choice == 0)
        return -1;

    if (alloc_dma_buffer(choice,ch) < 0)
        return -1;

    if (soundcard_assign_isa_dma_buffer(isa_dma) < 0) {
        free_dma_buffer();
        return -1;
    }

    return 0;
}

void hook_irq(void) {
    if (sb_card->irq != -1) {
        if (old_irq == NULL) {
            old_irq_masked = p8259_is_masked(sb_card->irq);
            if (vector_is_iret(irq2int(sb_card->irq)))
                old_irq_masked = 1;

            old_irq = _dos_getvect(irq2int(sb_card->irq));
            _dos_setvect(irq2int(sb_card->irq),sb_irq);
            /* if the IRQ is still masked, keep it that way until we begin playback */
        }
    }
}

void unhook_irq(void) {
    if (old_irq != NULL) {
        if (sb_card->irq >= 0 && old_irq_masked)
            p8259_mask(sb_card->irq);

        if (sb_card->irq != -1 && old_irq != NULL)
            _dos_setvect(irq2int(sb_card->irq),old_irq);

        old_irq = NULL;
    }
}

int check_dma_buffer(void) {
    int8_t ch;

    /* alloc DMA buffer.
     * if already allocated, then realloc if changing from 8-bit to 16-bit DMA */
    if (isa_dma == NULL)
        realloc_dma_buffer();
    else {
        ch = soundcard_will_use_isa_dma_channel();
        if (ch >= 0 && isa_dma->dma_width != (ch >= 4 ? 16 : 8))
            realloc_dma_buffer();
    }

    if (isa_dma == NULL)
        return -1;

    return 0;
}

int set_play_format(struct wav_cbr_t * const d,const struct wav_cbr_t * const s) {
    uint32_t osz,oph;
    int r;

    /* API check: d != s */
    if (d == s) return -1;

    /* by default, use source format */
    *d = *s;

    /* allow overwrite */
    if (prefer_rate != 0) d->sample_rate = prefer_rate;
    if (prefer_bits != 0) d->bits_per_sample = prefer_bits;
    if (prefer_channels != 0) d->number_of_channels = prefer_channels;

    /* TODO: Later we should support 5.1 surround to mono/stereo conversion, 24-bit PCM support, etc. */

    /* stereo -> mono conversion if needed (if DSP doesn't support stereo) */
    if (d->number_of_channels == 2 && sb_card->dsp_vmaj < 3) d->number_of_channels = 1;

    /* 16-bit -> 8-bit if needed (if DSP doesn't support 16-bit) */
    if (d->bits_per_sample == 16) {
        if (sb_card->is_gallant_sc6600 && sb_card->dsp_vmaj >= 3) /* SC400 and DSP version 3.xx or higher: OK */
            { }
        else if (sb_card->ess_extensions && sb_card->dsp_vmaj >= 2) /* ESS688 and DSP version 2.xx or higher: OK */
            { }
        else if (sb_card->dsp_vmaj >= 4) /* DSP version 4.xx or higher (SB16): OK */
            { }
        else
            d->bits_per_sample = 8;
    }

    if (d->number_of_channels < 1 || d->number_of_channels > 2) /* SB is mono or stereo, nothing else. */
        return -1;
    if (d->bits_per_sample < 8 || d->bits_per_sample > 16) /* SB is 8-bit. SB16 and ESS688 is 16-bit. */
        return -1;

    /* HACK! */
    osz = sb_card->buffer_size; sb_card->buffer_size = 1;
    oph = sb_card->buffer_phys; sb_card->buffer_phys = 0;

    /* SB specific: I know from experience and calculations that Sound Blaster cards don't go below 4000Hz */
    if (d->sample_rate < 4000)
        d->sample_rate = 4000;

    /* we'll follow the recommendations on what is supported by the DSP. no hacks. */
    r = sndsb_dsp_out_method_supported(sb_card,d->sample_rate,/*stereo*/d->number_of_channels > 1 ? 1 : 0,/*16-bit*/d->bits_per_sample > 8 ? 1 : 0);
    if (!r) {
        /* we already made concessions for channels/bits, so try sample rate */
        d->sample_rate = 44100;
        r = sndsb_dsp_out_method_supported(sb_card,d->sample_rate,/*stereo*/d->number_of_channels > 1 ? 1 : 0,/*16-bit*/d->bits_per_sample > 8 ? 1 : 0);
    }
    if (!r) {
        /* we already made concessions for channels/bits, so try sample rate */
        d->sample_rate = 22050;
        r = sndsb_dsp_out_method_supported(sb_card,d->sample_rate,/*stereo*/d->number_of_channels > 1 ? 1 : 0,/*16-bit*/d->bits_per_sample > 8 ? 1 : 0);
    }
    if (!r) {
        /* we already made concessions for channels/bits, so try sample rate */
        d->sample_rate = 11025;
        r = sndsb_dsp_out_method_supported(sb_card,d->sample_rate,/*stereo*/d->number_of_channels > 1 ? 1 : 0,/*16-bit*/d->bits_per_sample > 8 ? 1 : 0);
    }

    /* HACK! */
    sb_card->buffer_size = osz;
    sb_card->buffer_phys = oph;

    if (!r) {
        if (sb_card->reason_not_supported != NULL && *(sb_card->reason_not_supported) != 0)
            printf("Negotiation failed (SB) even with %luHz %u-channel %u-bit:\n    %s\n",
                d->sample_rate,
                d->number_of_channels,
                d->bits_per_sample,
                sb_card->reason_not_supported);

        return -1;
    }

    /* PCM recalc */
    d->bytes_per_block = ((d->bits_per_sample+7U)/8U) * d->number_of_channels;

    return 0;
}

void disable_autoinit(void) {
    sb_card->dsp_autoinit_dma = 0;
    sb_card->dsp_autoinit_command = 0;
}

void silence_buffer(void) {
    if (sb_card->buffer_lin != NULL) {
#if TARGET_MSDOS == 16
        _fmemset(sb_card->buffer_lin,play_codec.bits_per_sample == 8 ? 128 : 0,sb_card->buffer_size);
#else
        memset(sb_card->buffer_lin,play_codec.bits_per_sample == 8 ? 128 : 0,sb_card->buffer_size);
#endif
    }
}

int prepare_buffer(void) {
    if (check_dma_buffer() < 0)
        return -1;

    return 0;
}

int prepare_play(void) {
    if (wav_state.prepared)
        return 0;

    if (sb_card->dsp_play_method == SNDSB_DSPOUTMETHOD_DIRECT)
        return -1;

    hook_irq();

    /* prepare DSP */
    if (!sndsb_prepare_dsp_playback(sb_card,/*rate*/play_codec.sample_rate,/*stereo*/play_codec.number_of_channels > 1,/*16-bit*/play_codec.bits_per_sample > 8)) {
        unhook_irq();
        return -1;
    }

    sndsb_setup_dma(sb_card);
    update_wav_dma_position();
    update_wav_play_delay();
    wav_state.prepared = 1;
    return 0;
}

int unprepare_play(void) {
    if (wav_state.prepared) {
        sndsb_stop_dsp_playback(sb_card);
        wav_state.prepared = 0;
    }

    unhook_irq();
    return 0;
}

uint32_t play_buffer_size(void) {
    return sb_card->buffer_size;
}

uint32_t set_irq_interval(uint32_t x) {
    uint32_t t;

    /* you cannot set IRQ interval once prepared */
    if (wav_state.prepared) return sb_card->buffer_irq_interval;

    /* keep it sample aligned */
    x -= x % play_codec.bytes_per_block;

    if (x != 0UL) {
        /* minimum */
        t = ((play_codec.sample_rate + 127UL) / 128UL) * play_codec.bytes_per_block;
        if (x < t) x = t;
        if (x > sb_card->buffer_size) x = sb_card->buffer_size;
    }
    else {
        x = sb_card->buffer_size;
    }

    sb_card->buffer_irq_interval = x;
    return sb_card->buffer_irq_interval;
}

int start_sound_card(void) {
    /* make sure the IRQ is acked */
    if (sb_card->irq >= 8) {
        p8259_OCW2(8,P8259_OCW2_SPECIFIC_EOI | (sb_card->irq & 7)); /* IRQ */
        p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | 2); /* IRQ cascade */
    }
    else if (sb_card->irq >= 0) {
        p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | sb_card->irq); /* IRQ */
    }

    /* unmask the IRQ, prepare */
    if (sb_card->irq >= 0)
        p8259_unmask(sb_card->irq);

    if (!sndsb_begin_dsp_playback(sb_card)) {
        unhook_irq();
        return -1;
    }

    return 0;
}

int stop_sound_card(void) {
    _cli();
    update_wav_dma_position();
    update_wav_play_delay();
    _sti();

    sndsb_stop_dsp_playback(sb_card);
    return 0;
}

int resampler_init(struct resampler_state_t *r,struct wav_cbr_t * const d,const struct wav_cbr_t * const s) {
    if (d->sample_rate == 0 || s->sample_rate == 0)
        return -1;

    if (d->number_of_channels == 0 || s->number_of_channels == 0)
        return -1;
    if (d->number_of_channels > resample_max_channels || s->number_of_channels > resample_max_channels)
        return -1;

    {
        /* NTS: Always compute with uint64_t even on 16-bit builds.
         *      This gives us the precision we need to compute the resample step value.
         *      Also remember that on 16-bit builds the resample intermediate type is
         *      32 bits wide, and the shift is 16. As signed integers, that means that
         *      this code will produce the wrong value when sample rates >= 32768Hz are
         *      involved unless we do the math in 64-bit wide integers. */
        uint64_t tmp;

        tmp  = (uint64_t)s->sample_rate << (uint64_t)resample_100_shift;
        tmp /= (uint64_t)d->sample_rate;

        r->step = (resample_intermediate_t)tmp;
        r->frac = 0;
    }

    if (r->step == resample_100 && d->number_of_channels == s->number_of_channels && d->bits_per_sample == s->bits_per_sample)
        resample_on = 0;
    else
        resample_on = 1;

    return 0;
}

static int begin_play() {
    if (wav_state.playing)
        return 0;

    if (wav_source == NULL)
        return -1;

    /* reset state */
    resampler_state_reset(&resample_state);
    convert_rdbuf_clear();
    wav_rebase_clear();
    wav_state.play_counter_prev = 0;
    wav_state.write_counter = 0;
    wav_state.play_counter = 0;
    wav_state.play_delay = 0;
    wav_state.play_empty = 1;

    /* get the file pointer ready */
    wav_position_to_file_pointer();

    /* make a rebase event */
    wav_rebase_position_event();

    /* choose output vs input */
    if (set_play_format(&play_codec,&file_codec) < 0) {
        unprepare_play();
        return -1;
    }

    /* based on sound card's choice vs source format, reconfigure resampler */
    if (resampler_init(&resample_state,&play_codec,&file_codec) < 0) {
        unprepare_play();
        return -1;
    }

    /* prepare buffer */
    if (prepare_buffer() < 0) {
        unprepare_play();
        return -1;
    }

    /* zero the buffer */
    silence_buffer();

    /* set IRQ interval (card will pick closest and sanitize it) */
    set_irq_interval(play_buffer_size());

    /* minimum buffer until loading again (100ms) */
    wav_play_min_load_size = (play_codec.sample_rate / 10 / play_codec.samples_per_block) * play_codec.bytes_per_block;

    /* no more than 1/4 the buffer */
    if (wav_play_min_load_size > (sb_card->buffer_size / 4UL))
        wav_play_min_load_size = (sb_card->buffer_size / 4UL);

    /* how much to load per call (100ms per call) */
    wav_play_load_block_size = (play_codec.sample_rate / 10 / play_codec.samples_per_block) * play_codec.bytes_per_block;

    /* no more than half the buffer */
    if (wav_play_load_block_size > (sb_card->buffer_size / 2UL))
        wav_play_load_block_size = (sb_card->buffer_size / 2UL);

    /* prepare the sound card (buffer, DMA, etc.) */
    if (prepare_play() < 0) {
        unprepare_play();
        return -1;
    }

    /* preroll */
    load_audio(wav_play_load_block_size);
    update_play_position();

    /* go! */
    if (start_sound_card() < 0) {
        unprepare_play();
        return -1;
    }

    _cli();
    wav_state.playing = 1;
    _sti();

    return 0;
}

static void stop_play() {
    if (!wav_state.playing) return;

    /* stop */
    stop_sound_card();
    unprepare_play();

    wav_position = wav_play_position;
    update_play_position();

    _cli();
    wav_state.playing = 0;
    _sti();
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
    signed long pos = (signed long)sndsb_read_dma_buffer_position(sb_card);
    signed long apos = (signed long)sb_card->buffer_last_io;

    printf("\x0D");

    printf("a=%6ld/p=%6ld/b=%6ld/d=%6lu/cw=%6lu/wp=%8lu/pp=%8lu/irq=%lu",
        apos,
        pos,
        (signed long)sb_card->buffer_size,
        (unsigned long)wav_state.play_delay_bytes,
        (unsigned long)can_write(),
        (unsigned long)wav_state.write_counter,
        (unsigned long)wav_state.play_counter,
        (unsigned long)sb_card->irq_counter);

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
    free_dma_buffer();
    sndsb_free_card(sb_card);
    free_sndsb(); /* will also de-ref/unhook the NMI reflection */
}

int probe_for_sound_blaster(void) {
    unsigned int i;

    if (!init_sndsb()) {
        printf("Cannot init library\n");
        return -1;
    }

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

    /* OK. done */
    return 0;
}

int main(int argc,char **argv) {
    unsigned char disp=1;
    int i,loop;

    if (!parse_argv(argc,argv))
        return 1;

    wav_state_init();

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

    /* PROBE: Sound Blaster.
     * Will return 0 if scan done, -1 if a serious problem happened.
     * A return value of 0 doesn't mean any cards were found. */
    if (probe_for_sound_blaster() < 0) {
        printf("Serious error while probing for Sound Blaster\n");
        return 1;
    }

    /* now let the user choose.
     * TODO: At some point, when we have abstracted ourself away from the Sound Blaster library
     *       we can list a different array of abstract sound card device objects. */
    {
        unsigned char count = 0;
        int sc_idx = -1;

        for (i=0;i < SNDSB_MAX_CARDS;i++) {
            struct sndsb_ctx *cx = sndsb_index_to_ctx(i);
            if (cx->baseio == 0) continue;

            printf("  [%u] base=%X dma=%d dma16=%d irq=%d DSPv=%u.%u\n",
                    i+1,cx->baseio,cx->dma8,cx->dma16,cx->irq,(unsigned int)cx->dsp_vmaj,(unsigned int)cx->dsp_vmin);

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
            sc_idx = i - '0';

            if (sc_idx < 1 || sc_idx > SNDSB_MAX_CARDS) {
                printf("Sound card index out of range\n");
                return 1;
            }
        }
        else { /* count == 1 */
            sc_idx = 1;
        }

        sb_card = &sndsb_card[sc_idx-1];
        if (sb_card->baseio == 0)
            return 1;
    }

    loop = 1;
    if (open_wav() < 0)
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
                    unsigned char wp = wav_state.playing;

                    if (wp) stop_play();
                    disable_autoinit();
                    printf("Disabled auto-init\n");
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
                    unsigned char wp = wav_state.playing;

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
                    unsigned char wp = wav_state.playing;

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
                    unsigned char wp = wav_state.playing;

                    if (wp) stop_play();

                    if (prefer_bits < 8)
                        prefer_bits = 8;
                    else if (prefer_bits < 16)
                        prefer_bits = 16;
                    else
                        prefer_bits = 0;

                    if (wp) begin_play();
                }
                else if (i == '[') {
                    unsigned char wp = wav_state.playing;

                    if (wp) stop_play();
                    move_pos(-((signed long)file_codec.sample_rate)); /* -1 sec */
                    if (wp) begin_play();
                }
                else if (i == ']') {
                    unsigned char wp = wav_state.playing;

                    if (wp) stop_play();
                    move_pos(file_codec.sample_rate); /* 1 sec */
                    if (wp) begin_play();
                }
                else if (i == '{') {
                    unsigned char wp = wav_state.playing;

                    if (wp) stop_play();
                    move_pos(-((signed long)file_codec.sample_rate * 30L)); /* -30 sec */
                    if (wp) begin_play();
                }
                else if (i == '}') {
                    unsigned char wp = wav_state.playing;

                    if (wp) stop_play();
                    move_pos(file_codec.sample_rate * 30L); /* 30 sec */
                    if (wp) begin_play();
                }
                else if (i == ' ') {
                    if (wav_state.playing) stop_play();
                    else begin_play();
                }
            }
        }
    }

    _sti();
    stop_play();
    close_wav();
    tmpbuffer_free();
    convert_rdbuf_free();

    free_sound_blaster_support();

    time_source->close(time_source);
    return 0;
}

