
#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/cpu/cpu.h>
#include <hw/vga/vga.h>
#include <hw/8042/8042.h>
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>

#include "gif_lib.h"

enum {
    GAME_TITLE=0,               // 0
    GAME_MENU,
    GAME_EXIT
};

unsigned char game_running_state = GAME_TITLE;
unsigned char game_running_state_stack[8];
unsigned char game_running_state_stack_sp=0;

FILE *debug_log = NULL;

void release_timer(void);

void DEBUG(const char *fmt,...) {
    va_list va;

    if (debug_log == NULL) return;

    va_start(va,fmt);
    vfprintf(debug_log,fmt,va);
    va_end(va);
    fprintf(debug_log,"\n");
}

void FAIL(const char *msg) {
    DEBUG("FAIL: %s",msg);
    release_timer();
    int10_setmode(0x3); /* text mode */
    puts(msg);
    exit(1);
}

void game_running_state_push(void) {
    if (game_running_state_stack_sp >= sizeof(game_running_state_stack))
        FAIL("Game running state stack overrun");

    game_running_state_stack[game_running_state_stack_sp++] = game_running_state;
}

void game_running_state_set(const unsigned char state) {
    game_running_state = state;
}

unsigned char game_running_state_stack_is_empty(void) {
    return (game_running_state_stack_sp == 0);
}

void game_running_state_pop(void) {
    if (game_running_state_stack_sp == 0)
        FAIL("Game running state stack underrun");

    game_running_state_set(game_running_state_stack[--game_running_state_stack_sp]);
}

/* font FNT blob */
typedef struct FNTBlob {
    unsigned char*          raw;
    size_t                  raw_length;
    size_t                  block_ofs[5+1-1]; /* start from 1, up to 5 inclusive */
    GifFileType*            gif;
    SavedImage*             img;
    unsigned char           ascii_lookup[127-32]; /* quick lookup for common ASCII chars 0xFF = no such entry */
    int                     y_adj; /* rendering offset due to a/r correction */
} FNTBlob;

unsigned char *FNTBlob_get_block(FNTBlob *b,unsigned int block,size_t *sz) {
    unsigned char *p;
    size_t o;

    if (b == NULL || block < 1 || block > 5 || sz == NULL)
        return NULL;
    if (b->raw == NULL)
        return NULL;

    o = b->block_ofs[block-1];
    if (o == 0)
        return NULL;

    p = b->raw + o;
    *sz = (size_t) *((uint32_t*)(p-4));
    return p;
}

#pragma pack(push,1)
typedef struct FNTBlob_info {
    int16_t             fontSize;           // +0x00
    uint8_t             bitField;           // +0x02
    uint8_t             charSet;            // +0x03
    uint16_t            stretchH;           // +0x04
    uint8_t             aa;                 // +0x06
    uint8_t             paddingUp;          // +0x07
    uint8_t             paddingRight;       // +0x08
    uint8_t             paddingDown;        // +0x09
    uint8_t             paddingLeft;        // +0x0A
    uint8_t             spacingHoriz;       // +0x0B
    uint8_t             spacingVert;        // +0x0C
    uint8_t             outline;            // +0x0D
    // don't care beyond this point
} FNTBlob_info;

typedef struct FNTBlob_common {
    uint16_t            lineHeight;         // +0x00
    uint16_t            base;               // +0x02
    uint16_t            scaleW;             // +0x04
    uint16_t            scaleH;             // +0x06
    uint16_t            pages;              // +0x08
    uint8_t             bitField;           // +0x0A
    uint8_t             alphaChnl;          // +0x0B
    uint8_t             redChnl;            // +0x0C
    uint8_t             greenChnl;          // +0x0D
    uint8_t             blueChnl;           // +0x0E
} FNTBlob_common;

typedef struct FNTBlob_chars {
    uint32_t            id;                 // +0x00
    uint16_t            x;                  // +0x04
    uint16_t            y;                  // +0x06
    uint16_t            w;                  // +0x08
    uint16_t            h;                  // +0x0A
    int16_t             xoffset;            // +0x0C
    int16_t             yoffset;            // +0x0E
    int16_t             xadvance;           // +0x10
    uint8_t             page;               // +0x12
    uint8_t             chnl;               // +0x13
} FNTBlob_chars;                            // =0x14
#pragma pack(pop)

FNTBlob_info *FNTBlob_get_info(FNTBlob *b) {
    size_t sz;
    unsigned char *p = FNTBlob_get_block(b,/*info*/1,&sz);
    if (p == NULL || sz < sizeof(FNTBlob_info)) return NULL;

    return (FNTBlob_info*)p;
}

FNTBlob_common *FNTBlob_get_common(FNTBlob *b) {
    size_t sz;
    unsigned char *p = FNTBlob_get_block(b,/*common*/2,&sz);
    if (p == NULL || sz < sizeof(FNTBlob_common)) return NULL;

    return (FNTBlob_common*)p;
}

FNTBlob_chars *FNTBlob_get_chars(FNTBlob *b,FNTBlob_chars **fence) {
    size_t sz;
    unsigned char *p = FNTBlob_get_block(b,/*chars*/4,&sz);
    if (p == NULL || sz < sizeof(FNTBlob_chars)) return NULL;
    *fence = (FNTBlob_chars*)(p+sz);
    return (FNTBlob_chars*)p;
}

FNTBlob_chars *FNTBlob_find_char(FNTBlob *b,uint32_t id) {
    FNTBlob_chars *cf;
    FNTBlob_chars *c = FNTBlob_get_chars(b,&cf);
    if (c == NULL) return NULL;

    if (id >= 32 && id <= 127) {
        if (b->ascii_lookup[id-32] != 0xFF)
            return c + b->ascii_lookup[id-32];
    }

    for (;c < cf;c++) {
        if (c->id == id)
            return c;
    }

    return NULL; /* not found */
}

void FNTBlob_ascii_lookup_build(FNTBlob *b) {
    FNTBlob_chars *fc,*fb;
    unsigned int c;

    /* quick lookup for ASCII chars 32-127 because linear scan is slow */
    fb = FNTBlob_get_chars(b,&fc);
    for (c=32;c < 127;c++) {
        b->ascii_lookup[c-32] = 0xFF;

        fc = FNTBlob_find_char(b,c);
        if (fc != NULL)
            b->ascii_lookup[c-32] = (unsigned char)(fc - fb);
    }
}

FNTBlob *FNTBlob_alloc(void) {
    FNTBlob *b = malloc(sizeof(*b));
    if (b == NULL) return NULL;
    memset(b,0,sizeof(*b));
    return b;
}

void FNTBlob_free_gif(FNTBlob *b) {
    int err;

    if (b->gif != NULL) {
        DGifCloseFile(b->gif,&err);
        b->gif = NULL;
    }
    b->img = NULL;
}

void FNTBlob_free_raw(FNTBlob *b) {
    unsigned int i;

    if (b->raw) free(b->raw);
    b->raw_length = 0;
    b->raw = NULL;

    for (i=0;i < (5-1);i++) b->block_ofs[i] = 0;
}

FNTBlob *FNTBlob_free(FNTBlob *b) {
    if (b != NULL) {
        FNTBlob_free_raw(b);
        FNTBlob_free_gif(b);
        free(b);
    }

    return NULL;
}

int FNTBlob_load(FNTBlob *b,const char *path) {
    off_t fsz;
    int fd;

    FNTBlob_free_raw(b);
    b->y_adj = 0;

    fd = open(path,O_RDONLY | O_BINARY);
    if (fd < 0) return -1;
    fsz = lseek(fd,0,SEEK_END);
    if (fsz < 8 || fsz > 32768UL) {
        close(fd);
        return -1;
    }

    lseek(fd,0,SEEK_SET);

    b->raw = malloc(fsz);
    if (b->raw == NULL) {
        close(fd);
        return -1;
    }
    b->raw_length = (size_t)fsz;
    read(fd,b->raw,b->raw_length);
    close(fd);

    /* BMF\x03 */
    if (memcmp(b->raw,"BMF\x03",4)) goto fail;

    /* for each block */
    {
        unsigned char *p = b->raw + 4;
        unsigned char *f = b->raw + b->raw_length;
        unsigned char blocktype;
        uint32_t blocksize;

        while ((p+1+4) <= f) {
            blocktype = *p++;
            blocksize = *((uint32_t*)p); p += 4;

            if (blocktype < 1 || blocktype > 5)
                goto fail;
            if ((p+blocksize) > f)
                goto fail;

            b->block_ofs[blocktype-1] = (size_t)(p - b->raw);
            p += blocksize;
        }
    }

    /* block type 1, 2, and 4 are REQUIRED */
    if (b->block_ofs[1-1] == (size_t)0 ||
        b->block_ofs[2-1] == (size_t)0 ||
        b->block_ofs[4-1] == (size_t)0)
        goto fail;

    return 0;
fail:
    FNTBlob_free_raw(b);
    return -1;
}

FNTBlob*                    font18_fnt = NULL;
FNTBlob*                    font22_fnt = NULL;
FNTBlob*                    font40_fnt = NULL;

int loadFont(FNTBlob **fnt,const char *fnt_path,const char *gif_path) {
    int err;

    if (*fnt == NULL) {
        *fnt = FNTBlob_alloc();
        if (*fnt == NULL) return -1;
    }

    if ((*fnt)->raw == NULL && fnt_path != NULL) {
        DEBUG("Loading font FNT=%s",fnt_path);

        if (FNTBlob_load(*fnt,fnt_path) < 0) {
            DEBUG("Font load FNT=%s failed",fnt_path);
            return -1;
        }

        FNTBlob_ascii_lookup_build(*fnt);
    }

    if ((*fnt)->gif == NULL && gif_path != NULL) {
        DEBUG("Loading font GIF=%s",gif_path);

        (*fnt)->gif = DGifOpenFileName(gif_path,&err);
        if ((*fnt)->gif == NULL) {
            DEBUG("Font load GIF=%s failed",gif_path);
            return -1;
        }

        /* TODO: How do we read only the first image? */
        if (DGifSlurp((*fnt)->gif) != GIF_OK) {
            DEBUG("DGifSlurp failed Error=%u %s",(*fnt)->gif->Error,GifErrorString((*fnt)->gif->Error));
            DGifCloseFile((*fnt)->gif,&err);
            (*fnt)->gif = NULL;
            return -1;
        }
        if ((*fnt)->gif->ImageCount == 0) {
            DEBUG("No GIF images");
            DGifCloseFile((*fnt)->gif,&err);
            (*fnt)->gif = NULL;
            return -1;
        }

        (*fnt)->img = &((*fnt)->gif->SavedImages[0]);
    }

    return 0;
}

void unloadFont(FNTBlob **fnt) {
    if (*fnt != NULL) *fnt = FNTBlob_free(*fnt);
}

/* flag slots */
#define MAX_FLAG_SLOTS      32
typedef uint8_t FlagSlot;

FlagSlot        flag_slots[MAX_FLAG_SLOTS];

/* color palette slots */
#define MAX_PAL_SLOTS       8

#pragma pack(push,1)
typedef struct PalColor {
    uint8_t     r,g,b;          // already scaled to 0-63
} PalColor;
#pragma pack(pop)

typedef struct PalSlot {
    PalColor    pal[256];
} PalSlot;

PalSlot         pal_slots[MAX_PAL_SLOTS];

/* async palette animation */
typedef struct AsyncPal {
    uint8_t     slot;           // which palette slot
    uint8_t     first;          // first color in palette to copy
    uint16_t    count;          // how many colors to copy
    uint8_t     first_target;   // first color in hardware palette to copy
    uint8_t     anim;           // what animation to do
    uint8_t     anim_p[2];      // depends on animation
} AsyncPal;

enum {
    ASYNC_PAL_ANIM_NONE=0,      // no animation, finish right away
    ASYNC_PAL_ANIM_FADE         // fade in/out. anim_p[0] is fade value 0-63. anim_p[1] is fade addition value, 1-63 to fade in, 0xFF-0x80 to fade out.
};

typedef struct AsyncVPan {
    uint16_t    start;          // display offset (given directly to CRTC)
    uint16_t    end;            // stop panning here
    uint16_t    adjust;         // after panning, start += adjust. event ends when start == end
} AsyncVPan;

typedef struct AsyncSetFlag {
    uint8_t     slot;
    uint8_t     value;
} AsyncSetFlag;

/* async event in general to do from vsync timer IRQ */
typedef struct AsyncEvent {
    uint8_t     what;
    union {
        AsyncPal        pal;
        AsyncVPan       vpan;
        uint16_t        wait;
        uint16_t        wait_complete;
        AsyncSetFlag    setflag;
    } e;
} AsyncEvent;

enum {
    ASYNC_WAIT_COMPLETE_PALETTE=0x0001U,
    ASYNC_WAIT_COMPLETE_VPAN=0x0002U
};

enum {
    ASYNC_EVENT_STOP=0,         // stop processing
    ASYNC_EVENT_WAIT,           // wait for vertical retrace counts given in e.wait (e.wait)
    ASYNC_EVENT_PALETTE,        // palette animation (e.pal)
    ASYNC_EVENT_VPAN,           // change display start address (e.vpan)
    ASYNC_EVENT_WAIT_COMPLETE,  // wait for panning or palette animation to complete
    ASYNC_EVENT_SET_FLAG        // set flag
};

#define MAX_ASYNC_EVENT         64

/* async queue, processed by vsync timer.
 * do not add to queue unless interrupts disabled. */
unsigned int    async_event_index = 0;
unsigned int    async_event_write = 0;
AsyncEvent      async_events[MAX_ASYNC_EVENT];

/* allow palette animation while doing another async event at the same time */
unsigned int    async_event_palette_index = ~0U;
/* allow panning while doing another async event */
unsigned int    async_event_vpan_index = ~0U;

/* measurement of VGA refresh rate */
uint16_t vga_refresh_timer_ticks = 0;
uint16_t timer_irq0_chain_add = 0;
uint16_t timer_irq0_chain_counter = 0;

uint32_t timer_irq0_ticks = 0;
uint32_t timer_irq0_ticks18 = 0;
uint32_t timer_irq0_ticksvsync = 0;

void interrupt (*old_timer_irq)() = NULL;

static inline unsigned char fadein_cap(unsigned char fade,unsigned char v) {
    return (v > fade) ? fade : v;
}

void async_flush(void);

/* do not call from interrupt */
AsyncEvent *next_async(void) {
    if (async_event_write >= MAX_ASYNC_EVENT)
        async_flush();
    if (async_event_write >= MAX_ASYNC_EVENT)
        FAIL("Too many async events");

    return &async_events[async_event_write];
}

AsyncEvent *current_active_async(void) {
    return &async_events[async_event_index];
}

unsigned int async_available_slot(void) {
    return (async_event_write < MAX_ASYNC_EVENT);
}

unsigned int async_has_finished(void) {
    return (async_event_index == async_event_write);
}

void next_async_finish(void) {
    async_event_write++;
}

void async_flush(void) {
    unsigned int minx;
    int c;

    _cli();

    if (async_event_index > async_event_write)
        FAIL("Async event index > Async event write");

    minx = async_event_index;
    if (async_event_palette_index != ~0U &&
        minx > async_event_palette_index)
        minx = async_event_palette_index;
    if (async_event_vpan_index != ~0U &&
        minx > async_event_vpan_index)
        minx = async_event_vpan_index;

    c = async_event_write - minx;
    if (c < 0) c = 0;

    if (async_event_palette_index != ~0U)
        async_event_palette_index -= minx;
    if (async_event_vpan_index != ~0U)
        async_event_vpan_index -= minx;

    if (c != 0 && minx != 0)
        memmove(&async_events[0],&async_events[minx],c * sizeof(AsyncEvent));

    async_event_index -= minx;
    async_event_write -= minx;

    _sti();
}

void flush_async(void) {
    _cli();
    async_event_write = async_event_index = 0;
    _sti();
}

void halt_async(void) {
    _cli();
    async_event_write = async_event_index = 0;
    async_event_palette_index = ~0U;
    async_event_vpan_index = ~0U;
    _sti();
}

void do_async_irq_pal(void) {
    // assume index != ~0U and valid
    AsyncEvent *ev = &async_events[async_event_palette_index];
    PalSlot *p = &pal_slots[ev->e.pal.slot];
    PalColor *scp = &(p->pal[ev->e.pal.first]);
    unsigned int c;

    vga_palette_lseek(ev->e.pal.first_target);

    switch (ev->e.pal.anim) {
        case ASYNC_PAL_ANIM_NONE:
            for (c=0;c < ev->e.pal.count;c++,scp++)
                vga_palette_write(scp->r,scp->g,scp->b);
            break;
        case ASYNC_PAL_ANIM_FADE: {
            unsigned char fade = ev->e.pal.anim_p[0];

            for (c=0;c < ev->e.pal.count;c++,scp++)
                vga_palette_write(fadein_cap(fade,scp->r),fadein_cap(fade,scp->g),fadein_cap(fade,scp->b));

            fade = ev->e.pal.anim_p[0] += ev->e.pal.anim_p[1];
            if (fade <= 64) return; // not yet done. we're done when fade < 0 or fade > 63
            } break;
    }

    // done, cancel
    async_event_palette_index = ~0U;
}

void do_async_irq_vpan(void) {
    // assume index != ~0U and valid
    AsyncEvent *ev = &async_events[async_event_vpan_index];

    // reprogram CRTC offset
    vga_set_start_location(ev->e.vpan.start);

    // panning?
    if (ev->e.vpan.start != ev->e.vpan.end) {
        ev->e.vpan.start += ev->e.vpan.adjust;
        return;
    }

    // done, cancel
    async_event_vpan_index = ~0U;
}

void interrupt timer_irq(void) {
    uint16_t padd;

    /* count ticks */
    timer_irq0_ticks++;

    /* vertical retrace */
    timer_irq0_ticksvsync++;
    while (async_event_index < async_event_write) {
        AsyncEvent *ev = &async_events[async_event_index];

        switch (ev->what) {
            case ASYNC_EVENT_STOP:
                async_event_index = async_event_write = 0;
                goto async_end;
            case ASYNC_EVENT_WAIT:
                if (ev->e.wait == 0) async_event_index++;
                else ev->e.wait--;
                goto async_end;
            case ASYNC_EVENT_PALETTE: /* copy the index, allow it to happen while doing another event */
                async_event_palette_index = async_event_index++;
                break;
            case ASYNC_EVENT_VPAN: /* copy the index, allow it to happen while doing another event */
                async_event_vpan_index = async_event_index++;
                break;
            case ASYNC_EVENT_WAIT_COMPLETE:
                if ((ev->e.wait_complete & ASYNC_WAIT_COMPLETE_PALETTE) && async_event_palette_index != ~0U) goto async_end;
                if ((ev->e.wait_complete & ASYNC_WAIT_COMPLETE_VPAN) && async_event_vpan_index != ~0U) goto async_end;
                async_event_index++;
                break;
            case ASYNC_EVENT_SET_FLAG:
                flag_slots[ev->e.setflag.slot] = ev->e.setflag.value;
                async_event_index++;
                break;
            default:
                async_event_index++;
                break;
        }
    }
async_end:

    if (async_event_palette_index != ~0U)
        do_async_irq_pal();
    if (async_event_vpan_index != ~0U)
        do_async_irq_vpan();

    /* chain at 18.2Hz */
    padd = timer_irq0_chain_counter;
    timer_irq0_chain_counter += timer_irq0_chain_add;
    timer_irq0_chain_counter &= 0xFFFFU;
    if (timer_irq0_chain_counter < padd) {
        timer_irq0_ticks18++;
        old_timer_irq();
    }
    else {
		p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
    }
}

void setup_timer(void) {
    if (old_timer_irq != NULL) return;

    _cli();

    write_8254_system_timer(timer_irq0_chain_add);
    old_timer_irq = _dos_getvect(irq2int(0));
    _dos_setvect(irq2int(0),timer_irq);

    _sti();
}

void release_timer(void) {
    if (old_timer_irq == NULL) return;

    _cli();

    write_8254_system_timer(0);
    _dos_setvect(irq2int(0),old_timer_irq);
    old_timer_irq = NULL;

    _sti();
}

uint16_t dos_get_freemem(void) {
    uint16_t r = 0;

    __asm {
        mov     ah,0x48         ; allocate memory
        mov     bx,0xFFFF       ; way beyond actual memory, obviously. we want it to fail.
        int     21h             ; result should be CF=1, AX=8, BX=size of largest block
        mov     r,bx
    }

    return r;
}

uint16_t bios_get_total_mem(void) {
    uint16_t r = 0;

    __asm {
        int     12h
        mov     r,ax
    }

    return r;
}

void DEBUG_PRINT_MEM_STATE(void) {
    uint16_t r = dos_get_freemem();
    uint16_t b = bios_get_total_mem();

    DEBUG("MS-DOS largest available free block: %lu bytes (%uKB)",
        (unsigned long)r << 4UL,(unsigned int)((unsigned long)r >> (10UL - 4UL)));
    DEBUG("Total conventional memory: %lu bytes (%uKB)",
        (unsigned long)b << 10UL,(unsigned int)b);

#if TARGET_MSDOS == 32
    {
        uint32_t buf[0x30/4];
        unsigned char *p=(unsigned char*)buf;

        memset(buf,0,sizeof(buf));

        __asm {
            mov     eax,0x0500      ; DPMI get memory info
            push    ds
            pop     es
            mov     edi,p
            int     31h
        }

        DEBUG("DPMI largest available free block: %lu bytes (%luKB)",
            (unsigned long)buf[0x00/4],
            (unsigned long)buf[0x00/4] >> 10UL);

        DEBUG("DPMI free pages: %lu bytes (%luKB)",
            (unsigned long)buf[0x14/4] << 12UL,
            (unsigned long)buf[0x14/4] << 2UL);

        DEBUG("DPMI total pages: %lu bytes (%luKB)",
            (unsigned long)buf[0x18/4] << 12UL,
            (unsigned long)buf[0x18/4] << 2UL);
    }
#endif

    /* Open Watcom specific */
    {
        struct _heapinfo h_info;

        DEBUG("Walking heap:");
        h_info._pentry = NULL;
        for (;;) {
            if (_heapwalk(&h_info) != _HEAPOK) break;
            DEBUG(" - %s block at %Fp of size %u (0x%X) bytes",
                (h_info._useflag ? "USED" : "FREE"),
                h_info._pentry,h_info._size,h_info._size);
        }
        DEBUG("--Done");
    }
}

void init_debug_log(void) {
    if (debug_log == NULL) {
        debug_log = fopen("debug.txt","w");
        if (debug_log == NULL) return;
        setbuf(debug_log,NULL); /* NO BUFFERING */

        DEBUG("==DEBUG LOG STARTED==");
    }
}

int initial_sys_check(void) {
    /*============================*/
    probe_dos();
    DEBUG("MS-DOS version: %u.%u",dos_version>>8,dos_version&0xFF);
    if (dos_version < 0x500) {
        puts("MS-DOS 5.00 or higher required");
        return 0;
    }
    detect_windows();
    if (windows_mode != WINDOWS_NONE) {
        puts("Running from within Windows is not supported");
        return 0;
    }

    /*============================*/
#if TARGET_MSDOS == 16
    cpu_probe();
    DEBUG("CPU: %s",cpu_basic_level_str[cpu_basic_level]);
    if (cpu_basic_level < CPU_286) {
        puts("286 or higher required");
        return 0;
    }
#endif

    /*============================*/
    if (!k8042_probe()) {
        puts("8042 keyboard not present");
        return 0;
    }

    /*============================*/
	if (!probe_8254()) {
        puts("8254 timer not present");
		return 0;
	}

    /*============================*/
	if (!probe_8259()) {
        puts("8259 interrupt controller not present");
		return 0;
	}

    /*============================*/
    if (!probe_vga()) {
        puts("This game requires VGA or compatible graphics");
		return 0;
    }

    /* NTS: In the future, if needed, 16-bit builds will also make use of EMS/XMS memory.
     *      But that's far off into the future. */
    DEBUG_PRINT_MEM_STATE();

    return 1; /* OK */
}

/* Mode X page flipping/panning support.
 * REMEMBER: A "byte" in mode X is 4 pixels wide because of VGA planar layout.
 * WARNING: Do not set offset beyond such that the screen will exceed 64KB. */
uint16_t modex_vis_offset = 0;
uint16_t modex_draw_offset = 0;
uint16_t modex_draw_width = 0;
uint16_t modex_draw_height = 0;
uint16_t modex_draw_stride = 0; /* in 4-byte pixels NOT pixels */

void modex_init(void) {
    modex_vis_offset = modex_draw_offset = 0;
    modex_draw_width = 320;
    modex_draw_height = 200;
    modex_draw_stride = modex_draw_width / 4U;
}

enum {
    BITBLT_BLOCK=0,
    BITBLT_NOBLACK,
    BITBLT_NOBLACK_BIAS,        // i.e. for fonts

    BITBLT_MAX
};

unsigned char xbitblt_nc_bias = 0;
unsigned char xbitblt_nc_mode = BITBLT_BLOCK;

void xbitblt_nc(unsigned int x,unsigned int y,unsigned int w,unsigned int h,unsigned int bits_stride,unsigned char *bits) {
    uint16_t o = (y * modex_draw_stride) + (x >> 2) + modex_draw_offset;
    unsigned char b = 1U << (x & 3U);
    unsigned char *src;
    unsigned int ch;
    VGA_RAM_PTR wp;

    /* assume w != 0 && h != 0 */
    /* assume x < modex_draw_width && y < modex_draw_height */
    /* assume (x+w) <= modex_draw_width && (y+h) <= modex_draw_height */
    /* assume bits != NULL */
    /* render vertical strip-wise because of Mode X */
    do {
	    vga_write_sequencer(VGA_SC_MAP_MASK,b);
        wp = vga_state.vga_graphics_ram + o;
        src = bits;
        ch = h;

        do {
            *wp = *src;
            wp += modex_draw_stride;
            src += bits_stride;
        } while ((--ch) != 0);

        bits++;
        if ((b <<= 1U) == 0x10U) {
            b = 0x01U;
            o++;
        }
    } while ((--w) != 0);
}

void xbitblt_nc_noblack(unsigned int x,unsigned int y,unsigned int w,unsigned int h,unsigned int bits_stride,unsigned char *bits) {
    uint16_t o = (y * modex_draw_stride) + (x >> 2) + modex_draw_offset;
    unsigned char b = 1U << (x & 3U);
    unsigned char *src;
    unsigned int ch;
    VGA_RAM_PTR wp;

    /* assume w != 0 && h != 0 */
    /* assume x < modex_draw_width && y < modex_draw_height */
    /* assume (x+w) <= modex_draw_width && (y+h) <= modex_draw_height */
    /* assume bits != NULL */
    /* render vertical strip-wise because of Mode X */
    do {
	    vga_write_sequencer(VGA_SC_MAP_MASK,b);
        wp = vga_state.vga_graphics_ram + o;
        src = bits;
        ch = h;

        do {
            if (*src != 0) *wp = *src;
            wp += modex_draw_stride;
            src += bits_stride;
        } while ((--ch) != 0);

        bits++;
        if ((b <<= 1U) == 0x10U) {
            b = 0x01U;
            o++;
        }
    } while ((--w) != 0);
}

void xbitblt_nc_noblack_bias(unsigned int x,unsigned int y,unsigned int w,unsigned int h,unsigned int bits_stride,unsigned char *bits) {
    uint16_t o = (y * modex_draw_stride) + (x >> 2) + modex_draw_offset;
    unsigned char b = 1U << (x & 3U);
    unsigned char *src;
    unsigned int ch;
    VGA_RAM_PTR wp;

    /* assume w != 0 && h != 0 */
    /* assume x < modex_draw_width && y < modex_draw_height */
    /* assume (x+w) <= modex_draw_width && (y+h) <= modex_draw_height */
    /* assume bits != NULL */
    /* render vertical strip-wise because of Mode X */
    do {
	    vga_write_sequencer(VGA_SC_MAP_MASK,b);
        wp = vga_state.vga_graphics_ram + o;
        src = bits;
        ch = h;

        do {
            if (*src != 0) *wp = *src + xbitblt_nc_bias;
            wp += modex_draw_stride;
            src += bits_stride;
        } while ((--ch) != 0);

        bits++;
        if ((b <<= 1U) == 0x10U) {
            b = 0x01U;
            o++;
        }
    } while ((--w) != 0);
}

typedef void (*xbitblt_nc_func)(unsigned int x,unsigned int y,unsigned int w,unsigned int h,unsigned int bits_stride,unsigned char *bits);

static xbitblt_nc_func xbitblt_nc_functable[BITBLT_MAX] = {
    xbitblt_nc,                     // BLOCK
    xbitblt_nc_noblack,             // NOBLACK
    xbitblt_nc_noblack_bias         // NOBLACK_BIAS
};

void xbitblt_setbltmode(unsigned char mode) {
    xbitblt_nc_mode = mode;
}

void xbitblt_setbltbias(unsigned char bias) {
    xbitblt_nc_bias = bias;
}

void xbitblts(int x,int y,int w,int h,unsigned int stride,unsigned char *bits) {
    /* NTS: We render in Mode X at all times */
    /* Assume: bits != NULL */
    if (x < 0) { bits += -x;          w += x; x = 0; }
    if (y < 0) { bits += -y * stride; h += y; y = 0; }
    if (w <= 0 || h <= 0) return;
    if (((uint16_t)(x+w)) > modex_draw_width) w = modex_draw_width - x;
    if (((uint16_t)(y+h)) > modex_draw_height) h = modex_draw_height - y;
    if (w <= 0 || h <= 0) return;

    xbitblt_nc_functable[xbitblt_nc_mode]((unsigned int)x,(unsigned int)y,(unsigned int)w,(unsigned int)h,stride,bits);
}

void xbitblt(int x,int y,int w,int h,unsigned char *bits) {
    xbitblts(x,y,w,h,(unsigned int)w,bits); // stride == w
}

void font_bitblt(FNTBlob *b,int cx/*left edge of box*/,int *x,int *y,uint32_t id) {
    if (b->img == NULL || b->gif == NULL)
        return;

    if (id == '\n') {
        FNTBlob_common *c = FNTBlob_get_common(b);
        if (c == NULL) return;

        *x = cx;
        *y += c->lineHeight;
    }
    else {
        unsigned char *rasterptr;
        FNTBlob_chars *fc = FNTBlob_find_char(b,id);
        if (fc == NULL) return;

        rasterptr = b->img->RasterBits;
        if (rasterptr == NULL) return;
        rasterptr += fc->y * b->img->ImageDesc.Width;
        rasterptr += fc->x;

        xbitblts(*x + fc->xoffset,*y + fc->yoffset + b->y_adj,fc->w,fc->h,b->img->ImageDesc.Width,rasterptr);

        *x += fc->xadvance;
    }
}

/* fill x1 <= x <= x2 and y1 <= y <= y2 */
void xbltbox(int x1,int y1,int x2,int y2,unsigned char color) { // filled rectangle, inclusive
    unsigned int y,xrem,xs;
    VGA_RAM_PTR cwp,wp;
    unsigned char b;

    if (x1 >= (int)modex_draw_width || y1 >= (int)modex_draw_height) return;
    if (x2 < 0 || y2 < 0) return;

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= modex_draw_width) x2 = modex_draw_width - 1;
    if (y2 >= modex_draw_height) y2 = modex_draw_height - 1;

    /* assume x1 <= x2 */
    /* assume y1 <= y2 */

    /* column-wise rendering, VGA Mode X optimal */
    {
        uint16_t co;

        co = modex_draw_offset + ((unsigned int)y1 * modex_draw_stride) + (((unsigned int)x1) >> 2);
        wp = vga_state.vga_graphics_ram + co;
    }

    xs = x2 + 1 - x1;
    do {
        xrem = (xs & 0xFFFC) ? (4 - (x1 & 3)) : xs;

        b = ((1 << xrem) - 1) << ((unsigned int)x1 & 3);
	    vga_write_sequencer(VGA_SC_MAP_MASK,b);

        /* draw the column */
        for (y=y1,cwp=wp;y <= y2;y++,cwp+=modex_draw_stride) *cwp = color;

        /* advance */
        xs -= xrem;
        if (xs == 0) break;
        x1 = 0;
        wp++;
    } while (1);

    /* left edge */
}

/* draw border x == x1 || x == x2 || y == y1 || y == y2 */
void xbltrect(int x1,int y1,int x2,int y2,unsigned char color) { // rectangle border, inclusive
    xbltbox(x1,y1,x1,y2,color);     // left
    xbltbox(x2,y1,x2,y2,color);     // right
    xbltbox(x1+1,y1,x2-1,y1,color); // top
    xbltbox(x1+1,y2,x2-1,y2,color); // bottom
}

enum {
	UTF8ERR_INVALID=-1L,
	UTF8ERR_NO_ROOM=-2L
};

#ifndef UNICODE_BOM
#define UNICODE_BOM 0xFEFF
#endif

long utf8_decode(const char **ptr,const char *fence) {
	const char *p = *ptr;
	int uchar_size=1;
	long ret = 0,c;

	if (!p) return UTF8ERR_NO_ROOM;
	if (p >= fence) return UTF8ERR_NO_ROOM;

	ret = (unsigned char)(*p);
	if (ret >= 0xFE) { p++; return UTF8ERR_INVALID; }
	else if (ret >= 0xFC) uchar_size=6;
	else if (ret >= 0xF8) uchar_size=5;
	else if (ret >= 0xF0) uchar_size=4;
	else if (ret >= 0xE0) uchar_size=3;
	else if (ret >= 0xC0) uchar_size=2;
	else if (ret >= 0x80) { p++; return UTF8ERR_INVALID; }

	if ((p+uchar_size) > fence)
		return UTF8ERR_NO_ROOM;

	switch (uchar_size) {
		case 1:	p++;
			break;
		case 2:	ret = (ret&0x1F)<<6; p++;
			c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
			ret |= c&0x3F;
			break;
		case 3:	ret = (ret&0xF)<<12; p++;
			c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
			ret |= (c&0x3F)<<6;
			c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
			ret |= c&0x3F;
			break;
		case 4:	ret = (ret&0x7)<<18; p++;
			c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
			ret |= (c&0x3F)<<12;
			c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
			ret |= (c&0x3F)<<6;
			c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
			ret |= c&0x3F;
			break;
		case 5:	ret = (ret&0x3)<<24; p++;
			c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
			ret |= (c&0x3F)<<18;
			c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
			ret |= (c&0x3F)<<12;
			c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
			ret |= (c&0x3F)<<6;
			c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
			ret |= c&0x3F;
			break;
		case 6:	ret = (ret&0x1)<<30; p++;
			c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
			ret |= (c&0x3F)<<24;
			c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
			ret |= (c&0x3F)<<18;
			c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
			ret |= (c&0x3F)<<12;
			c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
			ret |= (c&0x3F)<<6;
			c = (unsigned char)(*p++); if ((c&0xC0) != 0x80) return UTF8ERR_INVALID;
			ret |= c&0x3F;
			break;
	};

	*ptr = p;
	return ret;
}

void font_str_bitblt(FNTBlob *b,int cx,int *x,int *y,const char *str/*NTS: Will be UTF-8*/) {
    long id;

    while (*str != 0) {
        id = utf8_decode(&str,str+32);
        if (id < 0L) break;
        font_bitblt(b,cx,x,y,(uint32_t)id);
    }
}

void font_str_bitblt_center(FNTBlob *b,int cx,int center_x,const int top_y,const char *str/*NTS: Will be UTF-8*/) {
    int x,y = top_y;
    long id;

    {
        const char *scan = str;
        int w = 0,rw = 0;

        while (*scan != 0) {
            id = utf8_decode(&scan,scan+32);
            if (id < 0L) break;

            {
                FNTBlob_chars *fc = FNTBlob_find_char(b,id);
                if (fc != NULL) {
                    rw = w + fc->w;
                    w += fc->xadvance;
                }
            }
        }

        if (w < rw) w = rw;
        x = center_x - (w / 2);
    }

    while (*str != 0) {
        id = utf8_decode(&str,str+32);
        if (id < 0L) break;
        font_bitblt(b,center_x,&x,&y,(uint32_t)id);
    }
}

GifFileType *FreeGIF(GifFileType *gif) {
    int err;

    if (gif) {
        if (DGifCloseFile(gif,&err) != GIF_OK)
            DEBUG("DGifCloseFile failed, err=%d %s",err,GifErrorString(err));
    }

    return NULL;
}

GifFileType *LoadGIF(const char *path) {
    GifFileType *gif;
    int err;

    DEBUG("DisplayGIF: %s",path);

    gif = DGifOpenFileName(path,&err);
    if (gif == NULL) {
        DEBUG("DGifOpenFileName failed, err=%d %s",err,GifErrorString(gif->Error));
        return NULL;
    }

    /* TODO: How do we read only the first image? */
    if (DGifSlurp(gif) != GIF_OK) {
        DEBUG("DGifSlurp failed Error=%u %s",gif->Error,GifErrorString(gif->Error));
        DGifCloseFile(gif,&err);
        return NULL;
    }
    if (gif->ImageCount == 0) {
        DEBUG("No GIF images");
        DGifCloseFile(gif,&err);
        return NULL;
    }

    return gif;
}

/* NTS: Does NOT load the color palette */
void DrawGIF(int x,int y,GifFileType *gif,unsigned int index) {
    if (gif == NULL) return;
    if (index >= gif->ImageCount) return;

    {
        SavedImage *img = &gif->SavedImages[index];

        if (img->RasterBits == NULL) return;
        xbitblt(img->ImageDesc.Left+x,img->ImageDesc.Top+y,img->ImageDesc.Width,img->ImageDesc.Height,img->RasterBits);
    }
}

/* load GIF global palette into palette slot for async palette load */
void GIF_GlobalColorTableToPaletteSlot(unsigned int slot,GifFileType *gif) {
    if (gif == NULL) return;
    if (slot >= MAX_PAL_SLOTS) return;
    if (gif->SColorMap == NULL) return;

    {
        ColorMapObject *c = gif->SColorMap;
        GifColorType *color = c->Colors;
        unsigned int i;

        if (c->Colors == 0) return;

        for (i=0;i < c->ColorCount;i++) {
            pal_slots[slot].pal[i].r = color[i].Red >> 2;
            pal_slots[slot].pal[i].g = color[i].Green >> 2;
            pal_slots[slot].pal[i].b = color[i].Blue >> 2;
        }
    }
}

void load_all_fonts(void) {
    /* load fonts */
    loadFont(&font18_fnt,"font18.fnt","font18.gif");
    loadFont(&font22_fnt,"font22.fnt","font22.gif");
    loadFont(&font40_fnt,"font40.fnt","font40.gif");
}

void font_prep_palette_at(const unsigned char palidx,
    const unsigned char bg_r,const unsigned char bg_g,const unsigned char bg_b,
    const unsigned char fg_r,const unsigned char fg_g,const unsigned char fg_b) {
    unsigned char r,g,b;
    unsigned int i;

    vga_palette_lseek(palidx);
    for (i=0;i < 4;i++) {
        r = bg_r + ((((char)fg_r - (char)bg_r) * i) / 3);
        g = bg_g + ((((char)fg_g - (char)bg_g) * i) / 3);
        b = bg_b + ((((char)fg_b - (char)bg_b) * i) / 3);
        vga_palette_write(r>>2,g>>2,b>>2);
    }
}

void font_prep_palette_slot_at(const unsigned char slot,unsigned char palidx,
    const unsigned char bg_r,const unsigned char bg_g,const unsigned char bg_b,
    const unsigned char fg_r,const unsigned char fg_g,const unsigned char fg_b) {
    unsigned char r,g,b;
    unsigned int i;

    for (i=0;i < 4;i++) {
        r = bg_r + ((((char)fg_r - (char)bg_r) * i) / 3);
        g = bg_g + ((((char)fg_g - (char)bg_g) * i) / 3);
        b = bg_b + ((((char)fg_b - (char)bg_b) * i) / 3);

        pal_slots[slot].pal[palidx].r = r >> 2;
        pal_slots[slot].pal[palidx].g = g >> 2;
        pal_slots[slot].pal[palidx].b = b >> 2;
        palidx++;
    }
}

void font_prep_xbitblt_at(unsigned char palidx) {
    xbitblt_setbltbias(palidx);
    xbitblt_setbltmode(BITBLT_NOBLACK_BIAS);
}

void vga_refresh_rate_measure(void) {
    /* WARNING: This code assumes the refresh rate is >= 20Hz */
    unsigned long counter = 0;
    unsigned int frames;
    t8254_time_t pt,ct;

    vga_refresh_timer_ticks = (uint16_t)(T8254_REF_CLOCK_HZ / 70UL); /* reasonable default */

    DEBUG("VGA: measuring refresh rate");

    // Need to reprogram timer to normal 18.2Hz full range for this test
    _cli();
    write_8254_system_timer(0);
    vga_wait_for_vsync();
    vga_wait_for_vsync_end();
    ct = read_8254_ncli(0);
    for (frames=0;frames < 60;frames++) {
        vga_wait_for_vsync();
        vga_wait_for_vsync_end();

        pt = ct;
        ct = read_8254_ncli(0);

        counter += (unsigned long)((pt - ct) & 0xFFFFU); // NTS: remember, it counts DOWN
    }
    _sti();
    DEBUG("VGA measurement: %u frames, %lu ticks",
        frames,counter);

    vga_refresh_timer_ticks = (uint16_t)(counter / (unsigned long)frames);

    DEBUG("VGA refresh rate: %u ticks (%.3ffps)",
        vga_refresh_timer_ticks,
        (double)T8254_REF_CLOCK_HZ / (double)vga_refresh_timer_ticks);
}

void timer_sync_to_vrefresh(void) {
    _cli();
    /* make the IRQ 0 timer count to zero and stop */
	write_8254(T8254_TIMER_INTERRUPT_TICK,16,T8254_MODE_1_HARDWARE_RETRIGGERABLE_ONE_SHOT);
    /* wait for vsync */
    vga_wait_for_vsync();
    vga_wait_for_vsync_end();
    vga_wait_for_vsync();
    /* start it again. */
    write_8254_system_timer(timer_irq0_chain_add);
    _sti();
}

void blank_vga_palette(void) {
    unsigned int i;

    vga_palette_lseek(0);
    for (i=0;i < 256;i++) vga_palette_write(0,0,0);
}

void TitleSequenceAsyncScheduleSlide(unsigned char slot,uint16_t offset,uint8_t flag) {
    AsyncEvent *ev;

    ev = next_async();
    memset(ev,0,sizeof(*ev));
    ev->what = ASYNC_EVENT_VPAN;
    ev->e.vpan.start = ev->e.vpan.end = offset;
    ev->e.vpan.adjust = 0;
    next_async_finish();

    ev = next_async();
    memset(ev,0,sizeof(*ev));
    ev->what = ASYNC_EVENT_PALETTE;
    ev->e.pal.first = ev->e.pal.first_target = 0;
    ev->e.pal.count = 256;
    ev->e.pal.slot = slot;
    ev->e.pal.anim = ASYNC_PAL_ANIM_FADE;
    ev->e.pal.anim_p[0] = 2;
    ev->e.pal.anim_p[1] = 2;
    next_async_finish();

    ev = next_async();
    memset(ev,0,sizeof(*ev));
    ev->what = ASYNC_EVENT_WAIT;
    ev->e.wait = 70 * 4; // 4 seconds
    next_async_finish();

    ev = next_async();
    memset(ev,0,sizeof(*ev));
    ev->what = ASYNC_EVENT_PALETTE;
    ev->e.pal.first = ev->e.pal.first_target = 0;
    ev->e.pal.count = 256;
    ev->e.pal.slot = slot;
    ev->e.pal.anim = ASYNC_PAL_ANIM_FADE;
    ev->e.pal.anim_p[0] = 60;
    ev->e.pal.anim_p[1] = -4;
    next_async_finish();

    ev = next_async();
    memset(ev,0,sizeof(*ev));
    ev->what = ASYNC_EVENT_WAIT_COMPLETE;
    ev->e.wait_complete = ASYNC_WAIT_COMPLETE_PALETTE | ASYNC_WAIT_COMPLETE_VPAN;
    next_async_finish();

    ev = next_async();
    memset(ev,0,sizeof(*ev));
    ev->what = ASYNC_EVENT_SET_FLAG;
    ev->e.setflag.slot = 0;
    ev->e.setflag.value = flag;
    next_async_finish();
}

void TitleSequence(void) {
    unsigned char hurry=0;
    GifFileType *gif;
    int c;

    halt_async();
    blank_vga_palette();
    modex_init();
    timer_sync_to_vrefresh(); /* make sure the timer tick happens at vsync */

    /* we use flag slot 0 for title sequence steps. reset now */
    flag_slots[0] = 0;

    /* take title1.gif, load to 0x0000 on screen, palette slot 0 */
    modex_draw_offset = 0;
    gif = LoadGIF("title1.gif");
    GIF_GlobalColorTableToPaletteSlot(0,gif);
    DrawGIF(0,0,gif,0);
    FreeGIF(gif);

    TitleSequenceAsyncScheduleSlide(/*slot*/0,/*offset*/modex_draw_offset,'0');

    if (kbhit()) {
        c = getch();
        if (c == 27) goto user_abort;
        else if (c == 13 || c == ' ') hurry = 1;
    }

    /* take title2.gif, load to 0x4000 on screen, palette slot 1 */
    modex_draw_offset = 0x4000;
    gif = LoadGIF("title2.gif");
    GIF_GlobalColorTableToPaletteSlot(1,gif);
    DrawGIF(0,0,gif,0);
    FreeGIF(gif);

    TitleSequenceAsyncScheduleSlide(/*slot*/1,/*offset*/modex_draw_offset,'1');

    if (kbhit()) {
        c = getch();
        if (c == 27) goto user_abort;
        else if (c == 13 || c == ' ') hurry = 1;
    }

    /* take title3.gif, load to 0x8000 on screen, palette slot 2 */
    modex_draw_offset = 0x8000;
    gif = LoadGIF("title3.gif");
    GIF_GlobalColorTableToPaletteSlot(2,gif);
    DrawGIF(0,0,gif,0);
    FreeGIF(gif);

    TitleSequenceAsyncScheduleSlide(/*slot*/2,/*offset*/modex_draw_offset,'2');

    if (kbhit()) {
        c = getch();
        if (c == 27) goto user_abort;
        else if (c == 13 || c == ' ') hurry = 1;
    }

    /* while we're waiting, now would be a good time to load fonts */
    load_all_fonts();

    /* let it play out, then exit when done */
    do {
        if (kbhit()) {
            c = getch();
            if (c == 27) break;
            else if (c == 13 || c == ' ') hurry = 1;
        }

        if (hurry) {
            _cli();
            if (!async_has_finished()) {
                /* WARNING: current_active_async() will not return valid event if all finished */
                AsyncEvent *ev = current_active_async();

                if (ev->what == ASYNC_EVENT_WAIT) {
                    ev->e.wait = 0; /* hurry up */
                    hurry = 0;
                }
            }
            _sti();
        }
    } while (!async_has_finished());

user_abort:
    halt_async();
}

typedef void (*menu_cmd_t)(void);

enum {
    MENU_CMD_EXIT=0,                            // 0

    MENU_CMD_MAX
};

void menu_cmd_exit(void) {
    DEBUG("menu_cmd_exit");

    game_running_state_push();
    game_running_state_set(GAME_EXIT);
}

menu_cmd_t menu_command_func[MENU_CMD_MAX] = {
    menu_cmd_exit                               // 0
};

typedef struct menu_item {
    struct {
        unsigned int        disabled:1;
        unsigned int        hilighted:1;
    } f;

    int16_t                 command;        /* index into callback */
    const char*             text;

    struct {
        int                 x,y;            /* screen coordinates, y-adjusted by scroll */
        unsigned int        w,h;            /* width and height, not including x+w and y+h */
    } p;
} menu_item;

menu_item main_menu[] = {
    {
        {/*f*/
            0, /* disabled */
            0  /* hilighted */
        },
        -1,     /* command */
        "Hello" /* text */
    },
    {
        {/*f*/
            0, /* disabled */
            0  /* hilighted */
        },
        -1,     /* command */
        "World" /* text */
    },
    {
        {/*f*/
            1, /* disabled */
            0  /* hilighted */
        },
        -1,     /* command */
        "Can't touch this" /* text */
    },
    {
        {/*f*/
            1, /* disabled */
            0  /* hilighted */
        },
        -1,     /* command */
        "Help me" /* text */
    },
    {
        {/*f*/
            0, /* disabled */
            0  /* hilighted */
        },
        -1,     /* command */
        "Quit" /* text */
    },
    {
        {/*f*/
            0, /* disabled */
            0  /* hilighted */
        },
        MENU_CMD_EXIT, /* command */
        "Exit" /* text */
    },
    {
        {/*f*/
            0, /* disabled */
            0  /* hilighted */
        },
        -1,     /* command */
        "There are" /* text */
    },
    {
        {/*f*/
            0, /* disabled */
            0  /* hilighted */
        },
        -1,     /* command */
        "Deliberately" /* text */
    },
    {
        {/*f*/
            0, /* disabled */
            0  /* hilighted */
        },
        -1,     /* command */
        "Too many" /* text */
    },
    {
        {/*f*/
            0, /* disabled */
            0  /* hilighted */
        },
        -1,     /* command */
        "Items here" /* text */
    },
    {
        {/*f*/
            0, /* disabled */
            0  /* hilighted */
        },
        -1,     /* command */
        "To test" /* text */
    },
    {
        {/*f*/
            0, /* disabled */
            0  /* hilighted */
        },
        -1,     /* command */
        "This code" /* text */
    },
    {
        { 0 },
        -1,     /* command */
        NULL
    }
};

void menu_item_layout(menu_item *m,int left_x,int right_x,int top_y,int *items) {
    FNTBlob_common *com = FNTBlob_get_common(font22_fnt);
    unsigned int i = 0;

    if (left_x >= right_x || com == NULL)
        return;

    for (;m->text != NULL;m++,i++) {
        m->p.x = left_x;
        m->p.w = right_x + 1 - left_x;
        m->p.y = top_y;
        m->p.h = com->lineHeight;

        top_y += com->lineHeight;
    }

    if (items != NULL)
        *items = i;
}

#define FIND_ENABLED_WRAP       1U

void menu_item_find_enabled_item(menu_item *m,int items,int *sel,int step,unsigned int flags) {
    do {
        menu_item *s = m + *sel;

        if (s->f.disabled)
            *sel += step;

        if ((*sel) >= items) {
            if (flags & FIND_ENABLED_WRAP)
                *sel = 0;
            else {
                /* search the other way */
                *sel = items - 1;
                step = -step;
            }
        }
        else if ((*sel) < 0) {
            if (flags & FIND_ENABLED_WRAP)
                *sel = items - 1;
            else {
                /* search the other way */
                step = -step;
                *sel = 0;
            }
        }

        if (!s->f.disabled)
            break;
    } while (1);
}

#define SCROLL_TO_ITEM_ONELINE      1U

void menu_item_scroll_to_item(menu_item *m,int items,int select,int menu_h,int *scroll,unsigned int flags) {
    int adj;

    if (select < 0 || select >= items)
        return;

    m += select;

    if (m->p.y < *scroll) {
        if (flags & SCROLL_TO_ITEM_ONELINE)
            (*scroll)--;
        else
            *scroll = m->p.y;
    }
    else {
        adj = (m->p.y + m->p.h - *scroll) - menu_h;
        if (flags & SCROLL_TO_ITEM_ONELINE) {
            if (adj > 0) (*scroll)++;
        }
        else {
            if (adj > 0) *scroll += adj;
        }
    }
}

void xbltcopyfxr_nc(unsigned int x,unsigned int y,unsigned int w,unsigned int h,uint16_t src_vo,uint16_t src_stride) {
    uint16_t dst_vo = modex_draw_offset + (y * modex_draw_stride) + (x >> 2U);

    /* VGA block copy (write mode 1) */
    w = (w + 3U) >> 2U;
    vga_setup_wm1_block_copy();
    dst_vo += modex_draw_stride * (h - 1);
    src_vo += src_stride * (h - 1);
    do {
        vga_wm1_mem_block_copy(dst_vo,src_vo,w);
        dst_vo -= modex_draw_stride;
        src_vo -= src_stride;
    } while ((--h) != 0);
    /* must restore Write Mode 0/Read Mode 0 for this code to continue drawing normally */
    vga_restore_rm0wm0();
}

void xbltcopyfx_nc(unsigned int x,unsigned int y,unsigned int w,unsigned int h,uint16_t src_vo,uint16_t src_stride) {
    uint16_t dst_vo = modex_draw_offset + (y * modex_draw_stride) + (x >> 2U);

    /* VGA block copy (write mode 1) */
    w = (w + 3U) >> 2U;
    vga_setup_wm1_block_copy();
    do {
        vga_wm1_mem_block_copy(dst_vo,src_vo,w);
        dst_vo += modex_draw_stride;
        src_vo += src_stride;
    } while ((--h) != 0);
    /* must restore Write Mode 0/Read Mode 0 for this code to continue drawing normally */
    vga_restore_rm0wm0();
}

/* fast Mode-X VGA RAM to VGA RAM bitblt */
void xbltcopyfxr(int x,int y,int w,int h,uint16_t src_vo,uint16_t src_stride) {
    /* assume: src_stride is in VGA Mode X bytes (groups of 4 pixels) */
    /* assume: x is 4 pixel aligned (x & 3) == 0 */
    /* assume: w is 4 pixel aligned (w & 3) == 0 */
    /* NTS: We render in Mode X at all times */
    /* Assume: bits != NULL */
    if (x < 0) { src_vo += -x >> 2;         w += x; x = 0; }
    if (y < 0) { src_vo += -y * src_stride; h += y; y = 0; }
    if (w <= 0 || h <= 0) return;
    if (((uint16_t)(x+w)) > modex_draw_width) w = modex_draw_width - x;
    if (((uint16_t)(y+h)) > modex_draw_height) h = modex_draw_height - y;
    if (w <= 0 || h <= 0) return;

    xbltcopyfxr_nc(x,y,w,h,src_vo,src_stride);
}

/* fast Mode-X VGA RAM to VGA RAM bitblt */
void xbltcopyfx(int x,int y,int w,int h,uint16_t src_vo,uint16_t src_stride) {
    /* assume: src_stride is in VGA Mode X bytes (groups of 4 pixels) */
    /* assume: x is 4 pixel aligned (x & 3) == 0 */
    /* assume: w is 4 pixel aligned (w & 3) == 0 */
    /* NTS: We render in Mode X at all times */
    /* Assume: bits != NULL */
    if (x < 0) { src_vo += -x >> 2;         w += x; x = 0; }
    if (y < 0) { src_vo += -y * src_stride; h += y; y = 0; }
    if (w <= 0 || h <= 0) return;
    if (((uint16_t)(x+w)) > modex_draw_width) w = modex_draw_width - x;
    if (((uint16_t)(y+h)) > modex_draw_height) h = modex_draw_height - y;
    if (w <= 0 || h <= 0) return;

    xbltcopyfx_nc(x,y,w,h,src_vo,src_stride);
}

void MenuPhaseDrawItemRender(menu_item *m,unsigned int menu_top_offset,const unsigned char menu_h,unsigned int tmp_offset,int menuScroll) {
    const unsigned char brown_title_base = 1; /* 7-color gradient start */
    const unsigned char font_base_white_on_brown = 15; /* 4-color gradient */
    const unsigned char font_base_brown_on_black = 23; /* 4-color gradient */
    const unsigned char font_base_gray_on_black = 27; /* 4-color gradient */
 
    /* menu item coordinates are relative to menu_top_offset on the screen */
    modex_draw_offset = tmp_offset;
    modex_draw_width = (m->p.w + 3) & ~3;
    modex_draw_height = menu_h;
    modex_draw_stride = modex_draw_width / 4U;

    xbltbox(
        0,0,
        m->p.w - 1,m->p.h - 1,
        m->f.hilighted ? (brown_title_base+6) : 0);

    if (m->f.hilighted)
        font_prep_xbitblt_at(font_base_white_on_brown);
    else if (m->f.disabled)
        font_prep_xbitblt_at(font_base_brown_on_black);
    else
        font_prep_xbitblt_at(font_base_gray_on_black);

    font_str_bitblt_center(font22_fnt,0,m->p.w/2U,0,m->text);
}
 
void MenuPhaseDrawItemBlit(menu_item *m,unsigned int menu_top_offset,const unsigned char menu_h,unsigned int tmp_offset,int menuScroll) {
    unsigned int src_stride;
 
    /* menu item coordinates are relative to menu_top_offset on the screen */
    modex_draw_width = (m->p.w + 3) & ~3;
    modex_draw_stride = src_stride = modex_draw_width / 4U;

    /* menu item coordinates are relative to menu_top_offset on the screen */
    modex_draw_offset = menu_top_offset;
    modex_draw_width = 320;
    modex_draw_height = menu_h;
    modex_draw_stride = 320U / 4U;

    xbltcopyfx(
        /* dest */
        m->p.x,
        m->p.y - menuScroll,
        m->p.w,
        m->p.h,
        /* src */
        tmp_offset,
        src_stride);
}

void MenuPhaseDrawItem(menu_item *m,unsigned int menu_top_offset,const unsigned char menu_h,unsigned int tmp_offset,int menuScroll) {
    MenuPhaseDrawItemRender(m,menu_top_offset,menu_h,tmp_offset,menuScroll);
    MenuPhaseDrawItemBlit(m,menu_top_offset,menu_h,tmp_offset,menuScroll);
}

void MenuPhase(void) {
    _Bool menu_init = 0,menu_transition = 1,fullredraw = 1,redraw = 1,running = 1,exiting = 0,userctrl = 0;
    _Bool scheduled_fadein = 0;
    signed char scrollredraw = 0;
    const unsigned char menuScrollAccelTable[] = { 0, 1, 3, 6, 10, 14, 19 }; /* per unit of 10 pixels */
    const unsigned char brown_title_box[3] = { 142U, 78U, 0U }; /* shit brown */
    const unsigned char brown_title_base = 1; /* 7-color gradient start */
    const unsigned char yellow_title_box[3] = { 255U, 251U, 68U }; /* piss yellow */
    const unsigned char yellow_title_base = 8; /* 7-color gradient start */
    const unsigned char font_base_white_on_brown = 15; /* 4-color gradient */
    const unsigned char font_base_yellow_on_brown = 19; /* 4-color gradient */
    const unsigned char font_base_brown_on_black = 23; /* 4-color gradient */
    const unsigned char font_base_gray_on_black = 27; /* 4-color gradient */
    const unsigned char menu_left = 80,menu_right = 239;
    const unsigned char menu_top[2] = { 61, 181 };
    const unsigned int title_y[2] = {0,60};
    const unsigned int title_text_x = 160/*center pt*/, title_text_y = 4, subtitle_text_y = title_text_y + 28;
    const unsigned int menu_top_offset = ((320/4)*menu_top[0]);
    const unsigned int tmp_offset = ((320/4)*200);
    menu_item *menuList = main_menu;
    uint32_t prev_vsync_tick = 0;
    int scrollingSpeed = 0;
    int scrollingItem = 0;
    int scrollingTo = 0;
    int menu_items = 0;
    int menuScroll = 0;
    int menuItem = 0;
    AsyncEvent *ev;
    int c;

    /* load fonts, if not resident in memory */
    load_all_fonts();

    /* if other phases left async animations running, halt them now */
    halt_async();

    /* figure out each menu item's place on the screen */
    menu_item_layout(menuList,menu_left,menu_right,0,&menu_items);
    menu_item_find_enabled_item(menuList,menu_items,&menuItem,1/*step forward*/,0);
    menu_item_scroll_to_item(menuList,menu_items,menuItem,menu_top[1] + 1 - menu_top[0],&menuScroll,0);
    if (menuItem >= 0) menuList[menuItem].f.hilighted = 1;

    /* init palette */
    {
        unsigned int i,j;

        i = 0; /* background, black */
        pal_slots[0].pal[i].r =
        pal_slots[0].pal[i].g =
        pal_slots[0].pal[i].b = 0;

        i = brown_title_base;
        for (j=0;j < 7;j++) {
            pal_slots[0].pal[i+j].r = ((unsigned int)brown_title_box[0] * (j+1U)) >> (3U+2U);
            pal_slots[0].pal[i+j].g = ((unsigned int)brown_title_box[1] * (j+1U)) >> (3U+2U);
            pal_slots[0].pal[i+j].b = ((unsigned int)brown_title_box[2] * (j+1U)) >> (3U+2U);
        }

        i = yellow_title_base;
        for (j=0;j < 7;j++) {
            pal_slots[0].pal[i+j].r = ((unsigned int)yellow_title_box[0] * (j+1U)) >> (3U+2U);
            pal_slots[0].pal[i+j].g = ((unsigned int)yellow_title_box[1] * (j+1U)) >> (3U+2U);
            pal_slots[0].pal[i+j].b = ((unsigned int)yellow_title_box[2] * (j+1U)) >> (3U+2U);
        }

        font_prep_palette_slot_at(/*slot*/0,font_base_white_on_brown,
            /* background */brown_title_box[0],brown_title_box[1],brown_title_box[2],
            /* foreground */255,255,255);

        font_prep_palette_slot_at(/*slot*/0,font_base_yellow_on_brown,
            /* background */brown_title_box[0],brown_title_box[1],brown_title_box[2],
            /* foreground */yellow_title_box[0],yellow_title_box[1],yellow_title_box[2]);

        font_prep_palette_slot_at(/*slot*/0,font_base_brown_on_black,
            /* background */0,0,0,
            /* foreground */brown_title_box[0],brown_title_box[1],brown_title_box[2]);

        font_prep_palette_slot_at(/*slot*/0,font_base_gray_on_black,
            /* background */0,0,0,
            /* foreground */160,160,160);
    }

    /* menu loop */
    modex_init();
    do {
        /* menu transition and fullredraw:
         *   leave fullredraw set, leave transition set. if menu was inited, fade out and wait for fade out. blank screen, vpan to 0.
         *   code after block will detect fullredraw == 1, redraw all, clear flag. clear user control flag.
         *
         * menu transition and !fullredraw:
         *   clear transition. schedule fade in. */
        if (menu_transition) {
            if (!fullredraw) {
                fullredraw = 1;
                userctrl = 0;

                halt_async();

                if (menu_init) {
                    /* schedule fade out */
                    ev = next_async();
                    memset(ev,0,sizeof(*ev));
                    ev->what = ASYNC_EVENT_PALETTE;
                    ev->e.pal.first = ev->e.pal.first_target = 0;
                    ev->e.pal.count = 256;
                    ev->e.pal.slot = 0; /* slot 0 */
                    ev->e.pal.anim = ASYNC_PAL_ANIM_FADE;
                    ev->e.pal.anim_p[0] = 60;
                    ev->e.pal.anim_p[1] = -4;
                    next_async_finish();

                    ev = next_async();
                    memset(ev,0,sizeof(*ev));
                    ev->what = ASYNC_EVENT_WAIT_COMPLETE;
                    ev->e.wait_complete = ASYNC_WAIT_COMPLETE_PALETTE;
                    next_async_finish();

                    menu_init = 0;

                    while (!async_has_finished());
                }
            }
            else {
                userctrl = 1;
                menu_transition = 0;

                halt_async();

                /* blank the screen, pan to 0x0000 */
                blank_vga_palette();
                vga_set_start_location(0);

                scheduled_fadein = 1;
            }
        }

        if (fullredraw) {
            unsigned int i;

            modex_init();

            xbltbox(0,0,319,199,0); // black background

            xbltbox(0,title_y[0]+6,319,title_y[1]-6,brown_title_base+6/*brighest tone*/);
            for (i=0;i < 6;i++) xbltrect(-1,title_y[0]+i,319+1,title_y[1]-i,brown_title_base+i);

            font_prep_xbitblt_at(font_base_yellow_on_brown);
            font_str_bitblt_center(font40_fnt,0,title_text_x,title_text_y,"Shit Man\n");

            font_prep_xbitblt_at(font_base_white_on_brown);
            font_str_bitblt_center(font22_fnt,0,title_text_x,subtitle_text_y,"The start of a shitty adventure");

            font_prep_xbitblt_at(font_base_gray_on_black);
            font_str_bitblt_center(font18_fnt,0,160,199 - 15,"\xC2\xA9"/*Copyright symbol, UTF-8*/ " 2017 DOSLIB, Hackipedia");

            for (i=0;i < menu_items;i++)
                MenuPhaseDrawItem(menuList+i,menu_top_offset,menu_top[1] + 1 - menu_top[0],tmp_offset,menuScroll);

            if (scheduled_fadein) {
                scheduled_fadein = 0;

                /* schedule fade in */
                ev = next_async();
                memset(ev,0,sizeof(*ev));
                ev->what = ASYNC_EVENT_PALETTE;
                ev->e.pal.first = ev->e.pal.first_target = 0;
                ev->e.pal.count = 256;
                ev->e.pal.slot = 0; /* slot 0 */
                ev->e.pal.anim = ASYNC_PAL_ANIM_FADE;
                ev->e.pal.anim_p[0] = 2;
                ev->e.pal.anim_p[1] = 2;
                next_async_finish();
            }

            scrollredraw = 0;
            fullredraw = 0;
            menu_init = 1;
            redraw = 0;
        }
        else if (scrollredraw != 0) {
            if (prev_vsync_tick != timer_irq0_ticksvsync) {
                int renderItem = -1;

                modex_init();

                if (scrollingSpeed > 19) scrollingSpeed = 19;

                if (scrollredraw > 0) {
                    if ((menuScroll+scrollingSpeed) > scrollingTo)
                        scrollingSpeed = scrollingTo - menuScroll;

                    /* scroll up */
                    xbltcopyfx(
                        menu_left,
                        menu_top[0],
                        menu_right + 1 - menu_left,
                        menu_top[1] + 1 - menu_top[0] - scrollingSpeed,
                        menu_top_offset + (menu_left >> 2U) + modex_draw_stride*scrollingSpeed,
                        modex_draw_stride);

                    menuScroll += scrollingSpeed;
                    if (menuScroll == scrollingTo)
                        scrollredraw = 0;
                    else if (scrollingItem < menu_items) {
                        menu_item *m = menuList + scrollingItem;
                        if ((m->p.y+m->p.h-menuScroll) <= (menu_top[1] + 1 - menu_top[0]))
                            renderItem = scrollingItem + 1;
                    }
                }
                else {
                    if ((menuScroll-scrollingSpeed) < scrollingTo)
                        scrollingSpeed = menuScroll - scrollingTo;

                    /* scroll down */
                    xbltcopyfxr(
                        menu_left,
                        menu_top[0] + scrollingSpeed,
                        menu_right + 1 - menu_left,
                        menu_top[1] + 1 - menu_top[0] - scrollingSpeed,
                        menu_top_offset + (menu_left >> 2U),
                        modex_draw_stride);

                    menuScroll -= scrollingSpeed;
                    if (menuScroll == scrollingTo)
                        scrollredraw = 0;
                    else if (scrollingItem > 0) {
                        menu_item *m = menuList + scrollingItem;
                        if ((m->p.y-menuScroll) >= 0)
                            renderItem = scrollingItem - 1;
                    }
                }

                /* blit the pre-rendered item scrolling onto the screen */
                MenuPhaseDrawItemBlit(menuList+scrollingItem,menu_top_offset,menu_top[1] + 1 - menu_top[0],tmp_offset,menuScroll);

                if (renderItem >= 0) {
                    scrollingItem = renderItem;
                    MenuPhaseDrawItemRender(menuList+scrollingItem,menu_top_offset,menu_top[1] + 1 - menu_top[0],tmp_offset,menuScroll);
                    MenuPhaseDrawItemBlit(menuList+scrollingItem,menu_top_offset,menu_top[1] + 1 - menu_top[0],tmp_offset,menuScroll);
                }

                prev_vsync_tick = timer_irq0_ticksvsync;
            }
        }

        if (exiting)
            running = 0;

        if (userctrl) {
            if (kbhit()) {
                c = getch();

                if (c == 27) {
                    /* ok. fade out */
                    if (menuItem >= 0)
                        menuList[menuItem].f.hilighted = 0;

                    menuItem = -1;
                    menu_transition = 1;
                    userctrl = 0;
                    exiting = 1;
                }
                else if (c == 13) {
                    /* take selection */
                    if (menuItem >= 0) {
                        menuList[menuItem].f.hilighted = 0;
                        menu_transition = 1;
                        userctrl = 0;
                        exiting = 1;
                    }
                }
                else if (c == 0) {
                    /* extended */
                    c = getch();

                    if (c == 0x48/*UP arrow*/ || c == 0x50/*DOWN arrow*/) {
                        int oldItem = menuItem;

                        if (menuItem >= 0)
                            menuList[menuItem].f.hilighted = 0;

                        {
                            int adj;

                            if (c == 0x48)
                                adj = -1;
                            else
                                adj = 1;

                            menuItem += adj;
                            menu_item_find_enabled_item(menuList,menu_items,&menuItem,adj,0);
                            menu_item_scroll_to_item(menuList,menu_items,menuItem,menu_top[1] + 1 - menu_top[0],&scrollingTo,0);
                        }

                        if (menuItem >= 0)
                            menuList[menuItem].f.hilighted = 1;

                        if (menuScroll != scrollingTo) {
                            unsigned char accel;

                            if (!scrollredraw)
                                scrollingItem = oldItem; /* item to redraw while scrolling, only if not already */

                            scrollredraw = (scrollingTo > menuScroll) ? 1 : -1;
                            scrollingSpeed = 1;
                            accel = (abs(scrollingTo - menuScroll) / 10);
                            if (accel >= sizeof(menuScrollAccelTable)) accel = sizeof(menuScrollAccelTable) - 1;
                            scrollingSpeed += menuScrollAccelTable[accel];
                        }
                        if (menuItem != oldItem) {
                            if (oldItem >= 0)
                                MenuPhaseDrawItem(menuList+oldItem,menu_top_offset,menu_top[1] + 1 - menu_top[0],tmp_offset,menuScroll);
                            if (menuItem >= 0)
                                MenuPhaseDrawItem(menuList+menuItem,menu_top_offset,menu_top[1] + 1 - menu_top[0],tmp_offset,menuScroll);
                        }

                        if (scrollredraw) {
                            /* pre-render the item, so scrolling can draw it properly */
                            MenuPhaseDrawItemRender(menuList+scrollingItem,menu_top_offset,menu_top[1] + 1 - menu_top[0],tmp_offset,menuScroll);
                        }
                    }
                }
            }
        }
    } while (running);

    while (!async_has_finished());
    halt_async();

    if (menuItem < 0)
        game_running_state_pop();
    else {
        const int i = menuList[menuItem].command;
        if (i >= 0) menu_command_func[i]();
    }
}

int main(int argc,char **argv) {
    init_debug_log();

    if (!initial_sys_check())
        return 1;

    int10_setmode(0x13); /* 320x200x256-color */
    update_state_from_vga();
    vga_enable_256color_modex();
    modex_init();

    vga_refresh_rate_measure();

    /* setup timer. we will use it for async video panning and later, music */
    timer_irq0_chain_add = vga_refresh_timer_ticks;
    setup_timer();

    /* loop. start in title, drop to menu. */
    game_running_state_set(GAME_EXIT);
    game_running_state_push();
    game_running_state_set(GAME_MENU);
    game_running_state_push();
    game_running_state_set(GAME_TITLE);
    do {
        switch (game_running_state) {
            case GAME_TITLE:
                TitleSequence();
                game_running_state_pop();
                break;
            case GAME_MENU:
                MenuPhase(); /* may push/pop and set state as needed */
                break;
            case GAME_EXIT:
                goto game_exit;
            default:
                DEBUG("WARNING: Invalid game state %u",game_running_state);
                goto game_exit;
        }
    } while (1);
game_exit:

    release_timer();
    unloadFont(&font40_fnt);
    unloadFont(&font22_fnt);
    unloadFont(&font18_fnt);
    DEBUG("Timer ticks: %lu ticks / %lu vsync / %lu @ 18.2Hz",
        (unsigned long)timer_irq0_ticks,
        (unsigned long)timer_irq0_ticksvsync,
        (unsigned long)timer_irq0_ticks18);
    int10_setmode(0x3); /* text mode */
    DEBUG_PRINT_MEM_STATE();
    DEBUG("Running heap shrink...");
    _heapshrink();
    _heapmin();
    _heapshrink();
    DEBUG_PRINT_MEM_STATE();
    return 0;
}

