
#if defined(__WATCOMC__) && defined(__I86__) && TARGET_MSDOS == 16

/* Watcom C + MS-DOS/Windows 16-bit target for 8086 or higher */
static unsigned int resample_interpolate_asm86(unsigned int c,unsigned int p,unsigned int f);
/* This algorithm works ONLY if the values are unsigned.
 *
 * AX = c, BX = p, CX = f
 * AX -= BX
 * SI = 0 if AX >= 0, 0xFFFF if AX < 0
 * AX ^= SI  (convert to absolute... well almost... not quite NEG but close enough... off by 1 at most)
 * AX -= SI  (correct the negative value so the result is like NEG)
 * DX:AX = AX * CX
 * DX ^= SI
 * DX += BX */
#pragma aux resample_interpolate_asm86 = \
    "sub    ax,bx" \
    "sbb    si,si" \
    "xor    ax,si" \
    "sub    ax,si" \
    "mul    cx" \
    "xor    dx,si" \
    "add    dx,bx" \
    parm [ax] [bx] [cx] \
    value [dx] \
    modify [dx ax si]

static inline int resample_interpolate8(const unsigned int channel) {
    return (int)resample_interpolate_asm86((unsigned int)resample_state.c[channel],(unsigned int)resample_state.p[channel],(unsigned int)resample_state.frac);
}

static inline int resample_interpolate16(const unsigned int channel) {
    /* this only works IF we make the 16-bit PCM unsigned */
    return (int)(resample_interpolate_asm86((unsigned int)resample_state.c[channel] ^ 0x8000U,(unsigned int)resample_state.p[channel] ^ 0x8000U,(unsigned int)resample_state.frac) ^ 0x8000U);
}

#else
# pragma warning i86 resample include ignored, target not applicable for this header
#endif

