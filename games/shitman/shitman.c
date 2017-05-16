
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
    int10_setmode(0x3); /* text mode */
    puts(msg);
    exit(1);
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

static inline unsigned char fadein_cap(unsigned char fade,unsigned char v) {
    return (v > fade) ? fade : v;
}

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

    {
        ColorMapObject *c = gif->SColorMap;
        unsigned int i,fade;

        fade = (how & DisplayGIF_FADEIN) ? 0 : 64;

        do {
            GifColorType *color = c->Colors;

            vga_wait_for_vsync_end();

            vga_palette_lseek(0);

            i = c->ColorCount;
            do {
                vga_palette_write(
                        fadein_cap(fade,color->Red>>2),
                        fadein_cap(fade,color->Green>>2),
                        fadein_cap(fade,color->Blue>>2));
                color++;
            } while (--i != 0);

            vga_wait_for_vsync();
        } while ((fade += 2) < 64);
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
        unsigned int i,fade;

        fade = 60;

        do {
            GifColorType *color = c->Colors;

            vga_wait_for_vsync_end();

            vga_palette_lseek(0);

            i = c->ColorCount;
            do {
                vga_palette_write(
                        fadein_cap(fade,color->Red>>2),
                        fadein_cap(fade,color->Green>>2),
                        fadein_cap(fade,color->Blue>>2));
                color++;
            } while (--i != 0);

            vga_wait_for_vsync();
        } while ((fade -= 4) != 0);
    }

    if (DGifCloseFile(gif,&err) != GIF_OK)
        DEBUG("DGifCloseFile failed, err=%d %s",err,GifErrorString(err));
}

int main(int argc,char **argv) {
    init_debug_log();

    if (!initial_sys_check())
        return 1;

    int10_setmode(0x13); /* 320x200x256-color */
    update_state_from_vga();
    vga_enable_256color_modex();
    modex_init();

    DisplayGIF("title1.gif",DisplayGIF_FADEIN);
    DisplayGIF("title2.gif",DisplayGIF_FADEIN);
    DisplayGIF("title3.gif",DisplayGIF_FADEIN);

    int10_setmode(0x3); /* text mode */
    DEBUG_PRINT_MEM_STATE();
    DEBUG("Running heap shrink...");
    _heapshrink();
    _heapmin();
    _heapshrink();
    DEBUG_PRINT_MEM_STATE();
    return 0;
}

