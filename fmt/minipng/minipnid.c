
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

/* WARNING: Normalize far pointer dst before calling this */
int minipng_reader_read_idat(struct minipng_reader *rdr,unsigned char FAR *dst,size_t count) {
    int err;

    if (rdr == NULL) return -1;
    if (rdr->fd < 0) return -1;

    if (rdr->compr == NULL) {
        rdr->compr_size = 1024;
        rdr->compr = malloc(rdr->compr_size);
        if (rdr->compr == NULL) return -1;

        memset(&(rdr->compr_zlib),0,sizeof(rdr->compr_zlib));
        rdr->compr_zlib.next_in = rdr->compr;
        rdr->compr_zlib.avail_in = 0;
        rdr->compr_zlib.next_out = dst;
        rdr->compr_zlib.avail_out = 0;
        rdr->idat_rem = 0;

        if (inflateInit2(&(rdr->compr_zlib),15/*max window size 32KB*/) != Z_OK) {
            memset(&(rdr->compr_zlib),0,sizeof(rdr->compr_zlib));
            free(rdr->compr);
            rdr->compr = NULL;
            return -1;
        }
    }

    rdr->compr_zlib.next_out = dst;
    rdr->compr_zlib.avail_out = count;
    while (rdr->compr_zlib.avail_out != 0) {
        if (rdr->compr_zlib.avail_in == 0) {
            if (rdr->idat_rem == 0) {
                if (minipng_reader_next_chunk(rdr)) break;
                if (rdr->chunk_data_header.type == minipng_chunk_fourcc('I','D','A','T')) {
                    rdr->idat_rem = rdr->chunk_data_header.length;
                    if (rdr->idat_rem == 0) continue;
                }
                else {
                    rdr->ungetch++;
                    break;
                }
            }

            /* assume idat_rem != 0 */
            {
                size_t icount = (rdr->idat_rem < rdr->compr_size) ? rdr->idat_rem : rdr->compr_size; /* lesser of the two */

                read(rdr->fd,rdr->compr,icount);
                rdr->idat_rem -= icount;
                rdr->compr_zlib.next_in = rdr->compr;
                rdr->compr_zlib.avail_in = icount;
            }
        }

        if ((err=inflate(&(rdr->compr_zlib),Z_NO_FLUSH)) != Z_OK) {
            break;
        }
        if (rdr->compr_zlib.avail_out == count) {
            if ((err=inflate(&(rdr->compr_zlib),Z_SYNC_FLUSH)) != Z_OK) {
                break;
            }
        }
    }

    return (count - rdr->compr_zlib.avail_out);
}

