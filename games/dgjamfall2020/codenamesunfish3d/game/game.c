
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
#include "dbgheap.h"
#include "fontbmp.h"
#include "unicode.h"
#include "commtmp.h"
#include "sin2048.h"
#include "vrldraw.h"
#include "seqcomm.h"
#include "dumbpack.h"
#include "fzlibdec.h"
#include "fataexit.h"
#include "sorcpack.h"
#include "rotozoom.h"
#include "seqcanvs.h"

void gen_res_free(void) {
    seq_com_cleanup();
    sin2048fps16_free();
    font_bmp_free(&arial_small);
    font_bmp_free(&arial_medium);
    font_bmp_free(&arial_large);
    dumbpack_close(&sorc_pack);
}

/*---------------------------------------------------------------------------*/
/* introduction sequence                                                     */
/*---------------------------------------------------------------------------*/

const struct seqanim_event_t seq_intro_events[] = {
//  what                    param1,     param2,     params
    {SEQAEV_CALLBACK,       RZOOM_NONE, 0,          (const char*)seq_com_load_rotozoom}, // clear rotozoomer slot 0
    {SEQAEV_CALLBACK,       0,          3,          (const char*)seq_com_load_games_chars}, // load games chars
    {SEQAEV_CALLBACK,       0,          0,          (const char*)seq_com_put_solidcolor}, // canvas layer 0 (param2) solid fill 0 (param1)
    {SEQAEV_CALLBACK,       VRAMIMG_TMPLIE|(0x60ul<<8ul),0x8000ul,(const char*)seq_com_load_vram_image}, // load tmplie and offset by 0x60 load to 0x8000 in planar unchained VRAM for bitblt
    {SEQAEV_CALLBACK,       VRAMIMG_TWNCNTR|(0x80ul<<8ul),0xC000ul,(const char*)seq_com_load_vram_image}, // load tmplie and offset by 0x80 load to 0xC000 in planar unchained VRAM for bitblt
    {SEQAEV_CALLBACK,       VRAMIMG_TWNCNTR,0,      (const char*)seq_com_put_vram_image}, // take VRAMIMG_TWNCNTR (param1) put into canvas layer 0 (param2) via BitBlt
    {SEQAEV_CALLBACK,       1,          (0x00ul | (0x100ul << 16ul)),(const char*)seq_com_save_palette}, // save VGA palette and (param1) blank it too

    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMESCHARS_VRLBASE+3ul)/*vrl*/)|(130ul/*x*/<<10ul)|(/*y*/120ul<<20ul)|(1ul/*hflip*/<<30ul),4,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMESCHARS_VRLBASE+2ul)/*vrl*/)|(100ul/*x*/<<10ul)|(/*y*/40ul<<20ul)|(1ul/*hflip*/<<30ul),3,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMESCHARS_VRLBASE+1ul)/*vrl*/)|(140ul/*x*/<<10ul)|(/*y*/76ul<<20ul)|(1ul/*hflip*/<<30ul),2,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       (SORC_VRL_GAMESCHARS_VRLBASE/*vrl*/)|(280ul/*x*/<<10ul)|(/*y*/70ul<<20ul)|(0ul/*hflip*/<<30ul),1,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_PAUSE,          0,          0,          NULL}, //let it render
    {SEQAEV_CALLBACK,       0,          0,          (const char*)seq_com_fadein_saved_palette}, // fade in saved VGA palette

    // mouth, talking, main char
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHRMOUTH_BASE+0ul)/*vrl*/)|(285ul/*x*/<<10ul)|(/*y*/85ul<<20ul)|(0ul/*hflip*/<<30ul),5,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_TEXT_COLOR,     0,          0,          NULL}, //default
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Welcome one and all to a new day\nin this world."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "A day where we all mill about in this world\ndoing our thing as a society" "\x10\x30" "----"},
    // no fade out, interrupted speaking

    // mouth, talking, secondary char, replace on canvas with new position
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHRMOUTH_BASE+0ul)/*vrl*/)|(112ul/*x*/<<10ul)|(/*y*/53ul<<20ul)|(1ul/*hflip*/<<30ul),5,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_TEXT_COLOR,     0xFFFF00ul, 0,          NULL}, //RRGGBB yellow
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "But what is our purpose in this game?"},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    // mouth, talking, main char
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHRMOUTH_BASE+0ul)/*vrl*/)|(285ul/*x*/<<10ul)|(/*y*/85ul<<20ul)|(0ul/*hflip*/<<30ul),5,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_TEXT_COLOR,     0,          0,          NULL}, //default
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Hm? What?"},
    {SEQAEV_WAIT,           120*1,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    // mouth, talking, secondary char
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHROHCOMEON+0ul)/*vrl*/)|(100ul/*x*/<<10ul)|(/*y*/40ul<<20ul)|(1ul/*hflip*/<<30ul),3,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHRMOUTH_BASE+0ul)/*vrl*/)|(112ul/*x*/<<10ul)|(/*y*/53ul<<20ul)|(1ul/*hflip*/<<30ul),5,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_TEXT_COLOR,     0xFFFF00ul, 0,          NULL}, //RRGGBB yellow
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Our purpose? What is the story? The goal?"},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    // mouth, talking, main char
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMESCHARS_VRLBASE+2ul)/*vrl*/)|(100ul/*x*/<<10ul)|(/*y*/40ul<<20ul)|(1ul/*hflip*/<<30ul),3,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHRMOUTH_BASE+0ul)/*vrl*/)|(285ul/*x*/<<10ul)|(/*y*/85ul<<20ul)|(0ul/*hflip*/<<30ul),5,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_TEXT_COLOR,     0,          0,          NULL}, //default
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Good question! "},

    /* turn around, to enter temple */
    {SEQAEV_CALLBACK,       (SORC_VRL_GAMESCHARS_VRLBASE/*vrl*/)|(280ul/*x*/<<10ul)|(/*y*/70ul<<20ul)|(1ul/*hflip*/<<30ul),1,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    // mouth, talking, main char
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHRMOUTH_BASE+0ul)/*vrl*/)|(300ul/*x*/<<10ul)|(/*y*/85ul<<20ul)|(1ul/*hflip*/<<30ul),5,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_TEXT,           0,          0,          "I'll ask "},

    /* begins moving */
    {SEQAEV_CALLBACK,       0,          1,          (const char*)seq_com_begin_anim_move},
    {SEQAEV_CALLBACK,       (40ul/*x*/)|(0/*y*/<<10ul)|(60ul/*duration ticks*/<<20ul),1,(const char*)seq_com_do_anim_move},
    {SEQAEV_CALLBACK,       0,          5,          (const char*)seq_com_begin_anim_move},
    {SEQAEV_CALLBACK,       (40ul/*x*/)|(0/*y*/<<10ul)|(60ul/*duration ticks*/<<20ul),5,(const char*)seq_com_do_anim_move},

    {SEQAEV_TEXT,           0,          0,          "the programmer."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},
    {SEQAEV_PAUSE,          0,          0,          NULL},

    {SEQAEV_CALLBACK,       0,          (0x00ul | (0x100ul << 16ul)),(const char*)seq_com_save_palette}, // save VGA palette and (param1)
    {SEQAEV_CALLBACK,       0,          1,          (const char*)seq_com_fadein_saved_palette}, // fade out saved VGA palette

    /* walk to a door. screen fades out, fades in to room with only the one person. */

    {SEQAEV_CALLBACK,       0,          1,          (const char*)seq_com_put_nothing}, // clear canvas layer 1
    {SEQAEV_CALLBACK,       0,          2,          (const char*)seq_com_put_nothing}, // clear canvas layer 2
    {SEQAEV_CALLBACK,       0,          3,          (const char*)seq_com_put_nothing}, // clear canvas layer 3
    {SEQAEV_CALLBACK,       0,          4,          (const char*)seq_com_put_nothing}, // clear canvas layer 4
    {SEQAEV_CALLBACK,       0,          5,          (const char*)seq_com_put_nothing}, // clear canvas layer 5
    {SEQAEV_PAUSE,          0,          0,          NULL}, // render

    {SEQAEV_CALLBACK,       RZOOM_WXP,  0,          (const char*)seq_com_load_rotozoom}, // load rotozoomer slot 0 (param2) with 256x256 Windows XP background (param1)
    {SEQAEV_CALLBACK,       0,          0,          (const char*)seq_com_init_mr_woo}, // initialize code and data for Mr. Wooo Sorcerer (load palette) (Future dev: param1 select func)
    {SEQAEV_CALLBACK,       0,          1,          (const char*)seq_com_load_mr_woo_anim}, // load anim1 (required param2==1)
    {SEQAEV_CALLBACK,       1,          1,          (const char*)seq_com_load_mr_woo_anim}, // load uhhh (required param2==1)
    {SEQAEV_CALLBACK,       2,          1,          (const char*)seq_com_load_mr_woo_anim}, // load anim2 (required param2==1)
    {SEQAEV_CALLBACK,       VRAMIMG_TMPLIE,0,       (const char*)seq_com_put_vram_image}, // take VRAMIMG_TMPLIE (param1) put into canvas layer 0 (param2) via BitBlt
    {SEQAEV_CALLBACK,       (SORC_VRL_GAMESCHARS_VRLBASE/*vrl*/)|(100ul/*x*/<<10ul)|(/*y*/120ul<<20ul)|(1ul/*hflip*/<<30ul),1,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       1,          (0x00ul | (0x60ul << 16ul)),(const char*)seq_com_save_palette}, // save VGA palette new colors and (param1) blank it too. 0x00-0x3F new rotozoomer, 0x40-0x5F Mr. Wooo

    {SEQAEV_PAUSE,          0,          0,          NULL}, // render

    // mouth, talking, main char
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHRMOUTH_BASE+0ul)/*vrl*/)|(120ul/*x*/<<10ul)|(/*y*/135ul<<20ul)|(1ul/*hflip*/<<30ul),5,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_CALLBACK,       0,          0,          (const char*)seq_com_fadein_saved_palette}, // fade in saved VGA palette
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Hello, games programmer?"},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    /* cut to Mr. Woo Sorcerer in front of a demo effect */

    {SEQAEV_CALLBACK,       0,          5,          (const char*)seq_com_put_nothing}, // clear canvas layer 5
    {SEQAEV_CALLBACK,       0,          0,          (const char*)seq_com_put_rotozoom}, // put rotozoomer slot 0 (param1) to canvas layer 0 (param2)
    {SEQAEV_CALLBACK,       0,          1,          (const char*)seq_com_put_mr_woo_anim}, // put anim1 (param1) to canvas layer 1 (param2)
    {SEQAEV_TEXT_COLOR,     0x00FFFFul, 0,          NULL}, //RRGGBB cyan
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_PAUSE,          0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "I am a super awesome programmer! I write\nclever highly optimized code! Woooooooo!"},
    {SEQAEV_WAIT,           120*3,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},
    {SEQAEV_PAUSE,          0,          0,          NULL},
    {SEQAEV_CALLBACK,       0,          1,          (const char*)seq_com_put_nothing}, // clear canvas layer 1
    {SEQAEV_CALLBACK,       0,          2,          (const char*)seq_com_load_mr_woo_anim}, // unload anim1 (param2==2)

    /* game character and room */

    // mouth, talking, main char
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHRMOUTH_BASE+0ul)/*vrl*/)|(120ul/*x*/<<10ul)|(/*y*/135ul<<20ul)|(1ul/*hflip*/<<30ul),5,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_CALLBACK,       VRAMIMG_TMPLIE,0,       (const char*)seq_com_put_vram_image}, // take VRAMIMG_TMPLIE (param1) put into canvas layer 0 (param2) via BitBlt
    {SEQAEV_CALLBACK,       (SORC_VRL_GAMESCHARS_VRLBASE/*vrl*/)|(100ul/*x*/<<10ul)|(/*y*/120ul<<20ul)|(1ul/*hflip*/<<30ul),1,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_TEXT_COLOR,     0,          0,          NULL}, //default
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_PAUSE,          0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Oh super awesome games programmer.\nWhat is our purpose in this game?"},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},
    {SEQAEV_CALLBACK,       0,          5,          (const char*)seq_com_put_nothing}, // clear canvas layer 5. Hide the mouth so it doesn't look like the game froze mid-animation during load.
    {SEQAEV_PAUSE,          0,          0,          NULL},

    /* Mr. Woo Sorcerer, blank background, downcast */

    {SEQAEV_CALLBACK,       0,          5,          (const char*)seq_com_put_nothing}, // clear canvas layer 5
    {SEQAEV_CALLBACK,       0,          0,          (const char*)seq_com_put_solidcolor}, // canvas layer 0 (param2) solid fill 0 (param1)
    {SEQAEV_CALLBACK,       1,          1,          (const char*)seq_com_put_mr_woo_anim}, // put "uhhhh" (param1) to canvas layer 1 (param2)
    {SEQAEV_TEXT_COLOR,     0x00FFFFul, 0,          NULL}, //RRGGBB cyan
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_CALLBACK,       RZOOM_ATPB, 0,          (const char*)seq_com_load_rotozoom}, // load rotozoomer slot 0 (param2) with 256x256 Second Reality "atomic playboy" (param1)
    {SEQAEV_PAUSE,          0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Uhm" "\x10\x29" ".........."},
    // no fade out, abrupt jump to next part

    /* Begins waving hands, another demo effect appears */

    {SEQAEV_CALLBACK,       2,          1,          (const char*)seq_com_put_mr_woo_anim}, // put anim2 (param1) to canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       1,          2,          (const char*)seq_com_load_mr_woo_anim}, // unload uhhh (param2==2)
    {SEQAEV_CALLBACK,       0,          0,          (const char*)seq_com_put_rotozoom}, // put rotozoomer slot 0 (param1) to canvas layer 0 (param2)
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_PAUSE,          0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "I am super awesome programmer. I write\nawesome optimized code! Wooooooooo!"},
    {SEQAEV_WAIT,           120*5,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},
    {SEQAEV_CALLBACK,       0,          (0x00ul | (0x100ul << 16ul)),(const char*)seq_com_save_palette}, // save VGA palette and (param1)
    {SEQAEV_CALLBACK,       0,          1,          (const char*)seq_com_fadein_saved_palette}, // fade out saved VGA palette
    {SEQAEV_CALLBACK,       VRAMIMG_TWNCNTR,0,      (const char*)seq_com_put_vram_image}, // take VRAMIMG_TWNCNTR (param1) put into canvas layer 0 (param2) via BitBlt
    {SEQAEV_CALLBACK,       RZOOM_NONE, 0,          (const char*)seq_com_load_rotozoom}, // slot 0 we're done with the rotozoomer, free it
    {SEQAEV_CALLBACK,       0,          1,          (const char*)seq_com_put_nothing}, // clear canvas layer 1
    {SEQAEV_CALLBACK,       2,          2,          (const char*)seq_com_load_mr_woo_anim}, // unload anim2 (param2==2)

    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHRMOUTH_BASE+0ul)/*vrl*/)|(325ul/*x*/<<10ul)|(/*y*/85ul<<20ul)|(0ul/*hflip*/<<30ul),5,(const char*)seq_com_put_mr_vrl},
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMESCHARS_VRLBASE+3ul)/*vrl*/)|(130ul/*x*/<<10ul)|(/*y*/120ul<<20ul)|(1ul/*hflip*/<<30ul),4,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMESCHARS_VRLBASE+2ul)/*vrl*/)|(100ul/*x*/<<10ul)|(/*y*/40ul<<20ul)|(1ul/*hflip*/<<30ul),3,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMESCHARS_VRLBASE+1ul)/*vrl*/)|(140ul/*x*/<<10ul)|(/*y*/76ul<<20ul)|(1ul/*hflip*/<<30ul),2,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       (SORC_VRL_GAMESCHARS_VRLBASE/*vrl*/)|(320ul/*x*/<<10ul)|(/*y*/70ul<<20ul)|(0ul/*hflip*/<<30ul),1,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_PAUSE,          0,          0,          NULL},
    {SEQAEV_CALLBACK,       0,          0,          (const char*)seq_com_save_palette}, // save nothing of the VGA palette but reset anim timer
    {SEQAEV_CALLBACK,       0,          0,          (const char*)seq_com_fadein_saved_palette}, // fade in saved VGA palette

    {SEQAEV_CALLBACK,       0,          1,          (const char*)seq_com_begin_anim_move},
    {SEQAEV_CALLBACK,       ((-40l&0x3FFul)/*x*/)|(0/*y*/<<10ul)|(60ul/*duration ticks*/<<20ul),1,(const char*)seq_com_do_anim_move},
    {SEQAEV_CALLBACK,       0,          5,          (const char*)seq_com_begin_anim_move},
    {SEQAEV_CALLBACK,       ((-40l&0x3FFul)/*x*/)|(0/*y*/<<10ul)|(60ul/*duration ticks*/<<20ul),5,(const char*)seq_com_do_anim_move},

    /* game character returns outside */
    {SEQAEV_TEXT_COLOR,     0,          0,          NULL}, //default
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Alright,\x01 there is no story. So I'll just make\none up to get things started. \x02\x11\x21*ahem*"},
    {SEQAEV_WAIT,           120/4,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "There once was a master programmer with\nincredible programming skills."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "But he also had an incredible ego and was\nan incredible perfectionist."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "So he spent his life writing incredibly\noptimized clever useless code."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Then one day he ate one too many nachos\nand died of a heart attack. The end."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    /* someone in the crowd */

    // mouth, talking, secondary char
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHROHCOMEON+0ul)/*vrl*/)|(100ul/*x*/<<10ul)|(/*y*/40ul<<20ul)|(1ul/*hflip*/<<30ul),3,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHRMOUTH_BASE+0ul)/*vrl*/)|(112ul/*x*/<<10ul)|(/*y*/53ul<<20ul)|(1ul/*hflip*/<<30ul),5,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_TEXT_COLOR,     0xFFFF00ul, 0,          NULL}, //RRGGBB yellow
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Oh come on!\x01 That's just mean!"},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    /* game character */

    // mouth, talking, main char
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMESCHARS_VRLBASE+2ul)/*vrl*/)|(100ul/*x*/<<10ul)|(/*y*/40ul<<20ul)|(1ul/*hflip*/<<30ul),3,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHRMOUTH_BASE+0ul)/*vrl*/)|(285ul/*x*/<<10ul)|(/*y*/85ul<<20ul)|(0ul/*hflip*/<<30ul),5,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_TEXT_COLOR,     0,          0,          NULL}, //default
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Okay... okay...\x01 he spends his life writing\nincredible useless optimized code."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "He creates whole OSes but cares not\nfor utility and end user."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "He creates whole game worlds, but cares\nnot for the inhabitants."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "So he lives to this day writing... \x01well...\x01\nnothing of significance. Just woo."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Since the creator of this game doesn't seem\nto care for us or the story, we're on our own."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "So let's go make our own mini games so that\nthis game has something the user can play."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "I made a 3D maze-like overworld that\nconnects them all together for the user."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Alright, let's get started!"},
    {SEQAEV_WAIT,           120*1,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

/* screen fades out. Hammering, sawing, construction noises. */

    {SEQAEV_CALLBACK,       0,          0,          (const char*)seq_com_save_palette}, // save nothing of the VGA palette but reset anim timer
    {SEQAEV_CALLBACK,       0,          1,          (const char*)seq_com_fadein_saved_palette}, // fade out saved VGA palette

    {SEQAEV_END}
};

void seq_intro(void) {
#define ANIM_HEIGHT         168
#define ANIM_TEXT_TOP       168
#define ANIM_TEXT_LEFT      5
#define ANIM_TEXT_RIGHT     310
#define ANIM_TEXT_BOTTOM    198
    struct seqanim_t *sanim;
    int c;

    seq_com_anim_h = ANIM_HEIGHT;

    /* if we load this now, seqanim can automatically use it */
    if (font_bmp_do_load_arial_small())
        fatal("arial");
    if (font_bmp_do_load_arial_medium())
        fatal("arial");

    /* seqanim rotozoomer needs sin2048 */
    if (sin2048fps16_open())
        fatal("cannot open sin2048");

    if ((sanim=seqanim_alloc()) == NULL)
        fatal("seqanim");
    if (seqanim_alloc_canvas(sanim,32))
        fatal("seqanim");

    sanim->next_event = read_timer_counter();

    sanim->events = seq_intro_events;

    sanim->text.home_x = sanim->text.x = ANIM_TEXT_LEFT;
    sanim->text.home_y = sanim->text.y = ANIM_TEXT_TOP;
    sanim->text.end_x = ANIM_TEXT_RIGHT;
    sanim->text.end_y = ANIM_TEXT_BOTTOM;

    sanim->canvas_obj_count = 1;

    /* canvas obj #0: black fill */
    {
        struct seqcanvas_layer_t *c = &(sanim->canvas_obj[0]);
        c->rop.msetfill.h = ANIM_HEIGHT;
        c->rop.msetfill.c = 0;
        c->what = SEQCL_MSETFILL;
    }

    seqanim_set_redraw_everything_flag(sanim);

    while (seqanim_running(sanim)) {
        if (kbhit()) {
            c = getch();
            if (c == 27)
                break;
            else if (c == ' ') {
                seqanim_set_time(sanim,read_timer_counter());
                seqanim_hurryup(sanim);
            }
        }

        seqanim_set_time(sanim,read_timer_counter());
        seqanim_step(sanim);
        seqanim_redraw(sanim);
    }

    seqanim_free(&sanim);
#undef ANIM_HEIGHT
}

/* ------------- */

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
# if 0 // not using it yet
    probe_emm();            // expanded memory support
    probe_himem_sys();      // extended memory support
# endif
#endif

    init_timer_irq();
    init_vga256unchained();

    seq_intro();

    gen_res_free();
    check_heap();
    unhook_irqs();
    restore_text_mode();

    //debug
    dbg_heap_list();

    return 0;
}

