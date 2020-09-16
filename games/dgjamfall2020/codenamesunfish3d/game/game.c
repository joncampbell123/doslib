
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

#define GAMERC_MAPW         64
#define GAMERC_MAPH         64

struct gamerc_elem_t {
    uint8_t         type;
    uint8_t         flags;
};

#define GRCMF_OPAQUE                (1u << 0u)

#define GRCMXY(x,y)                 (((y) * GAMERC_MAPW) + (x))

#define GRC_STD_WALL_HEIGHT         ((uint32_t)128ul << (uint32_t)16ul)
#define GRC_STD_TEXTURE_HEIGHT      ((uint32_t)64ul << (uint32_t)16ul)

struct gamerc_elem_t                gamerc_map[GAMERC_MAPW * GAMERC_MAPH];

#include <math.h>

void game_loop(void) {
    unsigned int i;

    /* seqanim rotozoomer needs sin2048 */
    if (sin2048fps16_open())
        fatal("cannot open sin2048");

    for (i=0;i < GAMERC_MAPW;i++) {
        gamerc_map[GRCMXY(i,0)].type = 1;
        gamerc_map[GRCMXY(i,0)].flags = GRCMF_OPAQUE;

        gamerc_map[GRCMXY(i,GAMERC_MAPH-1)].type = 1;
        gamerc_map[GRCMXY(i,GAMERC_MAPH-1)].flags = GRCMF_OPAQUE;
    }

    for (i=0;i < GAMERC_MAPH;i++) {
        gamerc_map[GRCMXY(0,i)].type = 1;
        gamerc_map[GRCMXY(0,i)].flags = GRCMF_OPAQUE;

        gamerc_map[GRCMXY(GAMERC_MAPW-1,i)].type = 1;
        gamerc_map[GRCMXY(GAMERC_MAPW-1,i)].flags = GRCMF_OPAQUE;
    }

    for (i=0;i < GAMERC_MAPW;i += 2) {
        gamerc_map[GRCMXY(i,8)].type = 1;
        gamerc_map[GRCMXY(i,8)].flags = GRCMF_OPAQUE;
    }

    init_keyboard_irq();

/////////// REWRITE THIS USING FIXED POINT. This isn't even my code, just something to study.
    {
        unsigned int x,y,w=320;
        double cameraX,planeX,planeY/*camera plane direction*/,dirX,dirY/*player direction*/,posX,posY/*player pos*/;
        double rayDirX,rayDirY,sideDistX,sideDistY,deltaDistX,deltaDistY,perpWallDist,wallX;
        int mapX,mapY,stepX,stepY,h;
        unsigned char color;
        int texWidth = 64;
        double angle = 0;
        char hit,side;
        int texX;

        vga_palette_lseek(0);
        for (x=0;x < 64;x++) vga_palette_write(x,x,x);

        posX = 4.0;
        posY = 4.0;

        while (1) {
            if (kbdown_test(KBDS_ESCAPE)) break;

            if (kbdown_test(KBDS_LEFT_ARROW))
                angle -= (3.14 * 2.0 * 4) / 360.0;
            if (kbdown_test(KBDS_RIGHT_ARROW))
                angle += (3.14 * 2.0 * 4) / 360.0;

            planeX = sin(angle + (3.14 / 2.0));
            planeY = cos(angle + (3.14 / 2.0)) * 0.66;
            dirX = sin(angle);
            dirY = cos(angle);

            if (kbdown_test(KBDS_UP_ARROW)) {
                posX += dirX / 16;
                posY += dirY / 16;
            }
            if (kbdown_test(KBDS_DOWN_ARROW)) {
                posX -= dirX / 16;
                posY -= dirY / 16;
            }

            vga_write_sequencer(0x02/*map mask*/,0xF);
            vga_rep_stosw(vga_state.vga_graphics_ram,0,((320u/4u)*200)/2u);

            for (x=0;x < w;x++) {
                cameraX = (double)(2 * x) / (double)w - 1.0;
                rayDirX = dirX + (planeX * cameraX);
                rayDirY = dirY + (planeY * cameraX);
                mapX = (int)floor(posX);
                mapY = (int)floor(posY);
                deltaDistX = sqrt(1 + (rayDirY * rayDirY) / (rayDirX * rayDirX));
                deltaDistY = sqrt(1 + (rayDirX * rayDirX) / (rayDirY * rayDirY));
                side = 0;
                hit = 0;

                if (rayDirX < 0)
                {
                    stepX = -1;
                    sideDistX = (posX - mapX) * deltaDistX;
                }
                else
                {
                    stepX = 1;
                    sideDistX = (mapX + 1.0 - posX) * deltaDistX;
                }
                if (rayDirY < 0)
                {
                    stepY = -1;
                    sideDistY = (posY - mapY) * deltaDistY;
                }
                else
                {
                    stepY = 1;
                    sideDistY = (mapY + 1.0 - posY) * deltaDistY;
                }

                //perform DDA
                while (hit == 0)
                {
                    //jump to next map square, OR in x-direction, OR in y-direction
                    if (sideDistX < sideDistY)
                    {
                        sideDistX += deltaDistX;
                        mapX += stepX;
                        side = 0;
                    }
                    else
                    {
                        sideDistY += deltaDistY;
                        mapY += stepY;
                        side = 1;
                    }
                    //Check if ray has hit a wall
                    if (gamerc_map[GRCMXY(mapX,mapY)].type != 0) hit = 1;
                }

                //Calculate distance of perpendicular ray (Euclidean distance will give fisheye effect!)
                if (side == 0) perpWallDist = (mapX - posX + (1 - stepX) / 2) / rayDirX;
                else           perpWallDist = (mapY - posY + (1 - stepY) / 2) / rayDirY;

                if (side == 0) wallX = posY + perpWallDist * rayDirY;
                else           wallX = posX + perpWallDist * rayDirX;
                wallX -= floor(wallX);

                //x coordinate on the texture
                texX = (int)(wallX * (double)(texWidth));
                if(side == 0 && rayDirX > 0) texX = texWidth - texX - 1;
                if(side == 1 && rayDirY < 0) texX = texWidth - texX - 1;

                h = (int)(2048 / (perpWallDist * 18));
                if (h > 63) h = 63;
                color = h;

                if (texX & 4) color >>= 1u; 

                h = (int)(160 / perpWallDist);
                if (h > 200) h = 200;

                vga_write_sequencer(0x02/*map mask*/,1u << (x & 3u));

                y = (200u-h)/2u;
                while (y < ((200u+h)/2u)) {
                    vga_state.vga_graphics_ram[(y*(320u/4u))+(x>>2u)] = color;
                    y++;
                }
            }

            vga_swap_pages(); /* current <-> next */
            vga_update_disp_cur_page();
            vga_wait_for_vsync(); /* wait for vsync */
        }
    }

    restore_keyboard_irq();
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

