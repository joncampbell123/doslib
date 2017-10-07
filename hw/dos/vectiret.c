
#include <hw/dos/dos.h>

#if !defined(TARGET_WINDOWS) && !defined(WIN386) && !defined(TARGET_OS2)
int vector_is_iret(const unsigned char vector) {
    const unsigned char far *p;
    uint32_t rvector;

#if TARGET_MSDOS == 32
    rvector = ((uint32_t*)0)[vector];
    if (rvector == 0) return 0;
    p = (const unsigned char*)(((rvector >> 16UL) << 4UL) + (rvector & 0xFFFFUL));
#else
    rvector = *((uint32_t far*)MK_FP(0,(vector*4)));
    if (rvector == 0) return 0;
    p = (const unsigned char far*)MK_FP(rvector>>16UL,rvector&0xFFFFUL);
#endif

    if (*p == 0xCF) {
        // IRET. Yep.
        return 1;
    }
    else if (p[0] == 0xFE && p[1] == 0x38) {
        // DOSBox callback. Probably not going to ACK the interrupt.
        return 1;
    }

    return 0;
}
#endif

