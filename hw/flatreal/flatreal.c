
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/emm.h>
#include <hw/dos/doswin.h>
#include <hw/flatreal/flatreal.h>

#if TARGET_MSDOS == 16

#ifndef MMODE
#error I need to know what memory model
#endif

void __cdecl flatrealmode_force_datasel(void *ptr);

int flatrealmode_setup(uint32_t limit) {
    const unsigned long segds = 0;
    uint32_t *ap;
    void *buf;

    /* Requires a 386 or higher. As usual, if the programmer doesn't bother to check the CPU
     * we go ahead and do it anyway */
    if (cpu_basic_level < 3)
        return 0;

    if (!flatrealmode_allowed())
        return 0;

    if (limit < 0xE000UL) limit = 0xE000UL; /* WARNING: Do not allow limit much below 64KB or real-mode code will start crashing */
    limit >>= 12UL;

    buf = malloc(16 + (8*3));
    if (buf == NULL) return 0;

#if defined(__LARGE__) || defined(__COMPACT__) /* 16-bit large memory model */
    {
        unsigned int seg = FP_SEG(buf);
        unsigned int ofs = FP_OFF(buf);
        ofs = (ofs + 15) & (~15);
        seg += ofs >> 4;
        ofs &= 0xF;
        ap = MK_FP(seg,ofs);
    }
#else
    ap = (uint32_t*)(((size_t)buf + 15) & (~15));
#endif

    /* GDT[0] = null */
    ap[0] = ap[1] = 0;
    /* GDT[1] = 32-bit data segment */
    ap[2] = (segds << 16UL) | (limit & 0xFFFFUL);
    ap[3] = (limit & 0xF0000UL) | (0x80UL << 16UL) | (0x93UL << 8UL) | (segds >> 16UL);
    /* GDT[2] = 16-bit code segment */
    {
        void far *ptr = (void far*)flatrealmode_force_datasel;
        unsigned int seg = FP_SEG(ptr);
        unsigned long sego = (unsigned long)seg << 4UL;
/*      printf("%04x:%04x %05lx\n",seg,FP_OFF(ptr),sego); */
        ap[4] = (sego << 16UL) | 0xFFFFUL;
        ap[5] = 0xF0000UL | (0x80UL << 16UL) | (0x9BUL << 8UL) | (sego >> 16UL);
    }

    flatrealmode_force_datasel(ap);

    free(buf);
    return 1;
}
#endif

