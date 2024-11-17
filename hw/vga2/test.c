
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

    return 0;
}

