
#include <i86.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/vga2/vga2.h>

#ifndef TARGET_PC98
static void do_test(const unsigned int w) {
    vga2_set_alpha_width_cga(w/*width*/,w/*stride*/);

    /* clear screen */
    {
        VGA2_ALPHA_PTR p = vga2_alpha_ptr();
        unsigned int c = (unsigned int)vga2_alpha_base.width * (unsigned int)vga2_alpha_base.height;
        while (c > 0) {
            *p++ = VGA2_ALPHA_ATTR_FGBGCOLOR(7,0) | 0x20;
            c--;
        }
    }
    vga2_set_int10_cursor_pos(2/*row*/,0);
    printf("Hit ENTER. %ux%u. want=%u\n",vga2_alpha_base.width,vga2_alpha_base.height,w);
    printf("Hello 1\n");
    printf("Hello 2\n");
    printf("Hello 3\n");
    while (getch() != 13);
}
#endif

int main(int argc,char **argv) {
    /* base classicifcation */
    probe_vga2();

#ifndef TARGET_PC98
    /* this version requires CGA/MDA/PCjr/Tandy */
    if (((vga2_flags & (VGA2_FLAG_MDA|VGA2_FLAG_CGA)) == 0)/*neither MDA/CGA*/ ||
        (vga2_flags & VGA2_FLAG_CARD_MASK/*ignore non-card bits*/ & (~(VGA2_FLAG_MDA|VGA2_FLAG_CGA))) != 0/*something other than MDA/CGA*/) {
        printf("This program requires CGA/MDA/PCjr/Tandy\n");
        return 1;
    }

    /* we're interested in text mode, probe it */
    vga2_update_alpha_modeinfo();
    if (!vga2_alpha_ptr_valid()) {
        printf("I need to be able to locate the screen\n");
        return 1;
    }

    do_test(80);
    do_test(90);
    do_test(40);
    do_test(50);
    do_test(60);
    do_test(70);
#endif

    return 0;
}

