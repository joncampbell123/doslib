
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

struct game_2dsidedef_t {
    uint8_t                 texture;
    int8_t                  xoffset,yoffset;
    uint8_t                 sector;                     /* which sector this faces */
};

#define GAME_SIDEDEFS       128
struct game_2dsidedef_t     game_sidedef[GAME_SIDEDEFS];

struct game_2dsector_t {
    int16_t                 top,bottom;
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

static inline void game_set_sidedef(const unsigned i,const unsigned tex,int8_t xoff,int8_t yoff,unsigned int sector) {
    game_sidedef[i].texture = tex;
    game_sidedef[i].xoffset = xoff;
    game_sidedef[i].yoffset = yoff;
    game_sidedef[i].sector = sector;
}

void game_loop(void) {
    unsigned int i;
    unsigned int x;

    /* seqanim rotozoomer needs sin2048 */
    if (sin2048fps16_open())
        fatal("cannot open sin2048");

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

    game_set_linedef_ss(0,  0,      1,  0x0000/*flags*/,            0/*sidedef*/);
    game_set_linedef_ss(1,  1,      2,  0x0000/*flags*/,            0/*sidedef*/);
    game_set_linedef_ss(2,  2,      3,  0x0000/*flags*/,            0/*sidedef*/);
    game_set_linedef_ss(3,  3,      0,  0x0000/*flags*/,            0/*sidedef*/);

    game_set_sidedef(0,     0/*texture*/,   0/*xoff*/,  0/*yoff*/,  0/*sector*/);

    init_keyboard_irq();

    vga_palette_lseek(0);
    for (x=0;x < 64;x++) vga_palette_write(x,x,x);

    while (1) {
        if (kbdown_test(KBDS_ESCAPE)) break;

        /* clear screen */
        vga_write_sequencer(0x02/*map mask*/,0xF);
        vga_rep_stosw(vga_state.vga_graphics_ram,0,((320u/4u)*200)/2u);

        /*  */

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

