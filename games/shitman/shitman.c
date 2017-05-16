
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

/* async event in general to do from vsync timer IRQ */
typedef struct AsyncEvent {
    uint8_t     what;
    union {
        AsyncPal    pal;
        AsyncVPan   vpan;
        uint16_t    wait;
    } e;
} AsyncEvent;

enum {
    ASYNC_EVENT_STOP=0,         // stop processing
    ASYNC_EVENT_WAIT,           // wait for vertical retrace counts given in e.wait (e.wait)
    ASYNC_EVENT_PALETTE,        // palette animation (e.pal)
    ASYNC_EVENT_VPAN            // change display start address (e.vpan)
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

/* do not call from interrupt */
AsyncEvent *next_async(void) {
    if (async_event_write >= MAX_ASYNC_EVENT)
        FAIL("Too many async events");

    return &async_events[async_event_write];
}

void next_async_finish(void) {
    async_event_write++;
}

void flush_async(void) {
    _cli();
    async_event_write = async_event_index = 0;
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
            const unsigned char fade = ev->e.pal.anim_p[0];

            for (c=0;c < ev->e.pal.count;c++,scp++)
                vga_palette_write(fadein_cap(fade,scp->r),fadein_cap(fade,scp->g),fadein_cap(fade,scp->b));

            ev->e.pal.anim_p[0] += ev->e.pal.anim_p[1];
            if (fade < 64) return; // not yet done. we're done when fade < 0 or fade > 63
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

void xbitblt(int x,int y,int w,int h,unsigned char *bits) {
    unsigned int orig_w = w;

    /* NTS: We render in Mode X at all times */
    /* Assume: bits != NULL */
    if (x < 0) { bits += -x;          w += x; x = 0; }
    if (y < 0) { bits += -y * orig_w; h += y; y = 0; }
    if (w <= 0 || h <= 0) return;
    if (((uint16_t)(x+w)) > modex_draw_width) w = modex_draw_width - x;
    if (((uint16_t)(y+h)) > modex_draw_height) h = modex_draw_height - y;
    if (w <= 0 || h <= 0) return;

    xbitblt_nc((unsigned int)x,(unsigned int)y,(unsigned int)w,(unsigned int)h,orig_w,bits);
}

#define DisplayGIF_FADEIN       (1U << 0U)

void DisplayGIF(const char *path,unsigned int how) {
    GifFileType *gif;
    int err;

    DEBUG("DisplayGIF: %s",path);

    gif = DGifOpenFileName(path,&err);
    if (gif == NULL) {
        DEBUG("DGifOpenFileName failed, err=%d %s",err,GifErrorString(gif->Error));
        return;
    }

    /* TODO: How do we read only the first image? */
    if (DGifSlurp(gif) != GIF_OK) {
        DEBUG("DGifSlurp failed Error=%u %s",gif->Error,GifErrorString(gif->Error));
        DGifCloseFile(gif,&err);
        return;
    }
    if (gif->ImageCount == 0) {
        DEBUG("No GIF images");
        DGifCloseFile(gif,&err);
        return;
    }
    if (gif->SColorMap == NULL) {
        DEBUG("No colormap");
        DGifCloseFile(gif,&err);
        return;
    }

    flush_async();
    { /* blank VGA palette */
        unsigned int i;

        vga_palette_lseek(0);
        for (i=0;i < 256;i++) vga_palette_write(0,0,0);
    }

    /* assume gif->SavedImages != NULL
     * assume gif->SavedImages[0] != NULL */
    {
        SavedImage *img = &gif->SavedImages[0];

        xbitblt(img->ImageDesc.Left,img->ImageDesc.Top,img->ImageDesc.Width,img->ImageDesc.Height,img->RasterBits);
    }

    /* schedule palette fade in. take palette slot 0 */
    {
        ColorMapObject *c = gif->SColorMap;
        AsyncEvent *ae;

        {
            GifColorType *color = c->Colors;
            unsigned int i = c->ColorCount;

            for (i=0;i < c->ColorCount;i++) {
                pal_slots[0].pal[i].r = color[i].Red >> 2;
                pal_slots[0].pal[i].g = color[i].Green >> 2;
                pal_slots[0].pal[i].b = color[i].Blue >> 2;
            }
        }

        ae = next_async();
        ae->what = ASYNC_EVENT_PALETTE;
        ae->e.pal.slot = 0;
        ae->e.pal.first = 0;
        ae->e.pal.first_target = 0;
        ae->e.pal.count = c->ColorCount;
        ae->e.pal.anim = (how & DisplayGIF_FADEIN) ? ASYNC_PAL_ANIM_FADE : ASYNC_PAL_ANIM_NONE;
        ae->e.pal.anim_p[0] = 2; // start at 2
        ae->e.pal.anim_p[1] = 2; // step by 2
        next_async_finish();
    }

    {
        unsigned long waitper = t8254_us2ticks(100000); /* 100ms */
        unsigned int patience = 50;
        int c;

        /* wait for space, enter, or 4 seconds */
        do {
            if (kbhit()) {
                c = getch();
                if (c == 13 || c == ' ') break;
            }

            t8254_wait(waitper);
        } while (--patience != 0);
    }

    if (how & DisplayGIF_FADEIN) {
        ColorMapObject *c = gif->SColorMap;
        AsyncEvent *ae;

        ae = next_async();
        ae->what = ASYNC_EVENT_PALETTE;
        ae->e.pal.slot = 0;
        ae->e.pal.first = 0;
        ae->e.pal.first_target = 0;
        ae->e.pal.count = c->ColorCount;
        ae->e.pal.anim = (how & DisplayGIF_FADEIN) ? ASYNC_PAL_ANIM_FADE : ASYNC_PAL_ANIM_NONE;
        ae->e.pal.anim_p[0] = 60; // start at 60
        ae->e.pal.anim_p[1] = -4; // step by -4
        next_async_finish();
    }

    if (DGifCloseFile(gif,&err) != GIF_OK)
        DEBUG("DGifCloseFile failed, err=%d %s",err,GifErrorString(err));
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

    DisplayGIF("title1.gif",DisplayGIF_FADEIN);
    DisplayGIF("title2.gif",DisplayGIF_FADEIN);
    DisplayGIF("title3.gif",DisplayGIF_FADEIN);

    release_timer();
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

