
#if defined(TARGET_WINDOWS)
# define WINFCON_STOCK_WIN_MAIN
#endif

#if defined(TARGET_WINDOWS)
# define HW_DOS_DONT_DEFINE_MMSYSTEM
# include <windows.h>
# include <commdlg.h>
# include <mmsystem.h>
#endif

#include <stdio.h>
#include <stdint.h>
#ifdef LINUX
#include <signal.h>
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
#include "termios.h"

#include "sc_sb.h"
#include "sc_oss.h"
#include "sc_alsa.h"
#include "sc_winmm.h"

#if defined(TARGET_WINDOWS)
#include <signal.h>
#endif

/* this code won't work with the TINY memory model for awhile. sorry. */
#ifdef __TINY__
# error Open Watcom C tiny memory model not supported
#endif

/* file source */
dosamp_file_source_t dosamp_file_source_file_fd_open(const char * const path);

/* tool */
char                                            str_tmp[256];
char                                            soundcard_str_tmp[256];

/* DOSAMP state and user state */
static volatile unsigned char                   exit_now = 0;
static unsigned char                            test_mode = 0;
static unsigned long                            prefer_rate = 0;
static unsigned char                            prefer_channels = 0;
static unsigned char                            prefer_bits = 0;
static unsigned char                            prefer_no_clamp = 0;
static signed char                              opt_round = -1;

/* DOSAMP debug state */
static char                                     stuck_test = 0;
static unsigned char                            use_mmap_write = 1;

/* chosen time source.
 * NTS: Don't forget that by design, some time sources (8254 for example)
 *      require you to poll often enough to get a correct count of time. */
static dosamp_time_source_t                     time_source = NULL;

#if defined(HAS_IRQ)
/* ISA DMA buffer */
static struct dma_8237_allocation*              isa_dma = NULL;
#endif

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
static unsigned long                    wav_data_length = 0;/* in samples */

/* WAV playback state */
static unsigned long                    wav_position = 0;/* in samples. read pointer. after reading, points to next sample to read. */
static unsigned long                    wav_play_position = 0L;

/* buffering threshholds */
static unsigned long                    wav_play_load_block_size = 0;/*max load per call*/
static unsigned long                    wav_play_min_load_size = 0;/*minimum "can write" threshhold to load more*/

#if defined(HAS_IRQ)

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

#endif

int wav_rewind(void) {
    wav_position = 0;
    if (wav_source->seek(wav_source,(dosamp_file_off_t)wav_data_offset) != (dosamp_file_off_t)wav_data_offset) return -1;
    return 0;
}

int wav_file_pointer_to_position(void) {
    if ((uint64_t)wav_source->file_pos >= (uint64_t)wav_data_offset) {
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
            if ((uint64_t)wav_source->file_pos <= (uint64_t)rem)
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
        if ((uint64_t)wav_source->file_pos <= (uint64_t)rem)
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

#if defined(HAS_DMA)
static void free_dma_buffer() {
    if (isa_dma != NULL) {
        soundcard->ioctl(soundcard,soundcard_ioctl_isa_dma_assign_buffer,NULL,NULL,0); /* disassociate DMA buffer from sound card */
        dma_8237_free_buffer(isa_dma);
        isa_dma = NULL;
    }
}
#endif

#if defined(HAS_DMA)
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
                    return -1;
            }
        } while (1);
    }

    /* assume isa_dma != NULL */
    return 0;
}
#endif

#if defined(HAS_DMA)
static uint32_t soundcard_isa_dma_recommended_buffer_size(soundcard_t sc,uint32_t limit) {
    unsigned int sz = sizeof(uint32_t);
    uint32_t p = 0;

    if (soundcard->ioctl(soundcard,soundcard_ioctl_isa_dma_recommended_buffer_size,&p,&sz,0) < 0)
        return 0;

    return p;
}
#endif

#if defined(HAS_DMA)
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
#endif

#if defined(HAS_DMA)
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
#endif

int prepare_buffer(void) {
#if defined(HAS_DMA)
    if (check_dma_buffer() < 0)
        return -1;
#endif

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

#if defined(HAS_IRQ)
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
#endif

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
#if defined(HAS_IRQ)
    unhook_irq();
#endif
    return -1;
}

static void stop_play() {
    if (!soundcard->wav_state.playing) return;

    /* stop */
    soundcard->ioctl(soundcard,soundcard_ioctl_stop_play,NULL,NULL,0);
    soundcard->ioctl(soundcard,soundcard_ioctl_unprepare_play,NULL,NULL,0);
#if defined(HAS_IRQ)
    unhook_irq();
#endif

    wav_position = wav_play_position;
    update_play_position();
}

static void help() {
    printf("dosamp [options] <file>\n");
    printf(" /h /help             This help\n");
}

#if defined(TARGET_WINDOWS)
/* dynamic loading of MMSYSTEM/WINMM */
HMODULE             mmsystem_dll = NULL;
unsigned char       mmsystem_tried = 0;

void free_mmsystem(void) {
    if (mmsystem_dll != NULL) {
        FreeLibrary(mmsystem_dll);
        mmsystem_dll = NULL;
    }

    mmsystem_tried = 0;
}

void mmsystem_atexit(void) {
    free_mmsystem();
}

int init_mmsystem(void) {
    if (mmsystem_dll == NULL) {
        if (!mmsystem_tried) {
            UINT oldMode;

            oldMode = SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
#if TARGET_MSDOS == 16
            mmsystem_dll = LoadLibrary("MMSYSTEM");
#else
            mmsystem_dll = LoadLibrary("WINMM.DLL");
#endif
            SetErrorMode(oldMode);

            if (mmsystem_dll != NULL)
                atexit(mmsystem_atexit);

            mmsystem_tried = 1;
        }
    }

    return (mmsystem_dll != NULL) ? 1 : 0;
}

int check_mmsystem_time(void) {
    if (!init_mmsystem())
        return 0;

    if (GetProcAddress(mmsystem_dll,"TIMEGETTIME") == NULL)
        return 0;
    if (GetProcAddress(mmsystem_dll,"TIMEBEGINPERIOD") == NULL)
        return 0;
    if (GetProcAddress(mmsystem_dll,"TIMEENDPERIOD") == NULL)
        return 0;

    return 1;
}
#endif

#if defined(TARGET_WINDOWS)
/* dynamic loading of COMMDLG */
HMODULE             commdlg_dll = NULL;
unsigned char       commdlg_tried = 0;

typedef BOOL (WINAPI *GETOPENFILENAMEPROC)(OPENFILENAME FAR *);

GETOPENFILENAMEPROC commdlg_getopenfilenameproc(void) {
#if TARGET_MSDOS == 16
    if (commdlg_dll != NULL)
        return (GETOPENFILENAMEPROC)GetProcAddress(commdlg_dll,"GETOPENFILENAME");
#else
    if (commdlg_dll != NULL)
        return (GETOPENFILENAMEPROC)GetProcAddress(commdlg_dll,"GetOpenFileNameA");
#endif

    return NULL;
}

void free_commdlg(void) {
    if (commdlg_dll != NULL) {
        FreeLibrary(commdlg_dll);
        commdlg_dll = NULL;
    }

    commdlg_tried = 0;
}

void commdlg_atexit(void) {
    free_commdlg();
}

int init_commdlg(void) {
    if (commdlg_dll == NULL) {
        if (!commdlg_tried) {
            UINT oldMode;

            oldMode = SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
#if TARGET_MSDOS == 16
            commdlg_dll = LoadLibrary("COMMDLG");
#else
            commdlg_dll = LoadLibrary("COMDLG32.DLL");
#endif
            SetErrorMode(oldMode);

            if (commdlg_dll != NULL)
                atexit(commdlg_atexit);

            commdlg_tried = 1;
        }
    }

    return (commdlg_dll != NULL) ? 1 : 0;
}
#endif

#if defined(TARGET_WINDOWS)
char *prompt_open_file_windows_gofn(void) {
    GETOPENFILENAMEPROC gofn;

    /* GetOpenFileName() did not appear until Windows 3.1 */
    if (!init_commdlg())
        return NULL;
    if ((gofn=commdlg_getopenfilenameproc()) == NULL)
        return NULL;

    {
        char tmp[300];
        OPENFILENAME of;

        memset(&of,0,sizeof(of));
        memset(tmp,0,sizeof(tmp));

        of.lStructSize = sizeof(of);
        of.hwndOwner = _win_hwnd();
        of.hInstance = _win_hInstance;
        of.lpstrFilter =
            "All supported files\x00*.wav\x00"
            "WAV files\x00*.wav\x00"
            "All files\x00*.*\x00";
        of.nFilterIndex = 1;
        if (wav_file != NULL) strncpy(tmp,wav_file,sizeof(tmp)-1);
        of.lpstrFile = tmp;
        of.nMaxFile = sizeof(tmp)-1;
        of.lpstrTitle = "Select file to play";
        of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (!gofn(&of))
            return NULL;

        return strdup(tmp);
    }
}
#endif

char *prompt_open_file(void) {
    char *ret = NULL;

#if defined(TARGET_WINDOWS)
    if ((ret=prompt_open_file_windows_gofn()) != NULL)
        return ret;
#endif

    return ret;
}

static int parse_argv(int argc,char **argv) {
    int i;

    for (i=1;i < argc;) {
        char *a = argv[i++];

        if (*a == '-') {
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
            else if (!strcmp(a,"nrnd")) {
                opt_round = 0;
            }
            else if (!strcmp(a,"rnd")) {
                opt_round = 1;
            }
            else if (!strcmp(a,"tst")) {
                test_mode = TEST_TSC;
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

    if (test_mode == TEST_TSC) {
        /*OK*/
    }
    else {
        /* Windows: Try asking the user what to play */
        if (wav_file == NULL)
            wav_file = prompt_open_file();

        if (wav_file == NULL) {
            printf("You must specify a file to play\n");
            return 0;
        }
    }

    return 1;
}

void display_idle_timesource(void) {
    printf("\x0D");

    if (0)
        { }
#if defined(HAS_8254)
    else if (time_source == &dosamp_time_source_8254)
        printf("8254-PIT ");
#endif
#if defined(HAS_RDTSC)
    else if (time_source == &dosamp_time_source_rdtsc)
        printf("RDTSC ");
#endif
#if defined(HAS_CLOCK_MONOTONIC)
    else if (time_source == &dosamp_time_source_clock_monotonic)
        printf("CLOCK_MONOTONIC ");
#endif
#if defined(TARGET_WINDOWS)
    else if (time_source == &dosamp_time_source_mmsystem_time)
        printf("MMSYSTEM_TIME ");
#endif
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

void print_soundcard(soundcard_t sc) {
    unsigned int sz;

    sz = sizeof(str_tmp);
    if (sc->ioctl(sc,soundcard_ioctl_get_card_name,str_tmp,&sz,0) >= 0)
        printf("%s",str_tmp);
    else
        printf("(unknown card)");

    sz = sizeof(str_tmp);
    if (sc->ioctl(sc,soundcard_ioctl_get_card_detail,str_tmp,&sz,0) >= 0)
        printf(" %s",str_tmp);
}

#if defined(TARGET_WINDOWS) || defined(LINUX)
void sigint_handler(int x) {
    (void)x;

    exit_now++;
}
#endif

int main(int argc,char **argv,char **envp) {
    unsigned char disp=1;
    int i,loop;

#if defined(TARGET_WINDOWS) || defined(LINUX)
    /* winfcon will send SIGINT if the user closes the window through the system menu */
    signal(SIGINT,sigint_handler);
#endif

    (void)envp;

    if (!parse_argv(argc,argv))
        return 1;

    if (init_termios() < 0)
        return 2;

    /* default good resampler */
    /* TODO: If we detect the CPU is slow enough, default to "fast" (nearest neighbor) */
    resample_state.resample_mode = resample_good;

#if defined(LINUX)
    /* ... */
#else
    cpu_probe();
# if !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
    /* MS-DOS */
    probe_8237();
    if (!probe_8259()) {
        printf("Cannot init 8259 PIC\n");
        return 1;
    }
# endif
# if defined(HAS_8254)
    if (!probe_8254()) {
        printf("Cannot init 8254 timer\n");
        return 1;
    }
# endif
# if !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
    if (!init_isa_pnp_bios()) {
        printf("Cannot init ISA PnP\n");
        return 1;
    }
    find_isa_pnp_bios();
# endif
#endif

#if defined(TARGET_WINDOWS)
    init_mmsystem();
#endif

#if defined(HAS_8254)
    /* pick initial time source. */
    time_source = &dosamp_time_source_8254;

    /* NTS: When using the PIT source we NEED to do this, or else DOSBox-X (and possibly
     *      many other motherboards) will leave the PIT in a mode that produces the correct
     *      IRQ 0 rate but counts down twice as fast. We need the PIT to count down by 1,
     *      not by 2, in order to keep time. MS-DOS only. */
    write_8254_system_timer(0); /* 18.2 tick/sec on our terms (proper PIT mode) */
#elif defined(HAS_CLOCK_MONOTONIC)
    time_source = &dosamp_time_source_clock_monotonic;
#endif

#if defined(TARGET_WINDOWS)
    /* if we can use MMSYSTEM's timer, then please do so.
     * the 8254 fallback is for really old Windows (3.0 and earlier) that lacks the multimedia timer */
    if (check_mmsystem_time())
        time_source = &dosamp_time_source_mmsystem_time;
#endif

#if defined(HAS_RDTSC) && !defined(HAS_CLOCK_MONOTONIC)
    /* we can use the Time Stamp Counter on Pentium or higher systems that offer it */
    if (dosamp_time_source_rdtsc_available(time_source))
        time_source = &dosamp_time_source_rdtsc;
#endif

    if (time_source == NULL) {
        printf("Time source not available\n");
        return 1;
    }

    /* open the time source */
    if (time_source->open(time_source) < 0) {
        printf("Cannot open time source\n");
        return 1;
    }

    if (test_mode == TEST_TSC) {
        while (1) {
            display_idle_timesource();

            if (kbhit()) {
                if (getch() == 27)
                    break;
            }
        }

        time_source->close(time_source);
        return 1;
    }

    if (soundcardlist_init() < 0)
        return 1;

#if defined(HAS_SNDSB)
    /* PROBE: Sound Blaster.
     * Will return 0 if scan done, -1 if a serious problem happened.
     * A return value of 0 doesn't mean any cards were found. */
    if (probe_for_sound_blaster() < 0) {
        printf("Serious error while probing for Sound Blaster\n");
        return 1;
    }
#endif

#if defined(HAS_ALSA)
    /* PROBE: Sound Blaster.
     * Will return 0 if scan done, -1 if a serious problem happened.
     * A return value of 0 doesn't mean any cards were found. */
    if (probe_for_alsa() < 0) {
        printf("Serious error while probing ALSA devices\n");
        return 1;
    }
#endif

#if defined(HAS_OSS)
    if (probe_for_oss() < 0) {
        printf("Serious error while probing OSS devices\n");
        return 1;
    }
#endif

#if defined(TARGET_WINDOWS)
    if (probe_for_mmsystem() < 0) {
        printf("Serious error while probing MMSYSTEM devices\n");
        return 1;
    }
#endif

    /* now let the user choose. */
    {
        unsigned char count = 0;
        int sc_idx = -1;
        soundcard_t sc;

        for (i=0;(unsigned int)i < soundcardlist_count;i++) {
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

            for (i=0;(unsigned int)i < soundcardlist_count;i++) {
                sc = &soundcardlist[i];
                if (sc->driver == soundcard_none) continue;

                printf("%d: ",i+1);
                print_soundcard(sc);
                printf("\n");
            }

            printf("-----------\n");
            printf("Which card?: "); fflush(stdout);

            i = getch();
            printf("\n");
            if (i == 27) return 0;
            if (i == 13 || i == 10) i = '1';
            sc_idx = i - '1';

            if (sc_idx < 0 || sc_idx >= (int)soundcardlist_count) {
                printf("Sound card index out of range\n");
                return 1;
            }
        }
        else { /* count == 1 */
            sc_idx = 0;
        }

        soundcard = &soundcardlist[sc_idx];
    }

    if (!(soundcard->capabilities & soundcard_caps_mmap_write))
        use_mmap_write = 0;

    printf("Playing audio with: ");
    print_soundcard(soundcard);
    printf("\n");

    /* TODO: if the CPU is slow, and opt_round < 0 (not set) set opt_round = 0 (off).
     *       slow CPUs should be encouraged not to resample if the rate is "close enough" */

    loop = 1;
    if (soundcard->open(soundcard) < 0)
        printf("Failed to open soundcard\n");
    else {
        if (opt_round >= 0)
            soundcard->ioctl(soundcard,soundcard_ioctl_set_rate_rounding,NULL,NULL,opt_round);

        if (open_wav() < 0)
            printf("Failed to open file\n");
        else if (begin_play() < 0)
            printf("Failed to start playback\n");
        else {
            printf("Hit ESC to stop playback. Use 1-9 keys for other status.\n");

            while (loop) {
                wav_idle();
                display_idle(disp);

                if (exit_now)
                    break;

                if (kbhit()) {
                    i = getch();
                    if (i == 0) i = getch() << 8;

                    if (i == 27) {
                        loop = 0;
                        break;
                    }
                    else if (i == 'I') {
                        printf("\nIRQ status:\n");
#if defined(LINUX)
                        printf("  Not applicable for this platform\n");
#elif defined(HAS_IRQ)
                        printf("  IRQ=%u INT=0x%02X was_masked=%u chain_irq=%u was_iret=%u hooked=%u\n",
                            soundcard_irq.irq_number,
                            soundcard_irq.int_number,
                            soundcard_irq.was_masked,
                            soundcard_irq.chain_irq,
                            soundcard_irq.was_iret,
                            soundcard_irq.hooked);
#endif
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
    }

    convert_rdbuf_check();

#if defined(HAS_CLISTI)
    _sti();
#endif
    stop_play();
    close_wav();
    tmpbuffer_free();
#if defined(HAS_DMA)
    free_dma_buffer();
#endif
    convert_rdbuf_free();
    soundcard->close(soundcard);

#if defined(HAS_SNDSB)
    free_sound_blaster_support();
#endif
#if defined(HAS_ALSA)
    free_alsa_support();
#endif
#if defined(HAS_OSS)
    free_oss_support();
#endif
#if defined(TARGET_WINDOWS)
    free_mmsystem_support();
#endif

    soundcardlist_close();

    time_source->close(time_source);
    free_termios();

#if defined(LINUX)
    printf("\n");
#endif

    if (wav_file != NULL) {
        free(wav_file);
        wav_file = NULL;
    }

    return 0;
}

