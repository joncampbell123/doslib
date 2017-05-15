
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

#pragma pack(push,1)
typedef struct GIFColor {
    uint8_t                 r,g,b;
} GIFColor;
#pragma pack(pop)

typedef struct GIFLoader {
    int                     fd;                             /* >= 0 if open, -1 if not */
    unsigned char           version;                        /* 87 or 89 */
    uint16_t                screen_width,screen_height;
    uint8_t                 background_color_index;
    uint32_t                first_frame_at;
    union {
        struct {
            unsigned int    size_of_global_color_table:3;   /* bits 2-0 */
            unsigned int    sort_flag:1;                    /* bits 3-3 */
            unsigned int    color_resolution:3;             /* bits 6-4 */
            unsigned int    global_color_table:1;           /* bits 7-7 */
        } f;
        unsigned char       raw;
    } pf;
    GIFColor*               global_color_table;
} GIFLoader;

typedef struct GIFImage {
    uint16_t                left,top,width,height;
    union {
        struct {
            unsigned int    size_of_local_color_table:3;    /* bits 2-0 */
            unsigned int    reserved:2;                     /* bits 4-3 */
            unsigned int    sort_flag:1;                    /* bits 5-5 */
            unsigned int    interlace_flag:1;               /* bits 6-6 */
            unsigned int    local_color_table:1;            /* bits 7-7 */
        } f;
        unsigned char       raw;
    } pf;
    GIFColor*               local_color_table;
    unsigned char*          image;
} GIFImage;

unsigned int GIFLoader_global_color_table_size(GIFLoader *g) {
    return 1U << (g->pf.f.size_of_global_color_table + 1U);
}

unsigned int GIFImage_local_color_table_size(GIFImage *i) {
    return 1U << (i->pf.f.size_of_local_color_table + 1U);
}

void GIFLoader_closefile(GIFLoader *g) {
    if (g->fd >= 0) {
        close(g->fd);
        g->fd = -1;
    }
}

void GIFLoader_free_global_color_table(GIFLoader *g) {
    if (g->global_color_table) {
        free(g->global_color_table);
        g->global_color_table = NULL;
    }
}

int GIFLoader_openfile(GIFLoader *g,const char *path) {
    unsigned char tmp[16];

    GIFLoader_closefile(g);
    GIFLoader_free_global_color_table(g);

    g->fd = open(path,O_RDONLY | O_BINARY);
    if (g->fd < 0) return -1;

    if (lseek(g->fd,0,SEEK_SET) != 0)
        goto fail;

    /* GIF87a/GIF89a header */
    if (read(g->fd,tmp,6) != 6)
        goto fail;
    if (memcmp(tmp,"GIF8",4) != 0 || !(tmp[4] == '7' || tmp[4] == '9') || tmp[5] != 'a') {
        DEBUG("GIF: Invalid header");
        goto fail;
    }

    g->version = 80 + ((unsigned char)tmp[4] - '0');
    DEBUG("GIF: header is type %ua",g->version);

    /* Logical Screen Descriptor */
    if (read(g->fd,tmp,7) != 7)
        goto fail;

    g->screen_width = *((uint16_t*)(tmp+0));                /* +0 logical screen width */
    g->screen_height = *((uint16_t*)(tmp+2));               /* +2 logical screen height */
    g->pf.raw = tmp[4];                                     /* +4 packed fields */
    g->background_color_index = tmp[5];                     /* +5 background color index */
                                                            /* +6 pixel aspect ratio (ignored) */

    DEBUG("GIF: Logical screen descriptor is %u x %u background=%u global_color_table=%u",
        g->screen_width,
        g->screen_height,
        g->background_color_index,
        g->pf.f.global_color_table?1:0);

    if (g->pf.f.global_color_table) {
        unsigned int size = GIFLoader_global_color_table_size(g);
        unsigned int bc = size * sizeof(GIFColor);

        DEBUG("GIF: Global color table is %u colors",size);

        g->global_color_table = malloc(sizeof(GIFColor) * size);
        if (g->global_color_table == NULL) {
            DEBUG("Failed to alloc GIF global color table");
            goto fail;
        }

        if (read(g->fd,g->global_color_table,bc) != bc) {
            DEBUG("Failed to read GIF global color table");
            goto fail;
        }
    }

    g->first_frame_at = lseek(g->fd,0,SEEK_CUR);
    DEBUG("GIF starts at %lu bytes",(unsigned long)g->first_frame_at);

    return 0;
fail:
    GIFLoader_closefile(g);
    errno = EINVAL;
    return -1;
}

GIFLoader *GIFLoader_alloc(void) {
    GIFLoader *g = malloc(sizeof(*g));
    if (g == NULL) return NULL;

    memset(g,0,sizeof(*g));
    g->fd = -1;

    return g;
}

GIFLoader *GIFLoader_free(GIFLoader *g) {
    if (g != NULL) {
        GIFLoader_closefile(g);
        GIFLoader_free_global_color_table(g);
        memset(g,0,sizeof(*g));
        free(g);
    }

    return NULL;
}

GIFImage *GIFImage_alloc(void) {
    GIFImage *i = malloc(sizeof(*i));
    if (i == NULL) return NULL;

    memset(i,0,sizeof(*i));

    return i;
}

void GIFImage_free_image(GIFImage *i) {
    if (i->image) {
        free(i->image);
        i->image = NULL;
    }
}
        
void GIFImage_free_color_table(GIFImage *i) {
    if (i->local_color_table) {
        free(i->local_color_table);
        i->local_color_table = NULL;
    }
}

GIFImage *GIFImage_free(GIFImage *i) {
    if (i != NULL) {
        GIFImage_free_image(i);
        GIFImage_free_color_table(i);
        free(i);
    }

    return NULL;
}

int GIFLoader_getimage(GIFLoader *g,GIFImage **pImg) {
    unsigned char tmp[16];

    *pImg = GIFImage_free(*pImg);

    do {
        if (read(g->fd,tmp,1) != 1)
            break;

        if (tmp[0] == 0x21) {
            /* extension */
            if (read(g->fd,tmp+1,1) != 1)
                break;

            if (tmp[1] == 0xF9) { /* Graphic Control Extension */
                if (read(g->fd,tmp+2,1) != 1) /* block size */
                    break;

                if (tmp[2] > 4) {
                    DEBUG("GIFLoader_getimage graphic control ext too large, 0x%02X",tmp[2]);
                    break;
                }

                if (read(g->fd,tmp+3,tmp[2]+1) != (tmp[2]+1))
                    break;
                if (tmp[3+tmp[2]] != 0) /* must have block terminator */
                    break;
            }
            else {
                DEBUG("GIFLoader_getimage unknown extension block byte 0x%02X",tmp[1]);
                break;
            }
        }
        else if (tmp[0] == 0x2C) {
            GIFImage *i;

            /* Image descriptor */
            if (read(g->fd,tmp+1,9) != 9)
                break;

            i = GIFImage_alloc();
            if (i == NULL)
                break;

            i->left = *((uint16_t*)(tmp+1));            /* +0x01 left */
            i->top = *((uint16_t*)(tmp+3));             /* +0x03 top */
            i->width = *((uint16_t*)(tmp+5));           /* +0x05 width */
            i->height = *((uint16_t*)(tmp+7));          /* +0x07 height */
            i->pf.raw = tmp[9];                         /* +0x09 packed fields */

            DEBUG("GIF: Image descriptor %u x %u starting at %u x %u",
                i->width,i->height,i->left,i->top);

            if (i->pf.f.local_color_table) {
                unsigned int size = GIFImage_local_color_table_size(i);
                unsigned int bc = size * sizeof(GIFColor);

                DEBUG("GIF: Local color table is %u colors",size);

                i->local_color_table = malloc(sizeof(GIFColor) * size);
                if (i->local_color_table == NULL) {
                    DEBUG("Failed to alloc GIF local color table");
                    GIFImage_free(i);
                    break;
                }

                if (read(g->fd,i->local_color_table,bc) != bc) {
                    DEBUG("Failed to read GIF local color table");
                    GIFImage_free(i);
                    break;
                }
            }

            if (i->width == 0 || i->height == 0) {
                GIFImage_free(i);
                break;
            }

            {
                unsigned long sz =
                    (unsigned long)i->width *
                    (unsigned long)i->height;

#if TARGET_MSDOS == 16
                if (sz > 0xFFF0UL) {
                    DEBUG("GIF image too large (16-bit 64KB limit)");
                    GIFImage_free(i);
                    break;
                }
#else
                if (sz > 0xFFFFFUL) {
                    DEBUG("GIF image too large (32-bit 1MB limit)");
                    GIFImage_free(i);
                    break;
                }
#endif

                i->image = malloc(sz);
                if (i->image == NULL) {
                    DEBUG("Unable to allocate GIF image memory");
                    GIFImage_free(i);
                    break;
                }
            }
        }
        else {
            DEBUG("GIFLoader_getimage unknown block byte 0x%02X",tmp[0]);
            break;
        }
    } while (1);

    errno = ENOENT;
    return -1;
}

void DisplayGIF(const char *path) {
    GIFLoader *gif;
    GIFImage *img;

    DEBUG("DisplayGIF: %s",path);

    gif = GIFLoader_alloc();
    if (gif == NULL) FAIL("GIFLoader_alloc failed");

    if (GIFLoader_openfile(gif,path) < 0) FAIL("GIFLoader_openfile unable to open file");

    img = NULL;
    if (GIFLoader_getimage(gif,&img) < 0) FAIL("GIFLoader_getimage failed");

    GIFLoader_free(gif);
    GIFImage_free(img);
}

int main(int argc,char **argv) {
    init_debug_log();

    if (!initial_sys_check())
        return 1;

    int10_setmode(0x13); /* 320x200x256-color */

    DisplayGIF("title1.gif");
    DisplayGIF("title2.gif");
    DisplayGIF("title3.gif");

    int10_setmode(0x3); /* text mode */
    return 0;
}

