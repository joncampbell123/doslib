
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

void my_unhook_irq(void) {
    sound_blaster_stop_playback();
    sound_blaster_unhook_irq();
}

void gen_res_free(void) {
    game_spriteimg_free();
    sin2048fps16_free();

//    seq_com_cleanup();
//    font_bmp_free(&arial_small);
//    font_bmp_free(&arial_medium);
//    font_bmp_free(&arial_large);
//    dumbpack_close(&sorc_pack);

    sound_blaster_stop_playback();
    sound_blaster_unhook_irq();
    free_sound_blaster_dma();
    if (sound_blaster_ctx != NULL) {
        sndsb_free_card(sound_blaster_ctx);
        sound_blaster_ctx = NULL;
    }
}

int main(int argc,char **argv) {
    unsigned char done;
    int sel;

    memset(game_spriteimg,0,sizeof(game_spriteimg));

    (void)argc;
    (void)argv;

    probe_dos();
	cpu_probe();
    if (cpu_basic_level < 3) {
        printf("This game requires a 386 or higher\n");
        return 1;
    }

	if (!probe_8237()) {
		printf("Chip not present. Your computer might be 2010-era hardware that dropped support for it.\n");
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
    if (!init_sndsb()) { // will only fail if lib init fails, will succeed whether or not there is Sound Blaster
        printf("Sound Blaster lib init fail.\n");
        return 1;
    }
	if (init_adlib()) { // may fail if no OPL at 388h
        unsigned int i;

        printf("Adlib FM detected\n");

        memset(adlib_fm,0,sizeof(adlib_fm));
        memset(&adlib_reg_bd,0,sizeof(adlib_reg_bd));
        for (i=0;i < adlib_fm_voices;i++) {
            struct adlib_fm_operator *f;
            f = &adlib_fm[i].mod;
            f->ch_a = f->ch_b = 1; f->ch_c = f->ch_d = 0;
            f = &adlib_fm[i].car;
            f->ch_a = f->ch_b = 1; f->ch_c = f->ch_d = 0;
        }

        for (i=0;i < adlib_fm_voices;i++) {
            struct adlib_fm_operator *f;

            f = &adlib_fm[i].mod;
            f->mod_multiple = 1;
            f->total_level = 63 - 16;
            f->attack_rate = 15;
            f->decay_rate = 0;
            f->sustain_level = 7;
            f->release_rate = 7;
            f->f_number = 0x2AE; // C
            f->octave = 4;
            f->key_on = 0;

            f = &adlib_fm[i].car;
            f->mod_multiple = 1;
            f->total_level = 63 - 16;
            f->attack_rate = 15;
            f->decay_rate = 0;
            f->sustain_level = 7;
            f->release_rate = 7;
            f->f_number = 0;
            f->octave = 0;
            f->key_on = 0;
        }

        adlib_apply_all();
    }

    /* as part of the gag, we use the VGA 8x8 BIOS font instead of the nice Arial font */
    vga_8x8_font_ptr = (unsigned char FAR*)_dos_getvect(0x43); /* Character table (8x8 chars 0x00-0x7F) [http://www.ctyme.com/intr/rb-6169.htm] */

    other_unhook_irq = my_unhook_irq;

    write_8254_system_timer(0);

    detect_keyboard();
    detect_sound_blaster();

#if TARGET_MSDOS == 16
# if 0 // not using it yet
    probe_emm();            // expanded memory support
    probe_himem_sys();      // extended memory support
# endif
#endif

    init_timer_irq();
    init_vga256unchained();

    woo_title();

    done = 0;
    while (!done) {
        sel = woo_menu();
        switch (sel) {
            case -1:
                done = 1;
                break;
            case 0:
                game_0();
                break;
            default:
                fatal("Unknown menu selection %u",sel);
                break;
        }
    }

    gen_res_free();
    check_heap();
    unhook_irqs();
    restore_text_mode();

    //debug
    dbg_heap_list();

    return 0;
}

