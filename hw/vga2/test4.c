
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
#endif

    return 0;
}

