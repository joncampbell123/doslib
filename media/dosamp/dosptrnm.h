
#if TARGET_MSDOS == 32
static inline unsigned char *dosamp_ptr_add_normalize(unsigned char * const p,const unsigned int o) {
    return p + o;
}

static inline const unsigned char *dosamp_cptr_add_normalize(const unsigned char * const p,const unsigned int o) {
    return p + o;
}
#else
static inline uint32_t dosamp_ptr_to_linear(const unsigned char far * const p) {
    return ((unsigned long)FP_SEG(p) << 4UL) + (unsigned long)FP_OFF(p);
}

static inline unsigned char far *dosamp_linear_to_ptr(const uint32_t p) {
    const unsigned short s = (unsigned short)((unsigned long)p >> 4UL);
    const unsigned short o = (unsigned short)((unsigned long)p & 0xFUL);

    return (unsigned char far*)MK_FP(s,o);
}

static inline unsigned char far *dosamp_ptr_add_normalize(unsigned char far * const p,const unsigned int o) {
    return (unsigned char far*)dosamp_linear_to_ptr(dosamp_ptr_to_linear(p) + o);
}

static inline const unsigned char far *dosamp_cptr_add_normalize(const unsigned char far * const p,const unsigned int o) {
    return (const unsigned char far*)dosamp_linear_to_ptr(dosamp_ptr_to_linear(p) + o);
}
#endif

