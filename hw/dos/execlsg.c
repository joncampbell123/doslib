
#if !defined(TARGET_WINDOWS) && TARGET_MSDOS == 16

#include <dos.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <hw/dos/exeload.h>
#include <hw/dos/execlsg.h>

int exeload_clsg_validate_header(const struct exeload_ctx * const exe) {
    if (exe == NULL) return 0;

    {
        /* CLSG at the beginning of the resident image and a valid function call table */
        unsigned char far *p = MK_FP(exe->base_seg,0);
        unsigned short far *functbl = (unsigned short far*)(p+4UL+2UL);
        unsigned long exe_len = exeload_resident_length(exe);
        unsigned short numfunc;
        unsigned int i;

        if (*((unsigned long far*)(p+0)) != 0x47534C43UL) return 0; // seg:0 = CLSG

        numfunc = *((unsigned short far*)(p+4UL));
        if (((numfunc*2UL)+4UL+2UL) > exe_len) return 0;

        /* none of the functions can point outside the EXE resident image. */
        for (i=0;i < numfunc;i++) {
            unsigned short fo = functbl[i];
            if ((unsigned long)fo >= exe_len) return 0;
        }

        /* the last entry after function table must be 0xFFFF */
        if (functbl[numfunc] != 0xFFFFU) return 0;
    }

    return 1;
}

#endif
