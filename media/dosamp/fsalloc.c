
#if defined(TARGET_WINDOWS)
# define HW_DOS_DONT_DEFINE_MMSYSTEM
# include <windows.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>

#include "dosamp.h"
#include "filesrc.h"

dosamp_file_source_t dosamp_FAR dosamp_file_source_alloc(const_dosamp_file_source_t const inst_template) {
    dosamp_file_source_t inst;

    /* the reason we have a common alloc() is to enable pooling of structs in the future */
#if TARGET_MSDOS == 16
    inst = _fmalloc(sizeof(*inst));
#else
    inst = malloc(sizeof(*inst));
#endif

    if (inst != NULL)
        *inst = *inst_template;

    return inst;
}

void dosamp_FAR dosamp_file_source_free(dosamp_file_source_t const inst) {
    /* the reason we have a common free() is to enable pooling of structs in the future */
    /* ASSUME: inst != NULL */
#if TARGET_MSDOS == 16
    _ffree(inst);
#else
    free(inst);
#endif
}

