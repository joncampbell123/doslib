
#include <i86.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/vga2/vga2.h>

int main(int argc,char **argv) {
    (void)argc;
    (void)argv;

    /* base classicifcation */
    probe_vga2();

    /* If I don't call the function to obtain the alphanumeric pointer,
     * then that code should NOT be linked into this executable.
     *
     * That can be verified by compiling with Open Watcom C and then
     * reading the *.map file for this executable to make sure only
     * the probe and flags symbols were added. The alpha functions
     * should NOT be linked in. */

    printf("VGA2 flags: 0x%x\n",vga2_flags);
#ifdef TARGET_PC98
    /*nothing*/
#else
    if (vga2_flags & VGA2_FLAG_MDA)
        printf("  - MDA\n");
    if (vga2_flags & VGA2_FLAG_CGA)
        printf("  - CGA\n");
    if (vga2_flags & VGA2_FLAG_EGA)
        printf("  - EGA\n");
    if (vga2_flags & VGA2_FLAG_VGA)
        printf("  - VGA\n");
    if (vga2_flags & VGA2_FLAG_MCGA)
        printf("  - MCGA\n");
    if (vga2_flags & VGA2_FLAG_PGA)
        printf("  - PGA\n");
    if (vga2_flags & VGA2_FLAG_MONO_DISPLAY)
        printf("  - MONO DISPLAY\n");
    if (vga2_flags & VGA2_FLAG_DIGITAL_DISPLAY)
        printf("  - DIGITAL DISPLAY\n");
#endif

    return 0;
}

