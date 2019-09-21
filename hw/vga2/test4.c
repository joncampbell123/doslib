
#include <i86.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/vga2/vga2.h>

int main(int argc,char **argv) {
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
#endif

    return 0;
}

