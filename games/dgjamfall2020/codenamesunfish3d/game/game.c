
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <malloc.h>
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

#include "timer.h"
#include "vmode.h"
#include "fonts.h"
#include "vrlimg.h"
#include "freein.h"
#include "dbgheap.h"
#include "fontbmp.h"
#include "unicode.h"
#include "commtmp.h"
#include "sin2048.h"
#include "vrldraw.h"
#include "seqcomm.h"
#include "keyboard.h"
#include "dumbpack.h"
#include "fzlibdec.h"
#include "fataexit.h"
#include "sorcpack.h"
#include "rotozoom.h"
#include "seqcanvs.h"
#include "cutscene.h"

#include <hw/8042/8042.h>

struct game_2dvec_t {
    int32_t         x,y;    /* 16.16 fixed point */
};

struct game_2dvec_t         game_position;
uint64_t                    game_angle;

#define GAME_VERTICES       128
struct game_2dvec_t         game_vertex[GAME_VERTICES];
struct game_2dvec_t         game_vertexrot[GAME_VERTICES];
unsigned                    game_vertex_max;

/* from start to end, the wall is a line and faces 90 degrees to the right from the line, DOOM style.
 *
 *       ^
 *       |
 *   end +
 *       |
 *       |
 *       |-----> faces this way (sidedef 0)
 *       |
 *       |
 * start +
 */
struct game_2dlineseg_t {
    uint16_t                start,end;                  /* vertex indices */
    uint16_t                flags;
    uint16_t                sidedef[2];                 /* [0]=front side       [1]=opposite side */
};

#define GAME_LINESEG        128
struct game_2dlineseg_t     game_lineseg[GAME_LINESEG];
unsigned                    game_lineseg_max;

struct game_2dsidedef_t {
    uint8_t                 texture;
    int8_t                  xoffset,yoffset;
    uint8_t                 sector;                     /* which sector this faces */
};

#define GAME_SIDEDEFS       128
struct game_2dsidedef_t     game_sidedef[GAME_SIDEDEFS];

struct game_2dsector_t {
    int32_t                 top,bottom;
    uint8_t                 floor,ceiling;
};

#define GAME_SECTORS        16
struct game_2dsector_t      game_sector[GAME_SECTORS];

/* No BSP tree, sorry. The 3D "overworld" is too simple and less important to need it.
 * Also no monsters and cacodemons to shoot. */

#define GAME_TEXTURE_W      64
#define GAME_TEXTURE_H      64
struct game_2dtexture_t {
    unsigned char*          tex;        /* 64x64 texture = 2^6 * 2^6 = 2^12 = 4096 bytes = 4KB */
};

#define GAME_TEXTURES       8
struct game_2dtexture_t     game_texture[GAME_TEXTURES] = { {NULL} };

struct game_vslice_t {
    int16_t                 top,bottom;         /* total slice including floor to ceiling */
    int16_t                 floor,ceil;         /* wall slice (from floor to ceiling) */
    unsigned                sector;
    unsigned                sidedef;
    unsigned                next;               /* next to draw or ~0u */
    int32_t                 dist;
};

#define GAME_VSLICE_MAX     2048
struct game_vslice_t        game_vslice[GAME_VSLICE_MAX];
unsigned                    game_vslice_alloc;

#define GAME_VSLICE_DRAW    320
unsigned                    game_vslice_draw[GAME_VSLICE_DRAW];

#define GAME_MIN_Z          (1l << 11l)

int32_t game_3dto2d(struct game_2dvec_t *d2) {
    const int32_t dist = d2->y;

    d2->x = ((int32_t)(160l << 16l)) + (int32_t)(((int64_t)d2->x << (16ll + 6ll)) / (int64_t)d2->y); /* fixed point 16.16 division */
    d2->y = 100l << 16l;

    return dist;
}

void game_project_lineseg(const unsigned int i) {
    struct game_2dlineseg_t *lseg = &game_lineseg[i];

    /* 3D to 2D project */
    /* if the vertices are backwards, we're looking at the opposite side */
    {
        struct game_2dvec_t pr1,pr2;
        int32_t d1,d2;
        unsigned side;
        int x1,x2,x;
        int ix,ixd;

        pr1 = game_vertexrot[lseg->start];
        pr2 = game_vertexrot[lseg->end];
        side = 0;

        if (pr1.y < GAME_MIN_Z && pr2.y < GAME_MIN_Z) {
            return;
        }
        else if (pr1.y < GAME_MIN_Z || pr2.y < GAME_MIN_Z) {
            const int32_t dx = pr2.x - pr1.x;
            const int32_t dy = pr2.y - pr1.y;

            if (dx == 0l) {
                if (pr1.y < GAME_MIN_Z) pr1.y = GAME_MIN_Z;
                if (pr2.y < GAME_MIN_Z) pr2.y = GAME_MIN_Z;
            }
            else {
                if (pr2.y < GAME_MIN_Z) {
                    const int32_t cdy = GAME_MIN_Z - pr1.y;
                    pr2.x = pr1.x + (int32_t)(((int64_t)cdy * (int64_t)dx) / (int64_t)dy);
                    pr2.y = GAME_MIN_Z;
                }
                else if (pr1.y < GAME_MIN_Z) {
                    const int32_t cdy = GAME_MIN_Z - pr2.y;
                    pr1.x = pr2.x + (int32_t)(((int64_t)cdy * (int64_t)dx) / (int64_t)dy);
                    pr1.y = GAME_MIN_Z;
                }
                else {
                    return;
                }
            }
        }

        d1 = game_3dto2d(&pr1);
        d2 = game_3dto2d(&pr2);

        if (pr1.x > pr2.x) {
            struct game_2dvec_t tpr;
            int32_t td;

            tpr = pr1;
            td = d1;

            pr1 = pr2;
            d1 = d2;

            pr2 = tpr;
            d2 = td;

            side = 1;
        }

        if (side)
            return;

        d1 = (int32_t)((1ll << 32ll) / (int64_t)d1);
        d2 = (int32_t)((1ll << 32ll) / (int64_t)d2);

        ix = x1 = (int)(pr1.x >> 16l);
        if (x1 < 0) x1 = 0;

        x2 = (int)(pr2.x >> 16l);
        ixd = x2 - ix;
        if (x2 > 320) x2 = 320;

        for (x=x1;x < x2;x++) {
            if (game_vslice_alloc < GAME_VSLICE_MAX) {
                const int32_t id = d1 + (int32_t)((((int64_t)(d2 - d1) * (int64_t)(x - ix)) / (int64_t)ixd));
                const int32_t d = (int32_t)((1ll << 32ll) / (int64_t)id);
                const unsigned pri = game_vslice_draw[x];

                if (pri != (~0u)) {
                    if (d > game_vslice[pri].dist)
                        continue;
                }

                if (d >= GAME_MIN_Z) {
                    const unsigned vsi = game_vslice_alloc++;
                    struct game_vslice_t *vs = &game_vslice[vsi];
                    int32_t h = (int32_t)(((int64_t)64ll << (int64_t)16ll) / (int64_t)d);

                    vs->top = 0;
                    vs->bottom = 0;
                    vs->ceil = (int)(((100l << 1l) - h) >> 1l);
                    vs->floor = (int)(((100l << 1l) + h) >> 1l);
                    vs->next = pri;
                    vs->dist = d;

                    game_vslice_draw[x] = vsi;
                }
            }
        }
    }
}

void game_texture_free(struct game_2dtexture_t *t) {
    if (t->tex != NULL) {
        free(t->tex);
        t->tex = NULL;
    }
}

void game_texture_freeall(void) {
    unsigned int i;

    for (i=0;i < GAME_TEXTURES;i++)
        game_texture_free(&game_texture[i]);
}

static inline void game_set_vertexfip(const unsigned i,int32_t x,int32_t y) {
    game_vertex[i].x = x;
    game_vertex[i].y = y;
}

static inline void game_set_linedef_ss(const unsigned i,const unsigned s,const unsigned e,const uint16_t flags,const unsigned sd) {
    game_lineseg[i].start = s;
    game_lineseg[i].end = e;
    game_lineseg[i].flags = flags;
    game_lineseg[i].sidedef[0] = sd;
    game_lineseg[i].sidedef[1] = ~0u;
}

static inline void game_set_sidedef(const unsigned i,const unsigned tex,const int8_t xoff,const int8_t yoff,const unsigned int sector) {
    game_sidedef[i].texture = tex;
    game_sidedef[i].xoffset = xoff;
    game_sidedef[i].yoffset = yoff;
    game_sidedef[i].sector = sector;
}

static inline void game_set_sector(const unsigned i,const int32_t top,const int32_t bottom,const unsigned floor,const unsigned ceil) {
    game_sector[i].top = top;
    game_sector[i].bottom = bottom;
    game_sector[i].floor = floor;
    game_sector[i].ceiling = ceil;
}

void game_loop(void) {
    struct game_vslice_t *vsl;
    unsigned int x2;
    unsigned int o;
    unsigned int i;
    unsigned int x;

    /* seqanim rotozoomer needs sin2048 */
    if (sin2048fps16_open())
        fatal("cannot open sin2048");

    game_vertex_max = 0;
    game_lineseg_max = 0;

    /* init pos */
    game_position.x = 0;
    game_position.y = 0;
    game_angle = 0; /* looking straight ahead */

    /*    0------------>1           point 0 at -1, 1 | point 1 at  1, 1
     *   /|\     |      |
     *    |             |
     *    |--    x    --|           x at 0, 0        | y increases ahead of user at angle == 0, x increases to the right
     *    |             |
     *    |      |     \|/
     *    3<------------2           point 3 at -1,-1 | point 2 at  1,-1
     */
    game_set_vertexfip(0,   -1l << 16l,     1l << 16l); /* -1, 1 */
    game_set_vertexfip(1,    1l << 16l,     1l << 16l); /*  1, 1 */
    game_set_vertexfip(2,    1l << 16l,    -1l << 16l); /*  1,-1 */
    game_set_vertexfip(3,   -1l << 16l,    -1l << 16l); /* -1,-1 */

    game_vertex_max = 4;

    game_set_linedef_ss(0,  0,      1,  0x0000/*flags*/,            0/*sidedef*/);
    game_set_linedef_ss(1,  1,      2,  0x0000/*flags*/,            0/*sidedef*/);
    game_set_linedef_ss(2,  2,      3,  0x0000/*flags*/,            0/*sidedef*/);
    game_set_linedef_ss(3,  3,      0,  0x0000/*flags*/,            0/*sidedef*/);

    game_lineseg_max = 4;

    game_set_sidedef(0,     0/*texture*/,   0/*xoff*/,  0/*yoff*/,  0/*sector*/);

    game_set_sector(0,       1l << 16l/*top*/,      -1l << 16l/*bottom*/,       0/*floor*/,     1/*ceiling*/);

    init_keyboard_irq();

    vga_palette_lseek(0);
    for (x=0;x < 64;x++) vga_palette_write(x,x,x);

    while (1) {
        if (kbdown_test(KBDS_ESCAPE)) break;

        if (kbdown_test(KBDS_UP_ARROW)) {
            const unsigned ga = game_angle >> 5u;
            game_position.x += (int32_t)sin2048fps16_lookup(ga) / 30l;
            game_position.y += (int32_t)cos2048fps16_lookup(ga) / 30l;
        }
        if (kbdown_test(KBDS_DOWN_ARROW)) {
            const unsigned ga = game_angle >> 5u;
            game_position.x -= (int32_t)sin2048fps16_lookup(ga) / 30l;
            game_position.y -= (int32_t)cos2048fps16_lookup(ga) / 30l;
        }
        if (kbdown_test(KBDS_LEFT_ARROW))
            game_angle -= 0x10000l / 60l;
        if (kbdown_test(KBDS_RIGHT_ARROW))
            game_angle += 0x10000l / 60l;

        /* clear screen */
        vga_write_sequencer(0x02/*map mask*/,0xF);
        vga_rep_stosw(vga_state.vga_graphics_ram,0,((320u/4u)*200)/2u);

        /* project and render */
        game_vslice_alloc = 0;
        for (i=0;i < GAME_VSLICE_DRAW;i++) game_vslice_draw[i] = ~0u;

        for (i=0;i < game_vertex_max;i++) {
            /* TODO: 2D rotation based on player angle */
            /* TODO: Perhaps only the line segments we draw */
            game_vertexrot[i].x = game_vertex[i].x - game_position.x;
            game_vertexrot[i].y = game_vertex[i].y - game_position.y;

            {
                const unsigned ga = game_angle >> 5u;
                const int64_t inx = ((int64_t)game_vertexrot[i].x * (int64_t)cos2048fps16_lookup(ga)) - ((int64_t)game_vertexrot[i].y * (int64_t)sin2048fps16_lookup(ga));
                const int64_t iny = ((int64_t)game_vertexrot[i].y * (int64_t)cos2048fps16_lookup(ga)) + ((int64_t)game_vertexrot[i].x * (int64_t)sin2048fps16_lookup(ga));
                game_vertexrot[i].x = (int32_t)(inx >> 15ll);
                game_vertexrot[i].y = (int32_t)(iny >> 15ll);
            }
        }

        for (i=0;i < game_lineseg_max;i++)
            game_project_lineseg(i);

        for (i=0;i < GAME_VSLICE_DRAW;i++) {
            vga_write_sequencer(0x02/*map mask*/,1u << (i & 3u));
            if (game_vslice_draw[i] != (~0u)) {
                __segment vs = FP_SEG(vga_state.vga_graphics_ram);
                unsigned char c;

                vsl = &game_vslice[game_vslice_draw[i]];
                c = (vsl->dist >= (0x10000l>>1l)) ? ((63l << (16l-1l)) / vsl->dist) : 63;
                x2 = (unsigned int)((vsl->floor) < 0 ? 0 : vsl->floor);
                x = (unsigned int)((vsl->ceil) < 0 ? 0 : vsl->ceil);
                if (x2 > 200) x2 = 200;
                if (x > 200) x = 200;
                if (x >= x2) continue;

                o = (i >> 2u) + (x * 80u) + FP_OFF(vga_state.vga_graphics_ram);
                x2 -= x;
                do {
                    *((unsigned char far*)(vs:>o)) = c;
                    o += 80u;
                } while ((--x2) != 0u);
            }
        }

        /* present to screen, flip pages, wait for vsync */
        vga_swap_pages(); /* current <-> next */
        vga_update_disp_cur_page();
        vga_wait_for_vsync(); /* wait for vsync */
    }

    restore_keyboard_irq();
    game_texture_freeall();
}

/*---------------------------------------------------------------------------*/
/* main                                                                      */
/*---------------------------------------------------------------------------*/

int main(int argc,char **argv) {
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
    if (!(vga_state.vga_flags & VGA_IS_VGA)) {
        printf("This game requires VGA\n");
        return 1;
    }
    detect_keyboard();

#if TARGET_MSDOS == 16
# if 0 // not using it yet
    probe_emm();            // expanded memory support
    probe_himem_sys();      // extended memory support
# endif
#endif

    if (argc > 1 && !strcmp(argv[1],"KBTEST")) {
        printf("Keyboard test. Hit keys to see scan codes. ESC to exit.\n");

        init_keyboard_irq();
        while (1) {
            int k = kbd_buf_read();
            if (k >= 0) printf("0x%x\n",k);
            if (k == KBDS_ESCAPE) break;
        }

        printf("Dropping back to DOS.\n");
        unhook_irqs(); // including keyboard
        return 0;
    }

    init_timer_irq();
    init_vga256unchained();

    seq_intro();
    game_loop();

    gen_res_free();
    check_heap();
    unhook_irqs();
    restore_text_mode();

    //debug
    dbg_heap_list();

    return 0;
}

