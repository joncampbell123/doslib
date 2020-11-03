
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
#include <hw/8237/8237.h>
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>
#include <hw/sndsb/sndsb.h>
#include <hw/adlib/adlib.h>
#include <fmt/minipng/minipng.h>

#include "timer.h"
#include "vmode.h"
#include "fonts.h"
#include "vrlimg.h"
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
#include "craptn52.h"
#include "ldwavsn.h"

#include <hw/8042/8042.h>

static unsigned char game_0_tilemap[22*14] = {
/*      01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 */
/* 01 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
/* 02 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
/* 03 */ 7, 7, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7,
/* 04 */ 7, 7, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 7, 7, 7, 7,
/* 05 */ 7, 7, 6, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 6, 7, 7, 7, 7,
/* 06 */ 7, 7, 6, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 6, 7, 7, 7, 7,
/* 07 */ 7, 7, 6, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 6, 7, 7, 7, 7,
/* 08 */ 7, 7, 6, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 6, 7, 7, 7, 7,
/* 09 */ 7, 7, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 7, 7, 7, 7,
/* 10 */ 7, 7, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7,
/* 11 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
/* 12 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
/* 13 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
/* 14 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
};

/* Smiley Wars */
/* TODO: Game start text: "Turn that smile upside down! Go!"
 *       Game text at restart: "You lose!" "You win!" etc.
 *       WAV file of voice shouting "Turn that smile upside down! Go!"
 *       Random chance of shot turning opponent into a pile of poo.
 *       Random chance of opponent turning *you* into a pile of poo.
 *       WAV file when you turn into poo "You lose! Have a crappy day!"
 *       If both player and opponent hit each other and frown, show "Nobody wins! It's a tie!"
 */
/* Intentional bugs (do not fix):
 *   - Shooting the oppenent repeatedly should always reset the game over timer,
 *     allowing the player to possibly infinitely postpone the game reset.
 *     Because n00b mistakes.
 */
void game_0(void) {
    /* sprite slots: smiley 0 (player) and smiley 1 (opponent) */
    const unsigned smiley0=0,smiley1=1;
    const unsigned smileybullet0=2,smileybullet1=3;
    /* sprite images */
    const unsigned smi_smile=1,smi_frown=4,smi_bullet=0;
    /* player state (center) */
    unsigned player_x = 90;
    unsigned player_y = 100;
    unsigned player_dir = 0; // 0=right 1=down 2=left 3=up 4=stop 255=not moving 254=frowning
    struct game_2d player_bullet = {-1,-1};
    struct game_2d player_bulletv = {-1,-1};
    /* opponent state (center) */
    unsigned opp_x = 230;
    unsigned opp_y = 100;
    unsigned opp_dir = 0; // 0=right 1=down 2=left 3=up 4=stop 255=not moving 254=frowning
    unsigned opp_turn = 1;
    uint32_t opp_turn_next = 0;
    uint32_t opp_next_fire = 0;
    uint32_t game_restart = 0;
    struct game_2d opp_bullet = {-1,-1};
    struct game_2d opp_bulletv = {-1,-1};
    /* smiley size */
    const unsigned smilw = 26,smilh = 26,smilox = 32 - 16,smiloy = 32 - 16;
    /* other */
    unsigned int i,amult;
    unsigned char refade=0;
    uint32_t now,pnow;

    if (sin2048fps16_open())
        fatal("cannot open sin2048");

    vga_cur_page = VGA_GAME_PAGE_FIRST;
    vga_set_start_location(vga_cur_page);
    vga_rep_stosw(vga_state.vga_graphics_ram+((320u/4u)*0u),0x0000,((320u/4u)*200u)/2u);
    load_def_pal();

    vga_cur_page = VGA_GAME_PAGE_FIRST;
    vga_next_page = VGA_GAME_PAGE_SECOND;
    vga_state.vga_graphics_ram = orig_vga_graphics_ram + vga_next_page;
    vga_set_start_location(vga_cur_page);

    game_sprite_init();
    game_noscroll_setup();

    load_tiles(TILES_VRAM_OFFSET,0/*default*/,"cr52bt00.png");
    game_spriteimg_load(0,"cr52rn00.vrl");
    game_spriteimg_load(1,"cr52rn01.vrl");
    game_spriteimg_load(2,"cr52rn02.vrl");
    game_spriteimg_load(3,"cr52rn03.vrl");
    game_spriteimg_load(4,"cr52rn04.vrl");

    game_tilecopy(0,0,22,14,game_0_tilemap);

    game_sprite_imgset(smiley0,smi_smile);
    game_sprite_imgset(smiley1,smi_smile);

    game_draw_tiles_2pages(0,0,320,200);

    init_keyboard_irq();
    now = read_timer_counter();

    game_restart = 0;
    opp_next_fire = now + 120; // do not fire for 1 sec, give the player a fair chance

    while (1) {
        pnow = now;
        now = read_timer_counter();

        if (game_restart != 0 && now >= game_restart) {
            game_restart = 0;
            refade = 1;

            opp_next_fire = now + 90; // do not fire for 3/4 sec, give the player a fair chance

            game_sprite_imgset(smiley0,smi_smile);
            game_sprite_imgset(smiley1,smi_smile);

            player_dir = 0;
            opp_dir = 0;
            player_x = 90;
            player_y = 100;
            opp_x = 230;
            opp_y = 100;
        }

        amult = now - pnow;
        if (amult > 4) amult = 4;

        if (kbdown_test(KBDS_ESCAPE)) break;

        if (kbdown_test(KBDS_LEFT_ARROW)) {
            if (player_dir < 5) {
                player_dir = 2; // 0=right 1=down 2=left 3=up 4=stop
                if (player_x > smilw)
                    player_x -= amult * 2u;
            }
        }
        else if (kbdown_test(KBDS_RIGHT_ARROW)) {
            if (player_dir < 5) {
                player_dir = 0; // 0=right 1=down 2=left 3=up 4=stop
                if (player_x < (320-smilw))
                    player_x += amult * 2u;
            }
        }

        if (kbdown_test(KBDS_UP_ARROW)) {
            if (player_dir < 5) {
                player_dir = 3; // 0=right 1=down 2=left 3=up 4=stop
                if (player_y > smilh)
                    player_y -= amult * 2u;
            }
        }
        else if (kbdown_test(KBDS_DOWN_ARROW)) {
            if (player_dir < 5) {
                player_dir = 1; // 0=right 1=down 2=left 3=up 4=stop
                if (player_y < (200-smilh))
                    player_y += amult * 2u;
            }
        }

        if (kbdown_test(KBDS_SPACEBAR)) {
            if (player_bullet.x < 0 && player_dir < 5) {
                player_bullet.x = player_x;
                player_bullet.y = player_y;
                player_bulletv.x = (player_dir == 0/*right*/) ? 2 : (player_dir == 2/*left*/) ? -2 : 0;
                player_bulletv.y = (player_dir == 1/*down*/)  ? 2 : (player_dir == 3/*up*/)   ? -2 : 0;
                game_sprite_imgset(smileybullet0,smi_bullet);
            }
        }

        /* opponent: do not move if frowning (i.e. dead) */
        if (opp_dir == 254) {
        }
        /* opponent: avoid the player if too close */
        else if (now < opp_turn_next) {
            // keep going. The purpose of delaying the check again is to prevent
            // cases where the player can pin the opponent in a corner because
            // the opponent is constantly turning due to player proximity.
        }
        else if (abs(opp_x - player_x) < 50 && abs(opp_y - player_y) < 50) {
            if ((opp_x > player_x && opp_dir == 2) || // approaching from the left
                (opp_x < player_x && opp_dir == 0)) { // approaching from the right
                opp_turn_next = now + 30; // 30/120 or 1/4th a second
                opp_turn = (~opp_turn) + 1;
                if (opp_y < ((200*1)/3) || opp_y > ((200*2)/3))
                    opp_dir = (opp_x > player_x) ? 0 : 2; // reverse direction right/left
                else
                    opp_dir = (opp_dir + opp_turn) & 3;
            }
            else if (
                (opp_y > player_y && opp_dir == 3) || // approaching up from below
                (opp_y < player_y && opp_dir == 1)) { // approaching down from above
                opp_turn_next = now + 30; // 30/120 or 1/4th a second
                opp_turn = (~opp_turn) + 1;
                if (opp_x < ((320*1)/3) || opp_x > ((320*2)/3))
                    opp_dir = (opp_y > player_y) ? 1 : 3; // reverse direction up/down
                else
                    opp_dir = (opp_dir + opp_turn) & 3;
            }
            else {
                opp_turn_next = now + 12; // 12/120 or 1/10th a second
            }
        }
        /* opponent wants to follow player to attack too */
        else {
            if (now >= opp_next_fire && opp_bullet.x < 0 && player_dir != 254/*not dead*/) {
                if ((rand() % 20) == 0) {
                    opp_next_fire = now + 60 + ((uint32_t)rand() % 30ul);
                    if (abs(opp_x - player_x) < 30) {
                        opp_bullet.x = opp_x;
                        opp_bullet.y = opp_y;
                        opp_bulletv.x = 0;
                        opp_bulletv.y = (opp_y > player_y) ? -2/*up*/ : 2/*down*/;
                        game_sprite_imgset(smileybullet1,smi_bullet);
                    }
                    else if (abs(opp_y - player_y) < 30) {
                        opp_bullet.x = opp_x;
                        opp_bullet.y = opp_y;
                        opp_bulletv.x = (opp_x > player_x) ? -2/*left*/ : 2/*right*/;
                        opp_bulletv.y = 0;
                        game_sprite_imgset(smileybullet1,smi_bullet);
                    }
                }
            }

            if (now >= (opp_turn_next + 15u) && (abs(opp_x - player_x) > 80 && abs(opp_y - player_y) > 80)) {
                if ((opp_x > player_x && opp_dir == 2) || // right of player, heading left
                    (opp_x < player_x && opp_dir == 0) || // left of player, heading right
                    (opp_y > player_y && opp_dir == 3) || // below player, heading up
                    (opp_y < player_y && opp_dir == 1)) { // above player, heading down
                    // keep going
                }
                else {
                    opp_turn_next = now + 15u + ((uint32_t)rand() % (uint32_t)15ul);
                    opp_dir = (opp_dir + opp_turn) & 3;
                }
            }
        }

        switch (opp_dir) {
            case 0: // moving right
                if (opp_x < (320-smilw))
                    opp_x += amult * 2u;
                else
                    opp_dir = (opp_dir + opp_turn) & 3;
                break;
            case 1: // moving down
                if (opp_y < (200-smilw))
                    opp_y += amult * 2u;
                else
                    opp_dir = (opp_dir + opp_turn) & 3;
                break;
            case 2: // moving left
                if (opp_x > smilw)
                    opp_x -= amult * 2u;
                else
                    opp_dir = (opp_dir + opp_turn) & 3;
                break;
            case 3: // moving up
                if (opp_y > smilh)
                    opp_y -= amult * 2u;
                else
                    opp_dir = (opp_dir + opp_turn) & 3;
                break;
        }

        if (player_bullet.x >= 0) {
            player_bullet.x += player_bulletv.x * (int)amult;
            player_bullet.y += player_bulletv.y * (int)amult;
            if (player_bullet.x >= (320-(smilw/2)) ||
                player_bullet.x <= smilox ||
                player_bullet.y >= (200-(smilh/2)) ||
                player_bullet.y <= smiloy) {
                player_bullet.x = -1;
                game_sprite_hide(smileybullet0);
            }
            else if (abs(player_bullet.x - opp_x) < 24 && abs(player_bullet.y - opp_y) < 24) {
                opp_dir = 254; // HIT!
                game_sprite_imgset(smiley1,smi_frown);
                player_bullet.x = -1;
                game_sprite_hide(smileybullet0);
                game_restart = now + 120*3;
            }
        }

        if (opp_bullet.x >= 0) {
            opp_bullet.x += opp_bulletv.x * (int)amult;
            opp_bullet.y += opp_bulletv.y * (int)amult;
            if (opp_bullet.x >= (320-(smilw/2)) ||
                opp_bullet.x <= smilox ||
                opp_bullet.y >= (200-(smilh/2)) ||
                opp_bullet.y <= smiloy) {
                opp_bullet.x = -1;
                game_sprite_hide(smileybullet1);
            }
            else if (abs(opp_bullet.x - player_x) < 24 && abs(opp_bullet.y - player_y) < 24) {
                player_dir = 254; // HIT!
                game_sprite_imgset(smiley0,smi_frown);
                opp_bullet.x = -1;
                game_sprite_hide(smileybullet1);
                game_restart = now + 120*3;
            }
        }

        if (player_bullet.x >= 0)
            game_sprite_position(smileybullet0,player_bullet.x - smilox,player_bullet.y - smiloy);
        if (opp_bullet.x >= 0)
            game_sprite_position(smileybullet1,opp_bullet.x - smilox,opp_bullet.y - smiloy);

        game_sprite_position(smiley0,player_x - smilox,player_y - smiloy);
        game_sprite_position(smiley1,opp_x    - smilox,opp_y    - smiloy);

        game_update_sprites();

        if (refade) {
            /* fade out */
            outp(0x3C7,0); // prepare to read from 0
            for (i=0;i < 768;i++) common_tmp_small[i] = inp(0x3C9);

            for (amult=0;amult <= 16;amult++) {
                vga_wait_for_vsync(); /* wait for vsync */
                outp(0x3C8,0);
                for (i=0;i < 768;i++) outp(0x3C9,(common_tmp_small[i]*(16-amult))>>4);
            }
        }

        vga_swap_pages(); /* current <-> next */
        vga_update_disp_cur_page();
        vga_wait_for_vsync(); /* wait for vsync */

        if (refade) {
            refade = 0;

            /* fade in */
            for (amult=0;amult <= 16;amult++) {
                vga_wait_for_vsync(); /* wait for vsync */
                outp(0x3C8,0);
                for (i=0;i < 768;i++) outp(0x3C9,(common_tmp_small[i]*amult)>>4);
            }
        }
    }

    restore_keyboard_irq();
    game_sprite_exit();
}

