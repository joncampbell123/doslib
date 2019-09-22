
#include <i86.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/vga2/vga2.h>

int main(int argc,char **argv) {
    /* base classicifcation */
    probe_vga2();

#ifdef TARGET_PC98
#else
    /* this version requires CGA/MDA/PCjr/Tandy */
    if (((vga2_flags & (VGA2_FLAG_MDA|VGA2_FLAG_CGA)) == 0)/*neither MDA/CGA*/ ||
        (vga2_flags & VGA2_FLAG_CARD_MASK/*ignore non-card bits*/ & (~(VGA2_FLAG_MDA|VGA2_FLAG_CGA))) != 0/*something other than MDA/CGA*/) {
        printf("This program requires CGA/MDA/PCjr/Tandy\n");
        return 1;
    }
#endif

    return 0;
}

