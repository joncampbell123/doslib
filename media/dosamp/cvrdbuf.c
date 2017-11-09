
#if defined(TARGET_WINDOWS)
# include <windows.h>
#endif

#if TARGET_MSDOS == 16
# include <dos.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdint.h>
#include <assert.h>

#include "dosamp.h"
#include "cvrdbuf.h"

// if enabled, the buffer is 256 bytes larger (128 on either end) to help detect buffer overruns
#define BOUNDS_CHECK

#ifdef BOUNDS_CHECK
#define BOUNDS_EXTRA 256
#define BOUNDS_ADJUST 128
#else
#define BOUNDS_EXTRA 0
#define BOUNDS_ADJUST 0
#endif

void convert_rdbuf_free(void) {
    if (convert_rdbuf.buffer != NULL) {
#if TARGET_MSDOS == 32 || defined(LINUX)
        free(convert_rdbuf.buffer - BOUNDS_ADJUST);
#else
        _ffree(convert_rdbuf.buffer - BOUNDS_ADJUST);
#endif
        convert_rdbuf.buffer = NULL;
        convert_rdbuf.len = 0;
        convert_rdbuf.pos = 0;
    }
}

unsigned char dosamp_FAR * convert_rdbuf_get(uint32_t *sz) {
    if (convert_rdbuf.buffer == NULL) {
        if (convert_rdbuf.size == 0)
            convert_rdbuf.size = 4096;

#if TARGET_MSDOS == 32 || defined(LINUX)
        convert_rdbuf.buffer = malloc(convert_rdbuf.size + BOUNDS_EXTRA);
#else
        convert_rdbuf.buffer = _fmalloc(convert_rdbuf.size + BOUNDS_EXTRA);
#endif
        convert_rdbuf.len = 0;
        convert_rdbuf.pos = 0;

#ifdef BOUNDS_CHECK
        if (convert_rdbuf.buffer != NULL) {
            convert_rdbuf.buffer += BOUNDS_ADJUST;

            {
                unsigned char dosamp_FAR *p = convert_rdbuf.buffer - BOUNDS_ADJUST;
                register unsigned int i;

                for (i=0;i < BOUNDS_ADJUST;i++) p[i] = i;
            }

            {
                unsigned char dosamp_FAR *p = convert_rdbuf.buffer + convert_rdbuf.size;
                register unsigned int i;

                for (i=0;i < (BOUNDS_EXTRA - BOUNDS_ADJUST);i++) p[i] = i;
            }
        }
#endif
    }

    if (sz != NULL) *sz = convert_rdbuf.size;
    return convert_rdbuf.buffer;
}

void convert_rdbuf_check(void) {
#ifdef BOUNDS_CHECK
    if (convert_rdbuf.buffer != NULL) {
        {
            unsigned char dosamp_FAR *p = convert_rdbuf.buffer - BOUNDS_ADJUST;
            register unsigned int i;

            for (i=0;i < BOUNDS_ADJUST;i++) {
                if (p[i] != i) goto fail;
            }
        }

        {
            unsigned char dosamp_FAR *p = convert_rdbuf.buffer + convert_rdbuf.size;
            register unsigned int i;

            for (i=0;i < (BOUNDS_EXTRA - BOUNDS_ADJUST);i++) {
                if (p[i] != i) goto fail;
            }
        }
    }

    return;
fail:
# if !defined(USE_WINFCON)
    fprintf(stderr,"convert_rdbuf buffer overrun corruption detected.\n");
# endif
    abort();
#endif
}

void convert_rdbuf_clear(void) {
    convert_rdbuf.len = 0;
    convert_rdbuf.pos = 0;
}

