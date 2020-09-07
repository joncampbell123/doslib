
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/emm.h>
#include <hw/dos/himemsys.h>
#include <hw/vga/vga.h>
#include <hw/vga/vrl.h>
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>
#include <fmt/minipng/minipng.h>

struct seq_anim_i {
    unsigned int        duration;       // in ticks
    unsigned int        frame_rate;     // in Hz
    int                 init_frame;
    int                 min_frame;
    int                 max_frame;
    unsigned int        flags;
    signed char         init_dir;
};

#define SEQANF_PINGPONG     (0x01u)
#define SEQANF_OFF          (0x02u)

uint32_t counter;
uint16_t timer_irq_interval; /* PIT clock ticks per IRQ */
uint16_t timer_irq_count;
uint16_t timer_tick_rate = 120;

/* must disable interrupts temporarily to avoid incomplete read */
uint32_t counter_read() {
    uint32_t tmp;

    SAVE_CPUFLAGS( _cli() ) {
        tmp = counter;
    } RESTORE_CPUFLAGS();

    return tmp;
}

void (__interrupt __far *prev_timer_irq)() = NULL;
static void __interrupt __far timer_irq() { /* IRQ 0 */
    counter++;

    /* make sure the BIOS below our handler still sees 18.2Hz */
    {
        const uint32_t s = (uint32_t)timer_irq_count + (uint32_t)timer_irq_interval;
        timer_irq_count = (uint16_t)s;
        if (s >= (uint32_t)0x10000)
            _chain_intr(prev_timer_irq);
        else
            p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | 0);
    }
}

void init_timer_irq() {
    if (prev_timer_irq == NULL) {
        p8259_mask(0);
        p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | 0);
        prev_timer_irq = _dos_getvect(irq2int(0));
        _dos_setvect(irq2int(0),timer_irq);
        timer_irq_interval = T8254_REF_CLOCK_HZ / timer_tick_rate;
	    write_8254_system_timer(timer_irq_interval);
        timer_irq_count = 0;
        counter = 0;
        p8259_unmask(0);
    }
}

void restore_timer_irq() {
    if (prev_timer_irq != NULL) {
        p8259_mask(0);
        p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | 0);
        _dos_setvect(irq2int(0),prev_timer_irq);
	    write_8254_system_timer(0); /* normal 18.2Hz timer tick */
        prev_timer_irq = NULL;
        p8259_unmask(0);
    }
}

struct vrl_image {
    struct vrl1_vgax_header*        vrl_header;
    vrl1_vgax_offset_t*             vrl_lineoffs;
    unsigned char*                  buffer;
    unsigned int                    bufsz;
};

void free_vrl(struct vrl_image *img) {
    if (img->buffer != NULL) {
        free(img->buffer);
        img->buffer = NULL;
    }
    if (img->vrl_lineoffs != NULL) {
        free(img->vrl_lineoffs);
        img->vrl_lineoffs = NULL;
    }
    img->vrl_header = NULL;
    img->bufsz = 0;
}

int load_vrl(struct vrl_image *img,const char *path) {
    struct vrl1_vgax_header *vrl_header;
    vrl1_vgax_offset_t *vrl_lineoffs;
    unsigned char *buffer = NULL;
    unsigned int bufsz = 0;
    int fd = -1;

    fd = open(path,O_RDONLY|O_BINARY);
    if (fd < 0) {
        fprintf(stderr,"Unable to open '%s'\n",path);
        goto fail;
    }
    {
        unsigned long sz = lseek(fd,0,SEEK_END);
        if (sz < sizeof(*vrl_header)) goto fail;
        if (sz >= 65535UL) goto fail;

        bufsz = (unsigned int)sz;
        buffer = malloc(bufsz);
        if (buffer == NULL) goto fail;

        lseek(fd,0,SEEK_SET);
        if ((unsigned int)read(fd,buffer,bufsz) < bufsz) goto fail;

        vrl_header = (struct vrl1_vgax_header*)buffer;
        if (memcmp(vrl_header->vrl_sig,"VRL1",4) || memcmp(vrl_header->fmt_sig,"VGAX",4)) goto fail;
        if (vrl_header->width == 0 || vrl_header->height == 0) goto fail;
    }
    close(fd);

    /* preprocess the sprite to generate line offsets */
    vrl_lineoffs = vrl1_vgax_genlineoffsets(vrl_header,buffer+sizeof(*vrl_header),bufsz-sizeof(*vrl_header));
    if (vrl_lineoffs == NULL) goto fail;

    img->vrl_header = vrl_header;
    img->vrl_lineoffs = vrl_lineoffs;
    img->buffer = buffer;
    img->bufsz = bufsz;

    return 0;
fail:
    if (buffer != NULL) free(buffer);
    if (fd >= 0) close(fd);
    return 1;
}

void pal_buf_to_vga(unsigned int offset,unsigned int count,unsigned char *palette) {
    unsigned int i;

    vga_palette_lseek(offset);
    for (i=0;i < count;i++) vga_palette_write(palette[(i*3)+0]>>2,palette[(i*3)+1]>>2,palette[(i*3)+2]>>2);
}

int pal_load_to_vga(unsigned int offset,unsigned int count,const char *path) {
    unsigned char *palette;
    int fd,ret=-1;

    /* load color palette */
    fd = open(path,O_RDONLY|O_BINARY);
    if (fd >= 0) {
        palette = malloc(3u*count);
        if (palette != NULL) {
            read(fd,palette,3u*count);
            close(fd);

            pal_buf_to_vga(offset,count,palette);

            free(palette);
            ret = 0;
        }
    }

    return 0;
}

#define VGA_PAGE_FIRST          0x0000
#define VGA_PAGE_SECOND         0x4000

/* VGA unchained page flipping state for all code here */
VGA_RAM_PTR orig_vga_graphics_ram;
unsigned int vga_cur_page,vga_next_page;

void vga_swap_pages() {
    vga_cur_page = vga_next_page;
    vga_next_page = (vga_next_page ^ 0x4000) & 0x7FFF;
    vga_state.vga_graphics_ram = orig_vga_graphics_ram + vga_next_page;
}

void vga_update_disp_cur_page() {
    /* make sure the CRTC is not in the blanking portions or vsync
     * so that we're not changing offset during a time the CRTC might
     * latch the new display offset.
     *
     * caller is expected to wait for vsync start at some point to
     * keep time with vertical refresh or bad things (flickering)
     * happen. */
    vga_wait_for_vsync_end();
    vga_wait_for_hsync_end();
    vga_set_start_location(vga_cur_page);
}

static unsigned int vga_rep_stosw(const unsigned char far * const vp,const uint16_t v,const unsigned int wc);
#pragma aux vga_rep_stosw = \
    "rep stosw" \
    parm [es di] [ax] [cx] \
    modify [di cx] \
    value [di];

void vga_clear_npage() {
    vga_write_sequencer(0x02/*map mask*/,0xF);
    vga_rep_stosw(vga_state.vga_graphics_ram,0,0x4000u/2u); /* 16KB (8KB 16-bit WORDS) */
}

void init_vga256unchained() {
    vga_cur_page=VGA_PAGE_FIRST;
    vga_next_page=VGA_PAGE_SECOND;

    int10_setmode(19);
    update_state_from_vga();

    /* Real hardware testing results show that the VGA BIOS will set up chained 256-color mode
     * and only clear the bytes drawn and accessible by that. If you switch on unchained mode X,
     * the junk between the chained DWORDs become visible (not cleared by VGA BIOS). So before
     * we switch to it, use the DAC mask to blank the display first, then switch on unchained
     * mode, clear video memory, then unblank the display by restoring the mask. */
    outp(0x3C6,0x00); // DAC mask: set to 0 to blank display

    vga_enable_256color_modex(); // VGA mode X (unchained)

    vga_write_sequencer(0x02/*map mask*/,0xF); // zero the video memory
    vga_rep_stosw(vga_state.vga_graphics_ram,0,0x8000u/2u);

    outp(0x3C6,0xFF); // DAC mask: restore display

    orig_vga_graphics_ram = vga_state.vga_graphics_ram;
    vga_state.vga_graphics_ram = orig_vga_graphics_ram + vga_next_page;
    vga_set_start_location(vga_cur_page);
}

void restore_text_mode() {
    int10_setmode(3);
}

void unhook_irqs() {
    restore_timer_irq();
}

void fatal(const char *msg,...) {
    va_list va;

    unhook_irqs();
    restore_text_mode();

    printf("FATAL ERROR: ");

    va_start(va,msg);
    vprintf(msg,va);
    va_end(va);
    printf("\n");

    exit(1);
}

int file_zlib_decompress(int fd,unsigned char *buf,unsigned int sz,uint32_t srcsz) {
#define TMP_SZ 1024
    unsigned char *tmp;
    z_stream z;
    int r = -1;

    if (fd < 0) return -1;
    if ((tmp=malloc(TMP_SZ)) == NULL) return -1;

    memset(&z,0,sizeof(z));
    z.next_out = buf;
    z.avail_out = sz;
    z.next_in = tmp;
    z.avail_in = 0;
    if (inflateInit2(&z,15/*max window size 32KB*/) != Z_OK) goto fail;

    while (z.avail_out != 0) {
        if (z.avail_in == 0) {
            if (srcsz != 0) {
                const uint32_t rsz = (srcsz < TMP_SZ) ? srcsz : TMP_SZ;
                if (read(fd,tmp,rsz) != rsz) goto fail2;
                z.avail_in = rsz;
                z.next_in = tmp;
                srcsz -= rsz;
            }
            else {
                break;
            }
        }

        if (inflate(&z,Z_SYNC_FLUSH) != Z_OK) break;
    }

    if (z.avail_out == 0) r = 0;

    inflateEnd(&z);
    free(tmp);
    return r;
fail2:
    inflateEnd(&z);
fail:
    free(tmp);
    return -1;
#undef TMP_SZ
}

/* decode UTF-8 unicode character.
 * returns 0 at end of string or on invalid encoding */
/* [https://en.wikipedia.org/wiki/UTF-8] */
uint32_t utf8decode(const char **ptr) {
    const char *p = *ptr;
    uint32_t ret = 0;

    if (p == NULL) return 0;
    ret = (unsigned char)(*p++);

    /* 0xFE-0xFF            invalid
     * 0xFC-0xFD            6-byte UTF-8 TODO
     * 0xF8-0xFB            5-byte UTF-8 TODO
     * 0xF0-0xF7            4-byte UTF-8 TODO
     * 0xE0-0xEF            3-byte UTF-8
     * 0xC0-0xDF            2-byte UTF-8
     * 0x80-0xBF            invalid (we're in the middle of a UTF-8 char)
     * 0x00-0x7F            1-byte plain ASCII */
    if (ret >= 0xF0) {
        ret = 0;
    }
    else if (ret >= 0xE0) { /* 3-byte */
        /* [1110 xxxx] [10xx xxxx] [10xx xxxx]  4+6+6 = 16 bits */
        ret = (ret & 0xFul) << 12ul;
        if (((*p) & 0xC0) != 0x80) { ret=0; goto stop; }
        ret += (((unsigned char)(*p++)) & 0x3Ful) << 6ul;
        if (((*p) & 0xC0) != 0x80) { ret=0; goto stop; }
        ret += ((unsigned char)(*p++)) & 0x3Ful;
    }
    else if (ret >= 0xC0) { /* 2-byte */
        /* [110x xxxx] [10xx xxxx]  5+6 = 11 bits */
        ret = (ret & 0x1Ful) << 6ul;
        if (((*p) & 0xC0) != 0x80) { ret=0; goto stop; }
        ret += ((unsigned char)(*p++)) & 0x3Ful;
    }
    else if (ret >= 0x80) { /* invalid */
        ret = 0;
    }

stop:
    *ptr = p;
    return ret;
}

/*---------------------------------------------------------------------------*/
/* font handling                                                             */
/*---------------------------------------------------------------------------*/

#pragma pack(push,1)
struct chardef {
/* char id=32   x=178   y=65    width=3     height=1     xoffset=-1    yoffset=15    xadvance=4     page=0  chnl=15 */
    uint16_t        id;             /* unicode code value */
    uint8_t         x,y;            /* starting x and y (upper left corner). These images are 256x256 so it's fine to use uint8_t */
    uint8_t         w,h;            /* width and height at x,y */
    int8_t          xoffset,yoffset;/* offset from draw position to draw */
    int8_t          xadvance;       /* how many to advance */
    /* ignored: page */
    /* ignored: chnl */
};
#pragma pack(pop)

#pragma pack(push,1)
struct kerndef {
/* kerning first=32  second=65  amount=-1 */
    uint16_t        first;
    uint16_t        second;
    int8_t          amount;
};
#pragma pack(pop)

struct font_bmp {
    unsigned char*          fontbmp;        /* 1-bit font */
    unsigned int            stride;         /* bytes per row */
    unsigned int            height;         /* height of bitmap */
    unsigned int            chardef_count;  /* number of chardefs */
    unsigned int            kerndef_count;  /* number of kerndefs */
    struct chardef*         chardef;        /* array of chardef */
    struct kerndef*         kerndef;        /* array of kerndef */
};

void font_bmp_free(struct font_bmp **fnt);

struct font_bmp *font_bmp_load(const char *path) {
    struct font_bmp *fnt = calloc(1,sizeof(struct font_bmp));
    struct minipng_reader *rdr = NULL;

    if (fnt != NULL) {
        if ((rdr=minipng_reader_open(path)) == NULL)
            goto fail;
        if (minipng_reader_parse_head(rdr))
            goto fail;
        if (rdr->ihdr.width > 512 || rdr->ihdr.width < 8 || rdr->ihdr.height > 512 || rdr->ihdr.height < 8)
            goto fail;

        fnt->stride = (rdr->ihdr.width + 7u) / 8u;
        fnt->height = rdr->ihdr.height;

        if ((fnt->fontbmp=malloc(fnt->stride*fnt->height)) == NULL)
            goto fail;

        {
            unsigned int i;
            unsigned char *imgptr = fnt->fontbmp;
            for (i=0;i < fnt->height;i++) {
                minipng_reader_read_idat(rdr,imgptr,1); /* pad byte */
                minipng_reader_read_idat(rdr,imgptr,fnt->stride); /* row */
                imgptr += fnt->stride;
            }

            minipng_reader_reset_idat(rdr);
        }

        {
            uint16_t count;

            /* look for chardef and kerndef structures */
            while (minipng_reader_next_chunk(rdr) == 0) {
                if (rdr->chunk_data_header.type == minipng_chunk_fourcc('c','D','E','Z')) {
                    if (fnt->chardef == NULL) {
                        if (read(rdr->fd,&count,2) != 2) goto fail;
                        fnt->chardef_count = count;
                        if (count != 0 && count < (0xFF00 / sizeof(struct chardef))) {
                            if ((fnt->chardef = malloc(count * sizeof(struct chardef))) == NULL) goto fail;
                            if (file_zlib_decompress(rdr->fd,(unsigned char*)(fnt->chardef),count * sizeof(struct chardef),rdr->chunk_data_header.length-2ul)) goto fail;
                        }
                    }
                }
                else if (rdr->chunk_data_header.type == minipng_chunk_fourcc('k','D','E','Z')) {
                    if (fnt->kerndef == NULL) {
                        if (read(rdr->fd,&count,2) != 2) goto fail;
                        fnt->kerndef_count = count;
                        if (count != 0 && count < (0xFF00u / sizeof(struct kerndef))) {
                            if ((fnt->kerndef = malloc(count * sizeof(struct kerndef))) == NULL) goto fail;
                            if (file_zlib_decompress(rdr->fd,(unsigned char*)(fnt->kerndef),count * sizeof(struct kerndef),rdr->chunk_data_header.length-2ul)) goto fail;
                        }
                    }
                }
            }
        }

        minipng_reader_close(&rdr);
    }

    return fnt;
fail:
    minipng_reader_close(&rdr);
    font_bmp_free(&fnt);
    return NULL;
}

void font_bmp_free(struct font_bmp **fnt) {
    if (*fnt != NULL) {
        if ((*fnt)->fontbmp) free((*fnt)->fontbmp);
        (*fnt)->fontbmp = NULL;

        if ((*fnt)->chardef) free((*fnt)->chardef);
        (*fnt)->chardef = NULL;

        if ((*fnt)->kerndef) free((*fnt)->kerndef);
        (*fnt)->kerndef = NULL;

        free(*fnt);
        *fnt = NULL;
    }
}

struct font_bmp*                    arial_small = NULL;
struct font_bmp*                    arial_medium = NULL;
struct font_bmp*                    arial_large = NULL;

int font_bmp_do_load(struct font_bmp **fnt,const char *path) {
    if (*fnt == NULL) {
        if ((*fnt = font_bmp_load(path)) == NULL)
            return -1;
    }

    return 0;
}

static inline int font_bmp_do_load_arial_large() {
    return font_bmp_do_load(&arial_large,"ariallrg.png");
}

static inline int font_bmp_do_load_arial_medium() {
    return font_bmp_do_load(&arial_medium,"arialmed.png");
}

static inline int font_bmp_do_load_arial_small() {
    return font_bmp_do_load(&arial_small,"arialsml.png");
}

/* yes, we use unicode (UTF-8) strings here in this DOS application! */
int font_bmp_unicode_to_chardef(struct font_bmp *fnt,uint32_t c) {
    if (fnt != NULL && c < 0x10000ul) {
        if (fnt->chardef != NULL) {
            unsigned int i;

            /* NTS: I know, this is very inefficient. Later revisions will add a faster method */
            for (i=0;i < fnt->chardef_count;i++) {
                if (fnt->chardef[i].id == (uint16_t)c)
                    return i;
            }
        }
    }

    return -1;
}

unsigned int font_bmp_draw_chardef(struct font_bmp *fnt,unsigned int cdef,unsigned int x,unsigned int y,unsigned char color) {
    unsigned int nx = x;

    if (fnt != NULL) {
        if (fnt->chardef != NULL && cdef < fnt->chardef_count) {
            const struct chardef *cdent = fnt->chardef + cdef;
            x += (unsigned int)((int)cdent->xoffset);
            y += (unsigned int)((int)cdent->yoffset);
            nx += (unsigned int)((int)cdent->xadvance);
            if (x < 640 && y < 400) {
                const unsigned char *sp = fnt->fontbmp + (fnt->stride * cdent->y) + (cdent->x >> 3u);
                unsigned char *dp = vga_state.vga_graphics_ram + (80u * y) + (x >> 2u);
                unsigned char sm = 0x80u >> (cdent->x & 7u);
                unsigned char dm = 1 << (x & 3u);
                unsigned int sw = cdent->w;

                while (sw-- > 0) {
                    unsigned int sh = cdent->h;
                    const unsigned char *rsp = sp;
                    unsigned char *rdp = dp;

                    vga_write_sequencer(0x02/*map mask*/,dm);
                    while (sh-- > 0) {
                        if ((*rsp) & sm) *rdp = color;
                        rsp += fnt->stride;
                        rdp += 80u;
                    }

                    if ((dm <<= 1u) & 0xF0) {
                        dm = 1u;
                        dp++;
                    }
                    if ((sm >>= 1u) == 0) {
                        sm = 0x80;
                        sp++;
                    }
                }
            }
        }
    }

    return nx;
}

/*---------------------------------------------------------------------------*/
/* introductory sequence                                                     */
/*---------------------------------------------------------------------------*/

#define ANIM_SEQS                   4
#define VRL_IMAGE_FILES             19
#define ATOMPB_PAL_OFFSET           0x00
#define SORC_PAL_OFFSET             0x40
const char *seq_intro_sorc_vrl[VRL_IMAGE_FILES] = {
    "sorcwoo1.vrl",             // 0
    "sorcwoo2.vrl",
    "sorcwoo3.vrl",
    "sorcwoo4.vrl",
    "sorcwoo5.vrl",
    "sorcwoo6.vrl",             // 5
    "sorcwoo7.vrl",
    "sorcwoo8.vrl",
    "sorcwoo9.vrl",
    "sorcuhhh.vrl",
    "sorcbwo1.vrl",             // 10
    "sorcbwo2.vrl",
    "sorcbwo3.vrl",
    "sorcbwo4.vrl",
    "sorcbwo5.vrl",
    "sorcbwo6.vrl",             // 15
    "sorcbwo7.vrl",
    "sorcbwo8.vrl",
    "sorcbwo9.vrl"
                                // 19
};
/* anim1: ping pong loop 0-8 (sorcwoo1.vrl to sorcwoo9.vrl) */
/* anim2: single frame 9 (sorcuhhh.vrl) */
/* anim3: ping pong loop 10-18 (sorcbwo1.vrl to sorcbwo9.vrl) */

/* [0]: sorc "I am the coolest programmer ever woooooooooooo!"
 * [1]: game sprites "Oh great and awesome programmer. What is our purpose in this game?"
 * [2]: sorc "Uhm............"
 * [3]: sorc "I am the coolest programmer ever! I write super awesome code! Wooooooooooooooo!" */

struct seq_anim_i anim_seq[ANIM_SEQS] = {
    /*  dur     fr      if      minf    maxf    fl,                 init_dir */
    {   120*10, 15,     0,      0,      8,      SEQANF_PINGPONG,    1}, // 0
    {   120*12, 1,      0,      0,      0,      SEQANF_OFF,         0}, // 1 [sorc disappears]
    {   120*4,  1,      9,      9,      9,      0,                  0}, // 2
    {   120*12, 30,     10,     10,     18,     SEQANF_PINGPONG,    1}  // 3
};

/* message strings.
 * \n   moves to the next line
 * \x01 clears the text
 * \x02 pauses for 1/4th of a second
 * \x03 pauses for 1 second */
const char *anim_text[ANIM_SEQS] = {
    // 0
    "\x01I am the coolest programmer ever!\nI write super awesome optimized code!\x03\x03\x03\x01Wooooooo I'm so cool!",
    // 1
    "\x01Oh great and awesome programmer and\ncreator of our game world.\x03\x03\x03\x01What is our purpose in this game?",
    // 2
    "\x01Uhm\x02.\x02.\x02.\x02.\x02.\x02.\x02.\x02.\x02.\x02.\x02.",
    // 3
    "\x01I write super optimized clever awesome\nportable code! I am awesome programmer!\x03\x03\x03\x03\x01Wooooooooooooo!"
};

static uint32_t atpb_init_count;

static uint16_t *atpb_sin2048_table = NULL;         // 2048-sample sin(x) quarter wave lookup table (0....0x7FFF)

/* 0x0000 -> sin(0) */
/* 0x0800 -> sin(pi*0.5)    0x0800 = 2048
 * 0x1000 -> sin(pi)        0x1000 = 4096
 * 0x1800 -> sin(pi*1.5)    0x1800 = 6144
 * 0x2000 -> sin(pi*2.0)    0x2000 = 8192 */
int atpb_sin2048_lookup(unsigned int a) {
    int r;

    /* sin(0) to sin(pi) is symmetrical across sin(pi/2) */
    if (a & 0x800u)
        r = atpb_sin2048_table[0x7FFu - (a & 0x7FFu)];
    else
        r = atpb_sin2048_table[a & 0x7FFu];

    /* sin(pi) to sin(pi*2) is just the first half with the opposite sign */
    if (a & 0x1000u)
        r = -r;

    return r;
}

/* cos(x) is just sin(x) shifted by pi/2 (90 degrees) */
inline int atpb_cos2048_lookup(unsigned int a) {
    return atpb_sin2048_lookup(a + 0x800u);
}

/* "Second Reality" style rotozoomer because Woooooooooooo */
void atomic_playboy_zoomer(unsigned int w,unsigned int h,__segment imgseg,uint32_t count) {
    const uint32_t rt = count - atpb_init_count;
    const __segment vseg = FP_SEG(vga_state.vga_graphics_ram);

    // scale, to zoom in and out
    const long scale = ((long)atpb_sin2048_lookup(rt * 5ul) >> 1l) + 0xA000l;
    // column-step. multiplied 4x because we're rendering only every 4 pixels for smoothness.
    const uint16_t sx1 = (uint16_t)(((((long)atpb_cos2048_lookup(rt * 10ul) *  0x400l) >> 15l) * scale) >> 15l);
    const uint16_t sy1 = (uint16_t)(((((long)atpb_sin2048_lookup(rt * 10ul) * -0x400l) >> 15l) * scale) >> 15l);
    // row-step. multiplied by 1.2 (240/200) to compensate for 320x200 non-square pixels. (1.2 * 0x100) = 0x133
    const uint16_t sx2 = (uint16_t)(((((long)atpb_cos2048_lookup(rt * 10ul) *  0x133l) >> 15l) * scale) >> 15l);
    const uint16_t sy2 = (uint16_t)(((((long)atpb_sin2048_lookup(rt * 10ul) * -0x133l) >> 15l) * scale) >> 15l);

    unsigned cvo = FP_OFF(vga_state.vga_graphics_ram);
    uint16_t fcx,fcy;

// make sure rotozoomer is centered on screen
    fcx = 0 - ((w / 2u / 4u) * sx1) - ((h / 2u) *  sy2);
    fcy = 0 - ((w / 2u / 4u) * sy1) - ((h / 2u) * -sx2);

// NTS: Because of VGA unchained mode X, this renders the effect in vertical columns rather than horizontal rows
    while (w >= 4) {
        vga_write_sequencer(0x02/*map mask*/,0x0F);

        // WARNING: This loop temporarily modifies DS. While DS is altered do not access anything by name in the data segment.
        //          However variables declared locally in this function are OK because they are allocated from the stack, and
        //          are referenced using [bp] which uses the stack (SS) register.
        __asm {
            mov     cx,h
            mov     es,vseg
            mov     di,cvo
            inc     cvo
            push    ds
            mov     ds,imgseg
            mov     dx,fcx          ; DX = fractional X coord
            mov     si,fcy          ; SI = fractional Y coord

crloop:
            ; BX = (SI & 0xFF00) + (DX >> 8)
            ;   but DX/BX are the general registers with hi/lo access so
            ; BX = (SI & 0xFF00) + DH
            ; by the way on anything above a 386 that sort of optimization stuff doesn't help performance.
            ; later (Pentium Pro) processors don't like it when you alternate between DH/DL and DX because
            ; of the way the processor works internally regarding out of order execution and register renaming.
            mov     bx,si
            mov     bl,dh
            mov     al,[bx]
            stosb
            add     di,79           ; stosb / add di,79  is equivalent to  mov es:[di],al / add di,80

            add     dx,sy2
            sub     si,sx2

            loop    crloop
            pop     ds
        }

        fcx += sx1;
        fcy += sy1;
        w -= 4;
    }
}

static const int animtext_init_x = 10;
static const int animtext_init_y = 168;
static const unsigned char animtext_color_init = 255;

void seq_intro() {
    unsigned char animtext_color = animtext_color_init;
    const unsigned char at_interval = 120u / 20ul; /* 20 chars/second */
    int animtext_lineheight = 14;
    int animtext_x = animtext_init_x;
    int animtext_y = animtext_init_y;
    struct font_bmp *animtext_fnt = NULL;
    const char *animtext = "";
    uint32_t nanim_count = 0;
    uint16_t vrl_anim_interval = 0;
    uint32_t nf_count = 0,ccount = 0,atcount = 0;
    struct vrl_image vrl_image[VRL_IMAGE_FILES];
    signed char vrl_image_dir = 0;
    int vrl_image_select = 0;
    unsigned atpbseg; /* atomic playboy 256x256 background DOS segment value */
    unsigned char anim;
    int redraw = 1;
    int c;

    /* need arial medium */
    if (font_bmp_do_load_arial_medium())
        fatal("cannot load arial font");

    animtext_fnt = arial_medium;

    /* the rotozoomer effect needs a sin lookup table */
    atpb_sin2048_table = malloc(sizeof(uint16_t) * 2048);
    if (atpb_sin2048_table == NULL) fatal("sorcwoo.sin missing");

    {
        int fd = open("sorcwoo.sin",O_RDONLY | O_BINARY);
        if (fd < 0) fatal("sorcwoo.sin missing");
        read(fd,atpb_sin2048_table,sizeof(uint16_t) * 2048);
        close(fd);
    }

    /* atomic playboy background 256x256 */
    {
        struct minipng_reader *rdr;

        /* WARNING: Code assumes 16-bit large memory model! */

        if ((rdr=minipng_reader_open("atmpbrz.png")) == NULL)
            fatal("seq_intro: failed atmpbrz.png");
        if (minipng_reader_parse_head(rdr) || rdr->plte == NULL || rdr->plte_count < 64)
            fatal("seq_intro: failed atmpbrz.png");
        if (_dos_allocmem(0x1000/*paragrahs==64KB*/,&atpbseg) != 0)
            fatal("seq_intro: failed atmpbrz.png");

        {
            unsigned int i;

            for (i=0;i < 256;i++) {
                unsigned char far *imgptr = (unsigned char far*)MK_FP(atpbseg + (i * 16u/*paragraphs=256b*/),0);
                minipng_reader_read_idat(rdr,imgptr,1); /* pad byte */
                minipng_reader_read_idat(rdr,imgptr,256); /* row */
            }

            {
                const unsigned char *p = (const unsigned char*)(rdr->plte);
                vga_palette_lseek(ATOMPB_PAL_OFFSET);
                for (i=0;i < 64;i++)
                    vga_palette_write(p[(i*3)+0]>>2,p[(i*3)+1]>>2,p[(i*3)+2]>>2);
            }
        }

        minipng_reader_close(&rdr);
    }

    /* text color */
    vga_palette_lseek(0xFF);
    vga_palette_write(63,63,63);

    pal_load_to_vga(/*offset*/SORC_PAL_OFFSET,/*count*/32,"sorcwoo.pal");

    {
        unsigned int vrl_image_count = 0;
        for (vrl_image_count=0;vrl_image_count < VRL_IMAGE_FILES;vrl_image_count++) {
            if (load_vrl(&vrl_image[vrl_image_count],seq_intro_sorc_vrl[vrl_image_count]) != 0)
                fatal("seq_intro: unable to load VRL %s",seq_intro_sorc_vrl[vrl_image_count]);

                vrl_palrebase(
                    vrl_image[vrl_image_count].vrl_header,
                    vrl_image[vrl_image_count].vrl_lineoffs,
                    vrl_image[vrl_image_count].buffer+sizeof(*(vrl_image[vrl_image_count].vrl_header)),
                    SORC_PAL_OFFSET);
        }
    }

    atpb_init_count = atcount = nanim_count = ccount = counter_read();
    anim = -1; /* increment to zero in loop */

    vga_clear_npage();

    do {
        if (ccount >= nanim_count) {
            if ((++anim) >= ANIM_SEQS) break;
            vrl_anim_interval = (uint16_t)(timer_tick_rate / anim_seq[anim].frame_rate);
            vrl_image_select = anim_seq[anim].init_frame;
            vrl_image_dir = anim_seq[anim].init_dir;
            nanim_count += anim_seq[anim].duration;
            if (nanim_count < ccount) nanim_count = ccount;
            nf_count = ccount + vrl_anim_interval;
            atcount = ccount + at_interval;
            animtext = anim_text[anim];
            redraw = 1;
        }
        else if (anim == 0 || anim == 3) {
            redraw = 1; /* always redraw for that super awesome rotozoomer effect :) */
        }

        if (redraw) {
            redraw = 0;

            if (anim == 0 || anim == 3)
                atomic_playboy_zoomer(320/*width*/,168/*height*/,atpbseg,ccount);
            else {
                vga_write_sequencer(0x02/*map mask*/,0xF);
                vga_rep_stosw(vga_state.vga_graphics_ram,0,((320u/4u)*168u)/2u);
            }

            if (!(anim_seq[anim].flags & SEQANF_OFF)) {
                draw_vrl1_vgax_modex(70,0,
                    vrl_image[vrl_image_select].vrl_header,
                    vrl_image[vrl_image_select].vrl_lineoffs,
                    vrl_image[vrl_image_select].buffer+sizeof(*vrl_image[vrl_image_select].vrl_header),
                    vrl_image[vrl_image_select].bufsz-sizeof(*vrl_image[vrl_image_select].vrl_header));
            }

            vga_swap_pages(); /* current <-> next */
            vga_update_disp_cur_page();
            vga_wait_for_vsync(); /* wait for vsync */
        }

        if (ccount >= atcount) {
            uint32_t last_atcount = atcount;

            atcount += at_interval;

            if (*animtext != 0) {
                switch (*animtext) {
                    case 0x01: // clear text
                        animtext_x = animtext_init_x;
                        animtext_y = animtext_init_y;
                        vga_write_sequencer(0x02/*map mask*/,0x0F);
                        vga_rep_stosw((unsigned char far*)MK_FP(0xA000,((320u/4u)*168u)+VGA_PAGE_FIRST),0,((320u/4u)*(200u-168u))/2u);
                        vga_rep_stosw((unsigned char far*)MK_FP(0xA000,((320u/4u)*168u)+VGA_PAGE_SECOND),0,((320u/4u)*(200u-168u))/2u);
                        animtext++;
                        break;
                    case 0x02: // 1/4-sec pause
                        atcount = last_atcount + (120ul / 4ul);
                        animtext++;
                        break;
                    case 0x03: // 1-sec pause
                        atcount = last_atcount + 120ul;
                        animtext++;
                        break;
                    case '\n': // newline
                        animtext_x = animtext_init_x;
                        animtext_y += animtext_lineheight;//FIXME: Font should specify height
                        animtext++;
                        break;
                    default: // print text
                        {
                            const uint32_t c = utf8decode(&animtext);
                            if (c != 0ul) {
                                unsigned char far *sp = vga_state.vga_graphics_ram;
                                const uint32_t cdef = font_bmp_unicode_to_chardef(animtext_fnt,c);

                                vga_state.vga_graphics_ram = (unsigned char far*)MK_FP(0xA000,VGA_PAGE_FIRST);
                                font_bmp_draw_chardef(animtext_fnt,cdef,animtext_x,animtext_y,animtext_color);

                                vga_state.vga_graphics_ram = (unsigned char far*)MK_FP(0xA000,VGA_PAGE_SECOND);
                                animtext_x = font_bmp_draw_chardef(animtext_fnt,cdef,animtext_x,animtext_y,animtext_color);

                                vga_state.vga_graphics_ram = sp;
                            }
                        }
                        break;
                }
            }

            if (atcount < ccount) atcount = ccount;
        }

        if (kbhit()) {
            c = getch();
            if (c == 27) break;
        }

        while (ccount >= nf_count) {
            if (!(anim_seq[anim].flags & SEQANF_OFF)) {
                redraw = 1;
                if (vrl_image_select >= anim_seq[anim].max_frame) {
                    if (anim_seq[anim].flags & SEQANF_PINGPONG) {
                        vrl_image_select = anim_seq[anim].max_frame - 1;
                        vrl_image_dir = -1;
                    }
                }
                else if (vrl_image_select <= anim_seq[anim].min_frame) {
                    if (anim_seq[anim].flags & SEQANF_PINGPONG) {
                        vrl_image_select = anim_seq[anim].min_frame + 1;
                        vrl_image_dir = 1;
                    }
                }
                else {
                    vrl_image_select += (int)vrl_image_dir; /* sign extend */
                }
            }

            nf_count += vrl_anim_interval;
        }

        ccount = counter_read();
    } while (1);

    /* sinus table */
    free(atpb_sin2048_table);
    atpb_sin2048_table = NULL;
    /* atomic playboy image seg */
    _dos_freemem(atpbseg);
    /* VRLs */
    for (vrl_image_select=0;vrl_image_select < VRL_IMAGE_FILES;vrl_image_select++)
        free_vrl(&vrl_image[vrl_image_select]);
#undef ATOMPB_PAL_OFFSET
#undef SORC_PAL_OFFSET
#undef VRL_IMAGE_FILES
#undef ANIM_SEQS
}

/*---------------------------------------------------------------------------*/
/* main                                                                      */
/*---------------------------------------------------------------------------*/

int main() {
    probe_dos();
	cpu_probe();
    if (cpu_basic_level < 3) {
        printf("This game requires a 386 or higher\n");
        return 1;
    }

	if (!probe_8254()) {
		printf("No 8254 detected.\n");
		return 1;
	}
	if (!probe_8259()) {
		printf("No 8259 detected.\n");
		return 1;
	}
    if (!probe_vga()) {
        printf("VGA probe failed.\n");
        return 1;
    }

#if TARGET_MSDOS == 16
    probe_emm();            // expanded memory support
    probe_himem_sys();      // extended memory support
#endif

    init_timer_irq();
    init_vga256unchained();

    seq_intro();

    font_bmp_free(&arial_small);
    font_bmp_free(&arial_medium);
    font_bmp_free(&arial_large);

    unhook_irqs();
    restore_text_mode();

    return 0;
}

