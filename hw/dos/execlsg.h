
#include <hw/dos/exeload.h>

#if !defined(TARGET_OS2) && !defined(TARGET_WINDOWS) && TARGET_MSDOS == 16

#define CLSG_EXPORT_PROC __cdecl far

int exeload_clsg_validate_header(const struct exeload_ctx * const exe);

static inline unsigned short exeload_clsg_function_count(const struct exeload_ctx * const exe) {
    return *((unsigned short far*)MK_FP(exe->base_seg,4));
}

static inline unsigned short exeload_clsg_function_offset(const struct exeload_ctx * const exe,const unsigned int i) {
    return *((unsigned short far*)MK_FP(exe->base_seg,4+2+(i*2U)));
}

/* NTS: You're supposed to typecast return value into function pointer prototype */
static inline void far *exeload_clsg_function_ptr(const struct exeload_ctx * const exe,const unsigned int i) {
    return (void far*)MK_FP(exe->base_seg,exeload_clsg_function_offset(exe,i));
}

#endif

