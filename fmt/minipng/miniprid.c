
#include <stdio.h>
#if defined(TARGET_MSDOS)
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#if defined(TARGET_MSDOS)
#include <dos.h>
#endif

#if defined(TARGET_MSDOS)
#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#endif

#if defined(TARGET_MSDOS)
#include <ext/zlib/zlib.h>
#else
#include <zlib.h>
#endif

#include <fmt/minipng/minipng.h>

void minipng_reader_reset_idat(struct minipng_reader *rdr) {
    if (rdr == NULL) return;
    if (rdr->fd < 0) return;

    if (rdr->compr != NULL) {
        free(rdr->compr);
        rdr->compr = NULL;
    }

    if (rdr->compr_zlib.next_in != NULL) {
        inflateEnd(&(rdr->compr_zlib));
        memset(&(rdr->compr_zlib),0,sizeof(rdr->compr_zlib));
    }
}

