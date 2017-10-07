
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
#include <hw/8237/8237.h>		/* 8237 DMA */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/dos/doswin.h>
#include <hw/dos/tgusmega.h>
#include <hw/dos/tgussbos.h>
#include <hw/dos/tgusumid.h>
#include <hw/isapnp/isapnp.h>
#include <hw/sndsb/sndsbpnp.h>

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

static unsigned char                    wav_stereo = 0,wav_16bit = 0,wav_bytes_per_sample = 1;
static unsigned long                    wav_data_offset = 44,wav_data_length = 0,wav_sample_rate = 8000,wav_position = 0,wav_buffer_filepos = 0;
static unsigned char                    wav_playing = 0;

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

static void load_audio(struct sndsb_ctx *cx,uint32_t up_to,uint32_t min,uint32_t max,uint8_t initial) { /* load audio up to point or max */
#if TARGET_MSDOS == 32
	unsigned char *buffer = sb_dma->lin;
#else
	unsigned char far *buffer = sb_dma->lin;
#endif
	int rd,i,bufe=0;
	uint32_t how;

	/* caller should be rounding! */
	assert((up_to & 3UL) == 0UL);
	if (up_to >= cx->buffer_size) return;
	if (cx->buffer_size < 32) return;
	if (cx->buffer_last_io == up_to) return;

	if (sb_card->dsp_adpcm > 0 && (wav_16bit || wav_stereo)) return;
	if (max == 0) max = cx->buffer_size/4;
	if (max < 16) return;

	wav_source->seek(wav_source,wav_data_offset + (wav_position * (unsigned long)wav_bytes_per_sample));

	if (cx->buffer_last_io == 0)
		wav_buffer_filepos = wav_position;

	while (max > 0UL) {
		if (cx->backwards) {
			if (up_to > cx->buffer_last_io) {
				how = cx->buffer_last_io;
				if (how == 0) how = cx->buffer_size - up_to;
				bufe = 1;
			}
			else {
				how = (cx->buffer_last_io - up_to);
				bufe = 0;
			}
		}
		else {
			if (up_to < cx->buffer_last_io) {
				how = (cx->buffer_size - cx->buffer_last_io); /* from last IO to end of buffer */
				bufe = 1;
			}
			else {
				how = (up_to - cx->buffer_last_io); /* from last IO to up_to */
				bufe = 0;
			}
		}

		if (how > 16384UL)
			how = 16384UL;

		if (how == 0UL)
			break;
		else if (how > max)
			how = max;
		else if (!bufe && how < min)
			break;

		if (cx->buffer_last_io == 0)
			wav_buffer_filepos = wav_position;

        {
            uint32_t oa,adj;

			oa = cx->buffer_last_io;
			if (cx->backwards) {
				if (cx->buffer_last_io == 0) {
					cx->buffer_last_io = cx->buffer_size - how;
				}
				else if (cx->buffer_last_io >= how) {
					cx->buffer_last_io -= how;
				}
				else {
					abort();
				}

				adj = (uint32_t)how / wav_bytes_per_sample;
				if (wav_position >= adj) wav_position -= adj;
				else if (wav_position != 0UL) wav_position = 0;
				else {
					wav_position = wav_source->file_size;
					if (wav_position >= adj) wav_position -= adj;
					else if (wav_position != 0UL) wav_position = 0;
					wav_position /= wav_bytes_per_sample;
				}

				wav_source->seek(wav_source,wav_data_offset + (wav_position * (unsigned long)wav_bytes_per_sample));
			}

			assert(cx->buffer_last_io <= cx->buffer_size);
            rd = wav_source->read(wav_source,dosamp_ptr_add_normalize(buffer,cx->buffer_last_io),how);
			if (rd == 0 || rd == -1) {
				if (!cx->backwards) {
					wav_position = 0;
					wav_source->seek(wav_source,wav_data_offset + (wav_position * (unsigned long)wav_bytes_per_sample));
					rd = wav_source->read(wav_source,dosamp_ptr_add_normalize(buffer,cx->buffer_last_io),how);
					if (rd == 0 || rd == -1) {
						/* hmph, fine */
#if TARGET_MSDOS == 32
						memset(buffer+cx->buffer_last_io,128,how);
#else
						_fmemset(buffer+cx->buffer_last_io,128,how);
#endif
						rd = (int)how;
					}
				}
				else {
					rd = (int)how;
				}
			}

			assert((cx->buffer_last_io+((uint32_t)rd)) <= cx->buffer_size);
			if (sb_card->audio_data_flipped_sign) {
				if (wav_16bit)
					for (i=0;i < (rd-1);i += 2) buffer[cx->buffer_last_io+i+1] ^= 0x80;
				else
					for (i=0;i < rd;i++) buffer[cx->buffer_last_io+i] ^= 0x80;
			}

			if (!cx->backwards) {
				cx->buffer_last_io += (uint32_t)rd;
				wav_position += (uint32_t)rd / wav_bytes_per_sample;
			}
		}

		assert(cx->buffer_last_io <= cx->buffer_size);
		if (!cx->backwards) {
			if (cx->buffer_last_io == cx->buffer_size) cx->buffer_last_io = 0;
		}
		max -= (uint32_t)rd;
	}

	if (cx->buffer_last_io == 0)
		wav_buffer_filepos = wav_position;
}

#define DMA_WRAP_DEBUG

static void wav_idle() {
	const unsigned int leeway = sb_card->buffer_size / 100;
	uint32_t pos;
#ifdef DMA_WRAP_DEBUG
	uint32_t pos2;
#endif

	if (!wav_playing || wav_source == NULL)
		return;

	/* if we're playing without an IRQ handler, then we'll want this function
	 * to poll the sound card's IRQ status and handle it directly so playback
	 * continues to work. if we don't, playback will halt on actual Creative
	 * Sound Blaster 16 hardware until it gets the I/O read to ack the IRQ */
	sndsb_main_idle(sb_card);

	_cli();
#ifdef DMA_WRAP_DEBUG
	pos2 = sndsb_read_dma_buffer_position(sb_card);
#endif
	pos = sndsb_read_dma_buffer_position(sb_card);
#ifdef DMA_WRAP_DEBUG
	if (sb_card->backwards) {
		if (pos2 < 0x1000 && pos >= (sb_card->buffer_size-0x1000)) {
			/* normal DMA wrap-around, no problem */
		}
		else {
			if (pos > pos2)	fprintf(stderr,"DMA glitch! 0x%04lx 0x%04lx\n",pos,pos2);
			else		pos = max(pos,pos2);
		}

		pos += leeway;
		if (pos >= sb_card->buffer_size) pos -= sb_card->buffer_size;
	}
	else {
		if (pos < 0x1000 && pos2 >= (sb_card->buffer_size-0x1000)) {
			/* normal DMA wrap-around, no problem */
		}
		else {
			if (pos < pos2)	fprintf(stderr,"DMA glitch! 0x%04lx 0x%04lx\n",pos,pos2);
			else		pos = min(pos,pos2);
		}

		if (pos < leeway) pos += sb_card->buffer_size - leeway;
		else pos -= leeway;
	}
#endif
	pos &= (~3UL); /* round down */
	_sti();

    /* load from disk */
    load_audio(sb_card,pos,min(wav_sample_rate/8,4096)/*min*/,
        sb_card->buffer_size/4/*max*/,0/*first block*/);
}

static void update_cfg();

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
        wav_sample_rate = 0;
        wav_bytes_per_sample = 0;
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

                        wav_sample_rate = le32toh(wfx->nSamplesPerSec);
                        wav_stereo = le16toh(wfx->nChannels) > 1;
                        wav_16bit = le16toh(wfx->wBitsPerSample) > 8;
                        wav_bytes_per_sample = (wav_stereo ? 2 : 1) * (wav_16bit ? 2 : 1);
                    }
                }
            }
            else if (!memcmp(tmp,"data",4)) {
                wav_data_offset = scan + 8UL;
                wav_data_length = len;
            }

            /* next! */
            scan += len + 8UL;
        }

        if (wav_sample_rate == 0UL || wav_data_length == 0UL) goto fail;
	}

	update_cfg();
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

    ch = sndsb_dsp_playback_will_use_dma_channel(sb_card,wav_sample_rate,wav_stereo,wav_16bit);

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

    if (!sndsb_assign_dma_buffer(sb_card,sb_dma))
        return;
    if (sb_dma == NULL)
        return;
}

static void begin_play() {
	unsigned long choice_rate;

	if (wav_playing)
		return;

	if (sb_card->dsp_play_method == SNDSB_DSPOUTMETHOD_DIRECT)
        return;

    if (sb_dma == NULL)
        realloc_dma_buffer();

    {
        int8_t ch = sndsb_dsp_playback_will_use_dma_channel(sb_card,wav_sample_rate,wav_stereo,wav_16bit);
        if (ch >= 0) {
            if (sb_dma->dma_width != (ch >= 4 ? 16 : 8))
                realloc_dma_buffer();
            if (sb_dma == NULL)
                return;
        }
    }

    if (sb_dma != NULL) {
        if (!sndsb_assign_dma_buffer(sb_card,sb_dma))
            return;
    }

	if (wav_source == NULL)
		return;

	choice_rate = wav_sample_rate;

	update_cfg();
	if (!sndsb_prepare_dsp_playback(sb_card,choice_rate,wav_stereo,wav_16bit))
		return;

	sndsb_setup_dma(sb_card);

    load_audio(sb_card,sb_card->buffer_size/2,0/*min*/,0/*max*/,1/*first block*/);

	/* make sure the IRQ is acked */
	if (sb_card->irq >= 8) {
		p8259_OCW2(8,P8259_OCW2_SPECIFIC_EOI | (sb_card->irq & 7));
		p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | 2);
	}
	else if (sb_card->irq >= 0) {
		p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | sb_card->irq);
	}
	if (sb_card->irq >= 0)
		p8259_unmask(sb_card->irq);

	if (!sndsb_begin_dsp_playback(sb_card))
		return;

	_cli();
	wav_playing = 1;
	_sti();
}

static void stop_play() {
	if (!wav_playing) return;

	_cli();
	sndsb_stop_dsp_playback(sb_card);
	wav_playing = 0;
	_sti();
}

static void update_cfg() {
    sb_card->dsp_adpcm = 0;
    sb_card->buffer_irq_interval = sb_card->buffer_size / wav_bytes_per_sample;
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

int main(int argc,char **argv) {
	int i,loop,redraw,bkgndredraw;

    if (!parse_argv(argc,argv))
        return 1;

	probe_8237();
	if (!probe_8259()) {
		printf("Cannot init 8259 PIC\n");
		return 1;
	}
	if (!probe_8254()) {
		printf("Cannot init 8254 timer\n");
		return 1;
	}
	if (!init_sndsb()) {
		printf("Cannot init library\n");
		return 1;
	}
	if (!init_isa_pnp_bios()) {
		printf("Cannot init ISA PnP\n");
		return 1;
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
    if (find_isa_pnp_bios()) {
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

    /* Non-plug & play scan */
	if (sndsb_try_blaster_var() != NULL) {
		if (!sndsb_init_card(sndsb_card_blaster))
			sndsb_free_card(sndsb_card_blaster);
	}

    /* Most SB cards exist at 220h or 240h */
    sndsb_try_base(0x220);
    sndsb_try_base(0x240);

    /* now let the user choose */
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

    realloc_dma_buffer();

	if (!sndsb_assign_dma_buffer(sb_card,sb_dma)) {
		printf("Cannot assign DMA buffer\n");
		return 1;
	}

	if (sb_card->irq != -1) {
		old_irq_masked = p8259_is_masked(sb_card->irq);
		if (vector_is_iret(irq2int(sb_card->irq)))
			old_irq_masked = 1;

		old_irq = _dos_getvect(irq2int(sb_card->irq));
		_dos_setvect(irq2int(sb_card->irq),sb_irq);
		p8259_unmask(sb_card->irq);
	}

	loop=1;
	redraw=1;
	bkgndredraw=1;

    if (open_wav() < 0) {
        printf("Failed to open file\n");
    }
    else {
        begin_play();

        while (loop) {
            wav_idle();

            if (kbhit()) {
                i = getch();
                if (i == 0) i = getch() << 8;

                if (i == 27) {
                    loop = 0;
                    break;
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
    free_dma_buffer();

	if (sb_card->irq >= 0 && old_irq_masked)
		p8259_mask(sb_card->irq);

	if (sb_card->irq != -1)
		_dos_setvect(irq2int(sb_card->irq),old_irq);

	sndsb_free_card(sb_card);
	free_sndsb(); /* will also de-ref/unhook the NMI reflection */
	return 0;
}

