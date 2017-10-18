
#if defined(__WATCOMC__) && defined(__386__) && TARGET_MSDOS == 32

/* Watcom C + MS-DOS/Windows 32-bit target for 386 or higher */
static unsigned int resample_interpolate_asm86(unsigned int c,unsigned int p,unsigned int f);
/* This algorithm works ONLY if the values are unsigned.
 *
 * EAX = c, EBX = p, ECX = f
 * EAX -= EBX
 * ESI = 0 if EAX >= 0, 0xFFFFFFFF if EAX < 0
 * EAX ^= ESI  (convert to absolute... well almost... not quite NEG but close enough... off by 1 at most)
 * EAX -= ESI  (this corrects the inversion to act like NEG)
 * EDX:EAX = EAX * ECX
 * EDX ^= ESI
 * EDX += EBX
 *
 * NTS: SBB is "subtract with borrow" which is equivalent to "SUB dest, src + Carry Flag".
 *      "SUB dest, src" will set CF if dest < src at the time of execution (because it carries and becomes negative)
 *      So we use SBB to subtract ESI by itself to that ESI either becomes zero (CF=0) or -1 (CF=1). On the x86,
 *      -1 is 0xFFFFFFFF (all bits set). We XOR the value by ESI as a cheap way to get the absolute value of the
 *      result we just computed. But that's not QUITE the absolute value, so we subtract EAX by ESI so the result
 *      is the same as the NEG instruction. In this way, we accomplish a conditional negate. */
#pragma aux resample_interpolate_asm86 = \
    "sub    eax,ebx" \
    "sbb    esi,esi" \
    "xor    eax,esi" \
    "sub    eax,esi" \
    "mul    ecx" \
    "xor    edx,esi" \
    "add    edx,ebx" \
    parm [eax] [ebx] [ecx] \
    value [edx] \
    modify [edx eax esi]

static inline int resample_interpolate8(const unsigned int channel) {
    return (int)resample_interpolate_asm86((unsigned int)resample_state.c[channel],(unsigned int)resample_state.p[channel],(unsigned int)resample_state.frac);
}

static inline int resample_interpolate16(const unsigned int channel) {
    /* this only works IF we make the 16-bit PCM unsigned */
    return (int)(resample_interpolate_asm86((unsigned int)resample_state.c[channel] ^ 0x80000000U,(unsigned int)resample_state.p[channel] ^ 0x80000000U,(unsigned int)resample_state.frac) ^ 0x80000000U);
}

#else
# pragma warning i386 resample include ignored, target not applicable for this header
#endif

