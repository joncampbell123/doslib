
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#include <ext/zlib/zlib.h>

#include "minipng.h"

int minipng_reader_rewind(struct minipng_reader *rdr) {
    if (rdr == NULL) return -1;
    if (rdr->fd < 0) return -1;

    rdr->chunk_data_offset = -1;
    rdr->next_chunk_start = 8;

    return 0;
}

