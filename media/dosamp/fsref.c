
#if defined(TARGET_WINDOWS)
# define HW_DOS_DONT_DEFINE_MMSYSTEM
# include <windows.h>
#endif

#include <stdio.h>
#include <stdint.h>

#include "dosamp.h"
#include "filesrc.h"

unsigned int dosamp_FAR dosamp_file_source_addref(dosamp_file_source_t const inst) {
    return ++(inst->refcount);
}

unsigned int dosamp_FAR dosamp_file_source_release(dosamp_file_source_t const inst) {
    if (inst->refcount != 0)
        inst->refcount--;
    // TODO: else, debug message if refcount == 0

    return inst->refcount;
}

unsigned int dosamp_FAR dosamp_file_source_autofree(dosamp_file_source_ptr_t inst) {
    unsigned int r = 0;

    /* assume inst != NULL */
    if (*inst != NULL) {
        r = (*inst)->refcount;
        if (r == 0) {
            (*inst)->close(*inst);
            (*inst)->free(*inst);
            *inst = NULL;
        }
    }

    return r;
}

