
#include <i86.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/vga2/vga2.h>

int main(int argc,char **argv) {
    (void)argc;
    (void)argv;

    /* base classicifcation */
    probe_vga2();

#if !defined(TARGET_PC98)
    vga2_set_int10_mode(1);
    printf("Hello\n");
    getch();

    vga2_set_int10_mode(3);
    printf("Hello\n");
    getch();

    vga2_set_int10_cursor_shape(0,7);
    getch();

    vga2_set_int10_cursor_shape(VGA2_INT10_CURSOR_SHAPE_INVISIBLE,7);
    getch();

    vga2_set_int10_cursor_shape(6,7); /* standard shape */
    getch();

    {
        unsigned int i;
        for (i=0;i < 32;i++) {
            vga2_set_int10_cursor_pos(7/*row*/,i);
            getch();
        }
    }

    /* controls BIOS cursor position and therefore the next location written by printf() */
    vga2_set_int10_cursor_pos(7/*row*/,0);
    printf("Hello\n");
    getch();
#endif

    return 0;
}

