
#include <i86.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/vga2/vga2.h>

static const char msg[] = "Hello world";

int main(int argc,char **argv) {
    unsigned char x,y;

    (void)argc;
    (void)argv;

    /* base classicifcation */
    probe_vga2();

    printf("VGA2 flags: 0x%x\n",vga2_flags);

    /* we're interested in text mode, probe it */
    vga2_update_alpha_modeinfo();

#if TARGET_MSDOS == 32
    printf("Alpha ptr: %p\n",vga2_alpha_ptr());
#else
    printf("Alpha ptr: %Fp\n",vga2_alpha_ptr());
#endif
    printf("Alpha size: %lu bytes\n",vga2_alpha_ptrsz());
    printf("Alpha dim: %u x %u\n",vga2_alpha_base.width,vga2_alpha_base.height);
    printf("Alpha ptr valid: %u\n",vga2_alpha_ptr_valid());

    if (!vga2_alpha_ptr_valid()) {
        printf("I need to be able to locate the screen\n");
        return 1;
    }

    /*----------------*/

    for (y=0;y < vga2_alpha_base.height;y++) {
        VGA2_ALPHA_PTR rowp = vga2_alpharow_ptr(y);

#ifdef TARGET_PC98
        const unsigned int fg = 7 - (y % 6);
        const unsigned short attr =
            VGA2_ALPHA_ATTR_NOTSECRET | VGA2_ALPHA_ATTR_COLOR(fg) |
            ((((y / 6) % 2) == 1) ? VGA2_ALPHA_ATTR_REVERSE : 0);
        VGA2_ALPHA_PTR rowpa = rowp + VGA2_PC98_ATTR_OFFSC;

        for (x=0;msg[x] != 0;x++) {
            rowp[x] = msg[x];
            rowpa[x] = attr;
        }
#else
        const unsigned int fg = 15 - (y % 15);
        const unsigned int bg = 1 + ((y / 15) % 15);

        for (x=0;msg[x] != 0;x++) rowp[x] = msg[x] | VGA2_ALPHA_ATTR_FGBGCOLOR(fg,bg);
#endif
    }

    while (getch() != 13);

    /*----------------*/

    for (y=0;y < vga2_alpha_base.height;y++) {
        VGA2_ALPHA_PTR rowp = vga2_alpharowcol_ptr(y,20);

#ifdef TARGET_PC98
        const unsigned int fg = 7 - (y % 6);
        const unsigned short attr =
            VGA2_ALPHA_ATTR_NOTSECRET | VGA2_ALPHA_ATTR_COLOR(fg) |
            ((((y / 6) % 2) == 1) ? VGA2_ALPHA_ATTR_REVERSE : 0);
        VGA2_ALPHA_PTR rowpa = rowp + VGA2_PC98_ATTR_OFFSC;

        for (x=0;msg[x] != 0;x++) {
            rowp[x] = msg[x];
            rowpa[x] = attr;
        }
#else
        const unsigned int fg = 15 - (y % 15);
        const unsigned int bg = 1 + ((y / 15) % 15);

        for (x=0;msg[x] != 0;x++) rowp[x] = msg[x] | VGA2_ALPHA_ATTR_FGBGCOLOR(fg,bg);
#endif
    }

    while (getch() != 13);

    return 0;
}

