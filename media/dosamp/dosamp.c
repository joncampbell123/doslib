
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
#include <hw/8237/8237.h>		/* 8237 DMA */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/cpu/cpurdtsc.h>
#include <hw/dos/doswin.h>
#include <hw/dos/tgusmega.h>
#include <hw/dos/tgussbos.h>
#include <hw/dos/tgusumid.h>
#include <hw/isapnp/isapnp.h>
#include <hw/sndsb/sndsbpnp.h>

static char                 stuck_test = 0;

#pragma pack(push,1)
typedef struct windows_WAVEFORMATPCM {
    uint16_t    wFormatTag;             /* +0 */
    uint16_t    nChannels;              /* +2 */
    uint32_t    nSamplesPerSec;         /* +4 */
    uint32_t    nAvgBytesPerSec;        /* +8 */
    uint16_t    nBlockAlign;            /* +12 */
    uint16_t    wBitsPerSample;         /* +14 */
} windows_WAVEFORMATPCM;                /* =16 */
#pragma pack(pop)

#define windows_WAVE_FORMAT_PCM         0x0001

/* this code won't work with the TINY memory model for awhile. sorry. */
#ifdef __TINY__
# error Open Watcom C tiny memory model not supported
#endif

/* making the API use FAR CALL function pointers will allow future development
 * where file access and media playback code can exist in external DLLs, whether
 * 16-bit or 32-bit code */
#if TARGET_MSDOS == 32 && !defined(WIN386)
# define dosamp_FAR
#else
# define dosamp_FAR far
#endif

enum {
    dosamp_time_source_id_null = 0,                 /* null */
    dosamp_time_source_id_8254 = 1,                 /* MS-DOS / Windows 3.x 8254 PIT timer */
    dosamp_time_source_id_rdtsc = 2                 /* Pentium RDTSC */
};

struct dosamp_time_source;
typedef struct dosamp_time_source dosamp_FAR * dosamp_time_source_t;

/* Time source.
 * NTS: For maximum portability, the caller should not expect accurate timekeeping UNLESS polling
 *      the time source on a semi-regular basis as indicated in the structure. The 8254 PIT time
 *      source especially needs this since the code does NOT hook the IRQ 0 interrupt.
 *
 *      Unlike file sources, these are statically declared and are NOT malloc'd or free'd.
 *      But you must open/close them regardless. */
struct dosamp_time_source {
    unsigned int                        obj_id;     /* what exactly this is */
    volatile unsigned int               refcount;   /* reference count. will NOT auto-free when zero. */
    unsigned int                        open_flags; /* zero if not open, nonzero if open */
    unsigned long                       clock_rate; /* tick rate of the time source */
    unsigned long long                  counter;    /* counter */
    unsigned long                       poll_requirement; /* you must poll the source this often (clock ticks) to keep time (Intel 8254 source) */
    int                                 (dosamp_FAR *close)(dosamp_time_source_t inst);
    int                                 (dosamp_FAR *open)(dosamp_time_source_t inst);
    unsigned long long                  (dosamp_FAR *poll)(dosamp_time_source_t inst);
};

/* previous count, and period (in case host changes timer) */
static uint16_t ts_8254_pcnt = 0;
static uint32_t ts_8254_pper = 0;

int dosamp_FAR dosamp_time_source_8254_close(dosamp_time_source_t inst) {
    inst->open_flags = 0;
    inst->counter = 0;
    return 0;
}

int dosamp_FAR dosamp_time_source_8254_open(dosamp_time_source_t inst) {
    inst->open_flags = (unsigned int)(-1);
    inst->clock_rate = T8254_REF_CLOCK_HZ;
    ts_8254_pcnt = read_8254(T8254_TIMER_INTERRUPT_TICK);
    ts_8254_pper = t8254_counter[T8254_TIMER_INTERRUPT_TICK];
    return 0;
}

unsigned long long dosamp_FAR dosamp_time_source_8254_poll(dosamp_time_source_t inst) {
    if (inst->open_flags == 0) return 0UL;

    if (ts_8254_pper == t8254_counter[T8254_TIMER_INTERRUPT_TICK]) {
        uint16_t ncnt = read_8254(T8254_TIMER_INTERRUPT_TICK);
        uint16_t delta;

        /* NTS: remember the timer chip counts DOWN to zero.
         *      so normally, ncnt < ts_8254_pcnt unless 16-bit rollover happened. */
        if (ncnt <= ts_8254_pcnt)
            delta = ts_8254_pcnt - ncnt;
        else
            delta = ts_8254_pcnt + (uint16_t)ts_8254_pper - ncnt;

        /* count */
        inst->counter += (unsigned long long)delta;

        /* store the prev counter */
        ts_8254_pcnt = ncnt;
    }
    else {
        ts_8254_pcnt = read_8254(T8254_TIMER_INTERRUPT_TICK);
        ts_8254_pper = t8254_counter[T8254_TIMER_INTERRUPT_TICK];
    }

    return inst->counter;
}

struct dosamp_time_source               dosamp_time_source_8254 = {
    .obj_id =                           dosamp_time_source_id_8254,
    .open_flags =                       0,
    .clock_rate =                       0,
    .counter =                          0,
    .poll_requirement =                 0xE000UL, /* counter is 16-bit */
    .close =                            dosamp_time_source_8254_close,
    .open =                             dosamp_time_source_8254_open,
    .poll =                             dosamp_time_source_8254_poll
};

static uint64_t ts_rdtsc_prev;

int dosamp_FAR dosamp_time_source_rdtsc_close(dosamp_time_source_t inst) {
    inst->open_flags = 0;
    ts_rdtsc_prev = 0;
    return 0;
}

int dosamp_FAR dosamp_time_source_rdtsc_open(dosamp_time_source_t inst) {
    inst->open_flags = (unsigned int)(-1);
    ts_rdtsc_prev = cpu_rdtsc();
    return 0;
}

unsigned long long dosamp_FAR dosamp_time_source_rdtsc_poll(dosamp_time_source_t inst) {
    uint64_t t;

    if (inst->open_flags == 0) return 0UL;

    t = cpu_rdtsc();

    inst->counter += t - ts_rdtsc_prev;

    ts_rdtsc_prev = t;

    return inst->counter;
}

struct dosamp_time_source               dosamp_time_source_rdtsc = {
    .obj_id =                           dosamp_time_source_id_rdtsc,
    .open_flags =                       0,
    .clock_rate =                       0,
    .counter =                          0,
    .poll_requirement =                 0, /* counter is 64-bit */
    .close =                            dosamp_time_source_rdtsc_close,
    .open =                             dosamp_time_source_rdtsc_open,
    .poll =                             dosamp_time_source_rdtsc_poll
};

#if TARGET_MSDOS == 32
static inline unsigned char *dosamp_ptr_add_normalize(unsigned char * const p,const unsigned int o) {
    return p + o;
}

static inline const unsigned char *dosamp_cptr_add_normalize(const unsigned char * const p,const unsigned int o) {
    return p + o;
}
#else
static inline uint32_t dosamp_ptr_to_linear(const unsigned char far * const p) {
    return ((unsigned long)FP_SEG(p) << 4UL) + (unsigned long)FP_OFF(p);
}

static inline unsigned char far *dosamp_linear_to_ptr(const uint32_t p) {
    const unsigned short s = (unsigned short)((unsigned long)p >> 4UL);
    const unsigned short o = (unsigned short)((unsigned long)p & 0xFUL);

    return (unsigned char far*)MK_FP(s,o);
}

static inline unsigned char far *dosamp_ptr_add_normalize(unsigned char far * const p,const unsigned int o) {
    return (unsigned char far*)dosamp_linear_to_ptr(dosamp_ptr_to_linear(p) + o);
}

static inline const unsigned char far *dosamp_cptr_add_normalize(const unsigned char far * const p,const unsigned int o) {
    return (const unsigned char far*)dosamp_linear_to_ptr(dosamp_ptr_to_linear(p) + o);
}
#endif

/* ISA DMA buffer */
static struct dma_8237_allocation*      sb_dma = NULL;

/* Sound Blaster sound card */
static struct sndsb_ctx*	            sb_card = NULL;

/* chosen time source.
 * NTS: Don't forget that by design, some time sources (8254 for example)
 *      require you to poll often enough to get a correct count of time. */
static dosamp_time_source_t             time_source = NULL;

#if TARGET_MSDOS == 32
static const unsigned int               dosamp_file_io_maxb = (unsigned int)INT_MAX - 1U;
static const unsigned int               dosamp_file_io_err = UINT_MAX;
#else
static const unsigned int               dosamp_file_io_maxb = UINT_MAX - 1U;
static const unsigned int               dosamp_file_io_err = UINT_MAX;
#endif

#if TARGET_MSDOS == 32
# if defined(TARGET_WINDOWS) && !defined(WIN386)
/* 32-bit Windows and later can support files >= 4GB */
typedef uint64_t                        dosamp_file_off_t;
static const dosamp_file_off_t          dosamp_file_off_max = ULLONG_MAX - 1ULL;
static const dosamp_file_off_t          dosamp_file_off_err = ULLONG_MAX;
# else
/* 32-bit MS-DOS is limited to 2GB or less (4GB if FAT32) */
typedef uint32_t                        dosamp_file_off_t;
static const dosamp_file_off_t          dosamp_file_off_max = ULONG_MAX - 1UL;
static const dosamp_file_off_t          dosamp_file_off_err = ULONG_MAX;
# endif
#else
/* 16-bit MS-DOS and Windows is limited to 2GB or less (4GB if FAT32) */
typedef uint32_t                        dosamp_file_off_t;
static const dosamp_file_off_t          dosamp_file_off_max = ULONG_MAX - 1UL;
static const dosamp_file_off_t          dosamp_file_off_err = ULONG_MAX;
#endif

enum {
    dosamp_file_source_id_null = 0,
    dosamp_file_source_id_file_fd = 1
};

/* obj_id == dosamp_file_source_id_file_fd.
 * must be sizeof() <= sizeof(private) */
typedef struct dosamp_file_source_priv_file_fd {
    int                                 fd;
};

typedef struct dosamp_file_source;
typedef struct dosamp_file_source dosamp_FAR * dosamp_file_source_t;
typedef struct dosamp_file_source dosamp_FAR * dosamp_FAR * dosamp_file_source_ptr_t;
typedef const struct dosamp_file_source dosamp_FAR * const_dosamp_file_source_t;

typedef struct dosamp_file_source {
    unsigned int                        obj_id;     /* what exactly this is */
    volatile unsigned int               refcount;   /* reference count. will NOT auto-free when zero. */
    int                                 open_flags; /* O_RDONLY, O_WRONLY, O_RDWR, etc. used to open file or 0 if closed */
    int64_t                             file_size;  /* file size in bytes (if known) or -1LL */
    int64_t                             file_pos;   /* file pointer or -1LL if not known */
    void                                (dosamp_FAR * free)(dosamp_file_source_t const inst); /* free the file source */
    int                                 (dosamp_FAR * close)(dosamp_file_source_t const inst); /* call this to close the file */
    unsigned int                        (dosamp_FAR * read)(dosamp_file_source_t const inst,void dosamp_FAR *buf,unsigned int count); /* read function */
    unsigned int                        (dosamp_FAR * write)(dosamp_file_source_t const inst,const void dosamp_FAR *buf,unsigned int count); /* write function */
    dosamp_file_off_t                   (dosamp_FAR * seek)(dosamp_file_source_t const inst,dosamp_file_off_t pos); /* seek function */
    union {
        struct dosamp_file_source_priv_file_fd      file_fd;
    } p;
};

static inline unsigned int dosamp_FAR dosamp_file_source_addref(dosamp_file_source_t const inst) {
    return ++(inst->refcount);
}

unsigned int dosamp_FAR dosamp_file_source_release(dosamp_file_source_t const inst) {
    if (inst->refcount != 0)
        inst->refcount--;
    // TODO: else, debug message if refcount == 0

    return inst->refcount;
}

unsigned int dosamp_FAR dosamp_file_source_autofree(dosamp_file_source_ptr_t inst) {
    unsigned int r = 0;

    /* assume inst != NULL */
    if (*inst != NULL) {
        r = (*inst)->refcount;
        if (r == 0) {
            (*inst)->close(*inst);
            (*inst)->free(*inst);
            *inst = NULL;
        }
    }

    return r;
}

dosamp_file_source_t dosamp_FAR dosamp_file_source_alloc(const_dosamp_file_source_t const inst_template) {
    dosamp_file_source_t inst;

    /* the reason we have a common alloc() is to enable pooling of structs in the future */
#if TARGET_MSDOS == 16
    inst = _fmalloc(sizeof(*inst));
#else
    inst = malloc(sizeof(*inst));
#endif

    if (inst != NULL)
        *inst = *inst_template;

    return inst;
}

void dosamp_FAR dosamp_file_source_free(dosamp_file_source_t const inst) {
    /* the reason we have a common free() is to enable pooling of structs in the future */
    /* ASSUME: inst != NULL */
#if TARGET_MSDOS == 16
    _ffree(inst);
#else
    free(inst);
#endif
}

static int dosamp_FAR dosamp_file_source_file_fd_close(dosamp_file_source_t const inst) {
    /* ASSUME: inst != NULL */
    if (inst->p.file_fd.fd >= 0) {
        close(inst->p.file_fd.fd);
        inst->p.file_fd.fd = -1;
    }

    return 0;/*success*/
}

static void dosamp_FAR dosamp_file_source_file_fd_free(dosamp_file_source_t const inst) {
    dosamp_file_source_file_fd_close(inst);
    dosamp_file_source_free(inst);
}

static unsigned int dosamp_FAR dosamp_file_source_file_fd_read(dosamp_file_source_t const inst,void dosamp_FAR * buf,unsigned int count) {
    int rd = 0;

    if (inst->p.file_fd.fd < 0 || count > dosamp_file_io_maxb)
        return dosamp_file_io_err;

    if (count > 0) {
#if TARGET_MSDOS == 16
        /* NTS: For 16-bit MS-DOS we must call MS-DOS read() directly instead of using the C runtime because
         *      the read() function in Open Watcom takes only a near pointer in small and compact memory models. */
        rd = _dos_xread(inst->p.file_fd.fd,buf,count);
#else
        rd = read(inst->p.file_fd.fd,buf,count);
#endif
        /* TODO: Open Watcom C always returns -1 on error (I checked the C runtime source to verify this)
         *       The _dos_xread() function in DOSLIB also returns -1 on error (note the SBB BX,BX and OR AX,BX in that code to enforce this when CF=1).
         *       For other OSes like Win32 and Linux we can't assume the same behavior. */
        if (rd == -1) return dosamp_file_io_err; /* also sets errno */
        inst->file_pos += (unsigned int)rd;
    }

    return (unsigned int)rd;
}

static unsigned int dosamp_FAR dosamp_file_source_file_fd_write(dosamp_file_source_t const inst,const void dosamp_FAR * buf,unsigned int count) {
    errno = EIO; /* not implemented */
    return dosamp_file_io_err;
}

static dosamp_file_off_t dosamp_FAR dosamp_file_source_file_fd_seek(dosamp_file_source_t const inst,dosamp_file_off_t pos) {
    off_t r;

    if (inst->p.file_fd.fd < 0 || pos == dosamp_file_io_err)
        return dosamp_file_off_err;

    if (pos > dosamp_file_off_max)
        pos = dosamp_file_off_max;

    r = lseek(inst->p.file_fd.fd,(off_t)pos,SEEK_SET);
    if (r == (off_t)-1L)
        return dosamp_file_off_err;

    return (inst->file_pos = (dosamp_file_off_t)r);
}
 
static const struct dosamp_file_source dosamp_file_source_priv_file_fd_init = {
    .obj_id =                           dosamp_file_source_id_file_fd,
    .file_size =                        -1LL,
    .file_pos =                         0,
    .free =                             dosamp_file_source_file_fd_free,
    .close =                            dosamp_file_source_file_fd_close,
    .read =                             dosamp_file_source_file_fd_read,
    .write =                            dosamp_file_source_file_fd_write,
    .seek =                             dosamp_file_source_file_fd_seek,
    .p.file_fd.fd =                     -1
};

/* FIXME: Possible API problem: Someday when we hand a pointer to this function off to external DLLs
 *        the "path" member will have to be const char dosamp_FAR * in order to accept C-strings from
 *        other data segments in 16-bit memory models, or else nobody except ourselves or anyone with
 *        with the same DGROUP segment will be able to use this function. The problem is that in all
 *        but the large memory models Open Watcom C's open() function takes only const char * (near).
 *        If Open Watcom's C runtime actually does have an _open() or variant that takes a const char FAR *
 *        path string in all 16-bit memory models that would solve this problem.
 *
 *        Then again, I suppose the ABI between us and those future DLLs could simply be defined instead
 *        where WE open the file and hand the DLL the file source object, and it doesn't have to care
 *        about opening, but that would then prevent future media code that may have to read from multiple
 *        files. */
dosamp_file_source_t dosamp_file_source_file_fd_open(const char * const path) {
    dosamp_file_source_t inst;
    struct stat st;

    if (path == NULL) return NULL;
    if (*path == 0) return NULL;

    inst = dosamp_file_source_alloc(&dosamp_file_source_priv_file_fd_init);
    if (inst == NULL) return NULL;

    /* TODO: #ifdef this code out so that it only applies to MS-DOS and Windows.
     *       C runtimes in MS-DOS and Windows cannot identify much about a file
     *       from it's handle but can sufficiently identify from the path. */
    if (stat(path,&st)) goto fail; /* cannot stat: fail */
    if (!S_ISREG(st.st_mode)) goto fail; /* not a file: fail */
    inst->file_size = (dosamp_file_off_t)st.st_size;

    inst->p.file_fd.fd = open(path,O_RDONLY|O_BINARY);
    if (inst->p.file_fd.fd < 0) goto fail;

    /* TODO: For Linux, and other OSes where we can fully identify a file
     *       from it's handle, use fstat() and check S_ISREG() and such HERE */

    if (lseek(inst->p.file_fd.fd,0,SEEK_SET) != 0) goto fail_fd; /* cannot lseek: fail */

    return inst;
fail_fd:
fail:
    inst->close(inst);
    inst->free(inst);
    return NULL;
}

static dosamp_file_source_t             wav_source = NULL;

static char*                            wav_file = NULL;

struct wav_cbr_t {
    uint32_t                            sample_rate;
    uint16_t                            number_of_channels;
    uint16_t                            bits_per_sample;
    uint16_t                            bytes_per_block;
    uint16_t                            samples_per_block;
};

struct wav_cbr_t                        file_codec;
struct wav_cbr_t                        play_codec;

static unsigned long                    wav_play_dma_position = 0;
static unsigned long                    wav_play_position = 0L;
static unsigned long                    wav_data_offset = 44;
static unsigned long                    wav_data_length_bytes = 0;
static unsigned long                    wav_data_length = 0;/* in samples */;
static unsigned long                    wav_position = 0;/* in samples. read pointer. after reading, points to next sample to read. */
static unsigned long                    wav_play_load_block_size = 0;
static unsigned long                    wav_play_min_load_size = 0;

/* NTS: If long-term playback is desired (long enough to overflow these variables)
 *      you should "extend" them by tracking their previous values and then adding
 *      to a "high half" when they wrap around. */
static unsigned long                    wav_write_counter = 0;/* at buffer_last_io, bytes written since play start */
static unsigned long                    wav_play_counter = 0;/* at dma position, bytes played since play start */

static unsigned long                    wav_play_delay_bytes = 0;/* in bytes. delay from wav_position to where sound card is playing now. */
static unsigned long                    wav_play_delay = 0;/* in samples. delay from wav_position to where sound card is playing now. */
static unsigned char                    wav_play_empty = 0;/* if set, buffer is empty. else, audio data is there */
static unsigned char                    wav_playing = 0;
static unsigned char                    wav_prepared = 0;

static const unsigned long              resample_100 = 1UL << 16UL;

static unsigned long                    resample_step = 0; /* 16.16 resampling step */
static unsigned long                    resample_frac = 0;
static unsigned char                    resample_on = 0;

void update_wav_dma_position(void) {
    _cli();
    wav_play_dma_position = sndsb_read_dma_buffer_position(sb_card);
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
    if (wav_playing) sndsb_irq_continue(sb_card,c);

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

void update_wav_play_delay();

/* this depends on keeping the "play delay" up to date */
uint32_t can_write(void) { /* in bytes */
    uint32_t x;

    update_wav_dma_position();
    update_wav_play_delay();

    x = sb_card->buffer_size;
    if (x >= wav_play_delay_bytes) x -= wav_play_delay_bytes;
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
        wav_write_counter += want;
        sb_card->buffer_last_io += want;
        if (sb_card->buffer_last_io >= sb_card->buffer_size)
            sb_card->buffer_last_io = 0;

        /* now that audio has been written, buffer is no longer empty */
        wav_play_empty = 0;

        /* update */
        update_wav_dma_position();
        update_wav_play_delay();
    }

    /* return ptr */
    return dosamp_ptr_add_normalize(sb_dma->lin,ret_pos);
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
        wav_position = wav_source->file_pos - wav_data_offset;
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

/* the reason I added "written/played" accounting is to make it possible
 * to robustly follow playback and act on events relative to playback.
 * such as, knowing that at such and such playback time, we looped the
 * WAV file back or switched to a new one. */
struct audio_playback_rebase_t {
    unsigned long           event_at;       /* playback time byte count */
    unsigned long           wav_position;   /* starting WAV position to count from using playback time */
};

#define MAX_REBASE                  32

struct audio_playback_rebase_t      wav_rebase_events[MAX_REBASE];
unsigned char                       wav_rebase_read=0,wav_rebase_write=0;

void wav_rebase_clear(void) {
    wav_rebase_read = 0;
    wav_rebase_write = 0;
}

void rebase_flush_old(unsigned long before) {
    int howmany;

    howmany = (int)wav_rebase_write - (int)wav_rebase_read;
    if (howmany < 0) howmany += MAX_REBASE;

    /* flush all but one */
    while (wav_rebase_read != wav_rebase_write && howmany > 1) {
        if (wav_rebase_events[wav_rebase_read].event_at >= before)
            break;

        howmany--;
        wav_rebase_read++;
        if (wav_rebase_read >= MAX_REBASE) wav_rebase_read = 0;
    }
}

struct audio_playback_rebase_t *rebase_find(unsigned long event) {
    struct audio_playback_rebase_t *r = NULL;
    unsigned char i = wav_rebase_read;

    while (i != wav_rebase_write) {
        if (event >= wav_rebase_events[i].event_at)
            r = &wav_rebase_events[i];
        else
            break;

        i++;
        if (i >= MAX_REBASE) i = 0;
    }

    return r;
}

struct audio_playback_rebase_t *rebase_add(void) {
    unsigned char i = wav_rebase_write;

    {
        unsigned char j = i + 1;
        if (j >= MAX_REBASE) j -= MAX_REBASE;
        if (j == wav_rebase_read) {
            /* well then we just throw the oldest event away */
            wav_rebase_read++;
            if (wav_rebase_read >= MAX_REBASE) wav_rebase_read = 0;
        }
        wav_rebase_write = j;
    }

    {
        struct audio_playback_rebase_t *p = &wav_rebase_events[i];

        memset(p,0,sizeof(*p));
        return p;
    }
}

void wav_rebase_position_event(void) {
    /* make a rebase event */
    struct audio_playback_rebase_t *r = rebase_add();

    if (r != NULL) {
        r->event_at = wav_write_counter;
        r->wav_position = wav_position;
    }
}

static unsigned char dosamp_FAR * tmpbuffer = NULL;
static size_t tmpbuffer_sz = 4096;

void tmpbuffer_free(void) {
    if (tmpbuffer != NULL) {
#if TARGET_MSDOS == 32
        free(tmpbuffer);
#else
        _ffree(tmpbuffer);
#endif
        tmpbuffer = NULL;
    }
}

unsigned char dosamp_FAR * tmpbuffer_get(uint32_t *sz) {
    if (tmpbuffer == NULL) {
#if TARGET_MSDOS == 32
        tmpbuffer = malloc(tmpbuffer_sz);
#else
        tmpbuffer = _fmalloc(tmpbuffer_sz);
#endif
    }

    if (sz != NULL) *sz = tmpbuffer_sz;
    return tmpbuffer;
}

void card_poll(void) {
	sndsb_main_idle(sb_card);
    update_wav_dma_position();
    update_wav_play_delay();
}

unsigned char use_mmap_write = 1;

static void load_audio_exact(uint32_t howmuch/*in bytes*/) { /* load audio up to point or max */
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
        wav_position = wav_source->file_pos / play_codec.bytes_per_block;
        howmuch -= towrite;
    }
}

static void load_audio_resample(uint32_t howmuch/*in bytes*/) {
    /* TODO */
    load_audio_exact(howmuch);
}

static void load_audio(uint32_t howmuch/*in bytes*/) { /* load audio up to point or max */
    if (resample_on)
        load_audio_resample(howmuch);
    else
        load_audio_exact(howmuch);
}

void update_wav_play_delay() {
    signed long delay;

    /* DMA trails our "last IO" pointer */
    delay  = (signed long)sb_card->buffer_last_io;
    delay -= (signed long)wav_play_dma_position;

    /* delay == 0 is a special case.
     * if wav_play_empty, then it means there's no delay.
     * else, it means there's one whole buffer's worth delay.
     * we HAVE to make this distinction because this code is
     * written to load new audio data RIGHT BEHIND the DMA position
     * which could easily lead to buffer_last_io == DMA position! */
    if (delay < 0L) delay += (signed long)sb_card->buffer_size;
    else if (delay == 0L && !wav_play_empty) delay = (signed long)sb_card->buffer_size;

    /* guard against inconcievable cases */
    if (delay < 0L) delay = 0L;
    if (delay >= (signed long)sb_card->buffer_size) delay = (signed long)sb_card->buffer_size;

    /* convert to samples */
    wav_play_delay_bytes = (unsigned long)delay;
    wav_play_delay = ((unsigned long)delay / play_codec.bytes_per_block) * play_codec.samples_per_block;

    /* play position is calculated here */
    wav_play_counter = wav_write_counter;
    if (wav_play_counter >= wav_play_delay_bytes) wav_play_counter -= wav_play_delay_bytes;
    else wav_play_counter = 0;
}

void update_play_position(void) {
    struct audio_playback_rebase_t *r;

    r = rebase_find(wav_play_counter);

    if (r != NULL) {
        wav_play_position =
            ((unsigned long long)(((wav_play_counter - r->event_at) / play_codec.bytes_per_block) *
                (unsigned long long)resample_step) >> 16ULL) + r->wav_position;
    }
    else if (wav_playing)
        wav_play_position = 0;
    else
        wav_play_position = wav_position;
}

void clear_remapping(void) {
    wav_rebase_clear();
}

void move_pos(signed long adj) {
    if (wav_playing)
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
	if (!wav_playing || wav_source == NULL)
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

                        file_codec.number_of_channels = le16toh(wfx->nChannels);
                        file_codec.bits_per_sample = le16toh(wfx->wBitsPerSample);
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

    /* done */
    return 0;
fail:
    dosamp_file_source_release(wav_source);
    dosamp_file_source_autofree(&wav_source);
    wav_source = NULL;
    return -1;
}

static void free_dma_buffer() {
    if (sb_dma != NULL) {
        dma_8237_free_buffer(sb_dma);
        sb_dma = NULL;
    }
}

static void realloc_dma_buffer() {
    uint32_t choice;
    int8_t ch;

    free_dma_buffer();

    ch = sndsb_dsp_playback_will_use_dma_channel(sb_card,play_codec.sample_rate,/*stereo*/play_codec.number_of_channels > 1,/*16-bit*/play_codec.bits_per_sample > 8);

    if (ch >= 4)
        choice = sndsb_recommended_16bit_dma_buffer_size(sb_card,0);
    else
        choice = sndsb_recommended_dma_buffer_size(sb_card,0);

    do {
        if (ch >= 4)
            sb_dma = dma_8237_alloc_buffer_dw(choice,16);
        else
            sb_dma = dma_8237_alloc_buffer_dw(choice,8);

        if (sb_dma == NULL) choice -= 4096UL;
    } while (sb_dma == NULL && choice > 4096UL);

    /* NTS: We WANT to call sndsb_assign_dma_buffer with sb_dma == NULL if it happens because it tells the Sound Blaster library to cancel it's copy as well */
    if (!sndsb_assign_dma_buffer(sb_card,sb_dma))
        return;
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

int sb_check_dma_buffer(void) {
    int8_t ch;

    /* alloc DMA buffer.
     * if already allocated, then realloc if changing from 8-bit to 16-bit DMA */
    if (sb_dma == NULL)
        realloc_dma_buffer();
    else {
        ch = sndsb_dsp_playback_will_use_dma_channel(sb_card,
            /*rate*/play_codec.sample_rate,
            /*stereo*/play_codec.number_of_channels > 1,
            /*16-bit*/play_codec.bits_per_sample > 8);

        if (ch >= 0 && sb_dma->dma_width != (ch >= 4 ? 16 : 8))
            realloc_dma_buffer();
    }

    if (sb_dma == NULL)
        return -1;

	if (!sndsb_assign_dma_buffer(sb_card,sb_dma))
        return -1;

    /* we want the DMA buffer region actually used by the card to be a multiple of (2 x the block size we play audio with).
     * we can let that be less than the actual DMA buffer by some bytes, it's fine. */
    {
        uint32_t adj = 2UL * (unsigned long)play_codec.bytes_per_block;

        sb_card->buffer_size = sb_dma->length;
        sb_card->buffer_size -= sb_card->buffer_size % adj;
        if (sb_card->buffer_size == 0UL) return -1;
    }

    return 0;
}

int negotiate_play_format(struct wav_cbr_t * const d,const struct wav_cbr_t * const s) {
    int r;

    /* by default, use source format */
    *d = *s;

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
    sb_card->buffer_size = 1;
    sb_card->buffer_phys = 0;

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

    {
        unsigned long long tmp;

        tmp  = (unsigned long long)s->sample_rate << 16ULL;
        tmp /= (unsigned long long)d->sample_rate;

        resample_step = (unsigned long)tmp;
        resample_frac = 0;
    }

    if (resample_step == resample_100 && d->number_of_channels == s->number_of_channels && d->bits_per_sample == s->bits_per_sample)
        resample_on = 0;
    else
        resample_on = 1;

    return 0;
}

void disable_autoinit(void) {
    sb_card->dsp_autoinit_dma = 0;
    sb_card->dsp_autoinit_command = 0;
}

int prepare_buffer(void) {
    if (sb_check_dma_buffer() < 0)
        return -1;

	if (sb_card->dsp_play_method == SNDSB_DSPOUTMETHOD_DIRECT)
        return -1;

    return 0;
}

int prepare_play(void) {
    if (wav_prepared)
        return 0;

	if (sb_card->dsp_play_method == SNDSB_DSPOUTMETHOD_DIRECT)
        return -1;

    if (sb_check_dma_buffer() < 0)
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
    wav_prepared = 1;
    return 0;
}

int unprepare_play(void) {
    if (wav_prepared) {
        sndsb_stop_dsp_playback(sb_card);
        wav_prepared = 0;
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
    if (wav_prepared) return sb_card->buffer_irq_interval;

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

static int begin_play() {
	if (wav_playing)
		return 0;

    if (wav_source == NULL)
		return -1;

    /* reset state */
    wav_rebase_clear();
    wav_write_counter = 0;
    wav_play_delay = 0;
    wav_play_empty = 1;

    /* get the file pointer ready */
    wav_position_to_file_pointer();

    /* make a rebase event */
    wav_rebase_position_event();

    /* choose output vs input */
    if (negotiate_play_format(&play_codec,&file_codec) < 0) {
        unprepare_play();
        return -1;
    }

    /* prepare buffer */
    if (prepare_buffer() < 0) {
        unprepare_play();
        return -1;
    }

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
	wav_playing = 1;
	_sti();

    return 0;
}

static void stop_play() {
	if (!wav_playing) return;

    /* stop */
    stop_sound_card();
    unprepare_play();

    wav_position = wav_play_position;
    update_play_position();

	_cli();
    wav_playing = 0;
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
    unsigned char percent;

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
            while ((m|d) >= 0x100000UL) {
                m >>= 4UL;
                d >>= 4UL;
            }

            /* we don't want divide by zero */
            if (d == 0UL) d = 1UL;

            percent = (unsigned char)((m * 100UL) / d);
        }

        printf("\x0D");
        printf("%02u:%02u:%02u.%02u %%%02u %lu / %lu ",hour,min,sec,centisec,percent,wav_play_position,wav_data_length);
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
        (unsigned long)wav_play_delay_bytes,
        (unsigned long)can_write(),
        (unsigned long)wav_write_counter,
        (unsigned long)wav_play_counter,
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

int calibrate_rdtsc(void) {
    unsigned long long ts_prev,ts_cur,tcc=0,rcc=0;
    rdtsc_t rd_prev,rd_cur;

    /* if we can't open the current time source then exit */
    if (time_source->open(time_source) < 0)
        return -1;

    /* announce (or the user will complain about 1-second delays at startup) */
    printf("Your CPU offers RDTSC instruction, calibrating RDTSC clock...\n");

    /* begin */
    ts_cur = time_source->poll(time_source);
    rd_cur = cpu_rdtsc();

    /* wait for one tick, because we may have just started in the middle of a clock tick */
    do {
        ts_prev = ts_cur; ts_cur = time_source->poll(time_source);
        rd_prev = rd_cur; rd_cur = cpu_rdtsc();
    } while (ts_cur == ts_prev);

    /* time for 1 second */
    do {
        tcc += ts_cur - ts_prev;
        rcc += rd_cur - rd_prev;
        ts_prev = ts_cur; ts_cur = time_source->poll(time_source);
        rd_prev = rd_cur; rd_cur = cpu_rdtsc();
    } while (tcc < time_source->clock_rate);

    /* close current time source */
    time_source->close(time_source);

    /* reject if out of range, or less precision than current time source */
    if (rcc < 10000000ULL) return -1; /* Pentium processors started at 60MHz, we expect at least 10MHz precision */
    if (rcc < time_source->clock_rate) return -1; /* RDTSC must provide more precision than current source */

    printf("RDTSC clock rate: %lluHz\n",rcc);

    dosamp_time_source_rdtsc.clock_rate = rcc;
    return 0;
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
	sndsb_detect_virtualbox();		// whether or not we're running in VirtualBox
	/* sndsb now allows us to keep the EXE small by not referring to extra sound card support */
	sndsb_enable_sb16_support();		// SB16 support
	sndsb_enable_sc400_support();		// SC400 support
	sndsb_enable_ess_audiodrive_support();	// ESS AudioDrive support

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
                    isa_pnp_write_data_register(0x02,0x02);	/* bit 1: set -> return to Wait For Key state (or else a Pentium Pro system I own eventually locks up and hangs) */
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

    /* for now, always use 8254 PIT as time source */
    time_source = &dosamp_time_source_8254;

    /* NTS: When using the PIT source we NEED to do this, or else DOSBox-X (and possibly
     *      many other motherboards) will leave the PIT in a mode that produces the correct
     *      IRQ 0 rate but counts down twice as fast. We need the PIT to count down by 1,
     *      not by 2, in order to keep time. */
    write_8254_system_timer(0); /* 18.2 tick/sec on our terms (proper PIT mode) */

    /* we can use the Time Stamp Counter on Pentium or higher systems that offer it */
	if (cpu_flags & CPU_FLAG_CPUID) {
        if (cpu_cpuid_features.a.raw[2] & 0x10) {
            /* RDTSC is available. We just have to figure out how fast it runs. */
            if (calibrate_rdtsc() >= 0)
                time_source = &dosamp_time_source_rdtsc;
        }
    }

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
                    unsigned char wp = wav_playing;

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
                else if (i == '[') {
                    unsigned char wp = wav_playing;

                    if (wp) stop_play();
                    move_pos(-((signed long)file_codec.sample_rate)); /* -1 sec */
                    if (wp) begin_play();
                }
                else if (i == ']') {
                    unsigned char wp = wav_playing;

                    if (wp) stop_play();
                    move_pos(file_codec.sample_rate); /* 1 sec */
                    if (wp) begin_play();
                }
                else if (i == '{') {
                    unsigned char wp = wav_playing;

                    if (wp) stop_play();
                    move_pos(-((signed long)file_codec.sample_rate * 30L)); /* -30 sec */
                    if (wp) begin_play();
                }
                else if (i == '}') {
                    unsigned char wp = wav_playing;

                    if (wp) stop_play();
                    move_pos(file_codec.sample_rate * 30L); /* 30 sec */
                    if (wp) begin_play();
                }
                else if (i == ' ') {
                    if (wav_playing) stop_play();
                    else begin_play();
                }
            }
        }
    }

	_sti();
	stop_play();
	close_wav();
    tmpbuffer_free();

    free_sound_blaster_support();

    time_source->close(time_source);
	return 0;
}

