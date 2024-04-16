
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <malloc.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include "fataexit.h"

void check_heap(void) {
#if 0 // this is broken now?
    const int r = _heapchk();
    if (!(r == _HEAPOK || r == _HEAPEMPTY))
        fatal("C runtime reports corrupt heap");
#endif
}

void dbg_heap_list(void) {
#if 0 // this is broken now?
    struct _heapinfo h;
    unsigned int c=0;

    fprintf(stderr,"HEAP DUMP:\n");

    h._pentry = NULL;
    while (_heapwalk(&h) == _HEAPOK) {
        printf("%c@%Fp+%04x ",
            (h._useflag == _USEDENTRY ? 'u' : 'f'),
            h._pentry,
            h._size);

        if ((++c) >= 4) {
            printf("\n");
            c = 0;
        }
    }

    if (c != 0)
        printf("\n");
#endif
}

