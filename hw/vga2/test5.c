
#include <i86.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/vga2/vga2.h>

void cls(void) {
    VGA2_ALPHA_PTR p = vga2_alpha_ptr();
    unsigned int c = vga2_alpha_base.width * vga2_alpha_base.height;

    while (c > 0) {
#if defined(TARGET_PC98)
#else
        *p++ = VGA2_ALPHA_ATTR_FGBGCOLOR(7,0) | 0x20;
#endif
        c--;
    }
}

int main(int argc,char **argv) {
    /* base classicifcation */
    probe_vga2();
    vga2_update_alpha_modeinfo();
    if (!vga2_alpha_ptr_valid()) {
        printf("Unable to determine text buffer\n");
        return 1;
    }

#if defined(TARGET_PC98)
#else
    cls();
    vga2_set_int10_cursor_pos(2,0);
    printf("Hello\n");
    printf("Hello 2\n");
    printf("Hello 3\n");
    while (getch() != 13);
#endif

    return 0;
}

