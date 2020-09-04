
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

#include <fmt/minipng/minipng.h>

/* assume caller just opened or called rewind.
 * don't expect this to work if you call this twice in a row or after the IDAT. */
int minipng_reader_parse_head(struct minipng_reader *rdr) {
    while (minipng_reader_next_chunk(rdr) == 0) {
        if (rdr->chunk_data_header.type == minipng_chunk_fourcc('I','H','D','R')) {
            if (rdr->chunk_data_header.length >= 0x0D) {
                read(rdr->fd,&rdr->ihdr,0x0D);
                /* byte swap fields */
                rdr->ihdr.width =       minipng_byteswap32(rdr->ihdr.width);
                rdr->ihdr.height =      minipng_byteswap32(rdr->ihdr.height);
            }
        }
        else if (rdr->chunk_data_header.type == minipng_chunk_fourcc('P','L','T','E')) {
            if (rdr->plte == NULL && rdr->chunk_data_header.length >= 3ul) {
                rdr->plte_count = rdr->chunk_data_header.length / 3ul;
                if (rdr->plte_count <= 256) {
                    rdr->plte = malloc(3 * rdr->plte_count);
                    if (rdr->plte != NULL) read(rdr->fd,rdr->plte,3 * rdr->plte_count);
                }
            }
        }
        else if (rdr->chunk_data_header.type == minipng_chunk_fourcc('t','R','N','S')) {
            if (rdr->trns == NULL && rdr->chunk_data_header.length != 0) {
                rdr->trns_size = rdr->chunk_data_header.length;
                if (rdr->chunk_data_header.length <= 1024) {
                    rdr->trns = malloc(rdr->trns_size);
                    if (rdr->trns != NULL) read(rdr->fd,rdr->trns,rdr->trns_size);
                }
            }
        }
        else if (rdr->chunk_data_header.type == minipng_chunk_fourcc('I','D','A','T')) {
            /* it's the body of the PNG. stop now */
            rdr->ungetch++;
            break;
        }
    }

    /* IHDR required */
    if (rdr->ihdr.bit_depth == 0 || rdr->ihdr.width == 0 || rdr->ihdr.height == 0)
        return -1;

    return 0;
}

