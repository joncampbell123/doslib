
/* USEFUL */
#if defined(__GNUC__)
# define VXD_INT3()                 __asm__ __volatile__ ("int $3")
#else
# define VXD_INT3()                 __asm int 3
#endif

#define VXD_STRINGIFY2(x)   #x
#define VXD_STRINGIFY(x)    VXD_STRINGIFY2(x)

#define VXD_AsmCall(dev,srv) \
        "int    $0x20\n"                                                        \
        ".word  " VXD_STRINGIFY(srv) "\n"                                       \
        ".word  " VXD_STRINGIFY(dev) "\n"

static inline void VXD_CF_SUCCESS(void) {
    __asm__ __volatile__ ("clc");
}

static inline void VXD_CF_FAILURE(void) {
    __asm__ __volatile__ ("stc");
}

#define VXD_CONTROL_DISPATCH(ctrlmsg, ctrlfunc) \
    __asm__ __volatile__ (                      \
        "cmp    %0,%%eax\n"                     \
        "jz     " #ctrlfunc "\n"                \
        : /*outputs*/                           \
        : /*inputs*/  /*%0*/ "i" (ctrlmsg)      \
        : /*clobbers*/       "cc")

inline static unsigned int VXD_GETEBX(void) {
    register unsigned int r asm("ebx"); /* FIXME: GCC-specific */
    return r;
}

#if (__GNUC__ >= 6 && __GNUC_MINOR__ >= 1) || (__GNUC__ >= 7)
# define GCC_INLINE_ASM_SUPPORTS_cc_OUTPUT
#endif

