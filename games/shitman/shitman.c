
#include <stdio.h>
#include <errno.h>
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

        // FIXME
#if TARGET_MSDOS == 16
        _fmemcpy(vga_state.vga_graphics_ram,img->RasterBits,320*200);
#else
        memcpy(vga_state.vga_graphics_ram,img->RasterBits,320*200);
#endif
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

    /* wait */
    while (getch() != 13);

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

    DisplayGIF("title1.gif",DisplayGIF_FADEIN);
    DisplayGIF("title2.gif",DisplayGIF_FADEIN);
    DisplayGIF("title3.gif",DisplayGIF_FADEIN);

    int10_setmode(0x3); /* text mode */
    DEBUG_PRINT_MEM_STATE();
    return 0;
}

