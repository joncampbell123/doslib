
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/emm.h>
#include <hw/dos/himemsys.h>
#include <hw/vga/vga.h>
#include <hw/vga/vrl.h>
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>
#include <fmt/minipng/minipng.h>

#include "timer.h"
#include "vrlimg.h"
#include "unicode.h"
#include "commtmp.h"
#include "fzlibdec.h"

int file_zlib_decompress(int fd,unsigned char *buf,unsigned int sz,uint32_t srcsz) {
#define TMP_SZ 1024
    unsigned char *tmp;
    z_stream z;
    int r = -1;

    if (fd < 0) return -1;
    if ((tmp=malloc(TMP_SZ)) == NULL) return -1;

    memset(&z,0,sizeof(z));
    z.next_out = buf;
    z.avail_out = sz;
    z.next_in = tmp;
    z.avail_in = 0;
    if (inflateInit2(&z,15/*max window size 32KB*/) != Z_OK) goto fail;

    while (z.avail_out != 0) {
        if (z.avail_in == 0) {
            if (srcsz != 0) {
                const uint32_t rsz = (srcsz < TMP_SZ) ? srcsz : TMP_SZ;
                if (read(fd,tmp,rsz) != rsz) goto fail2;
                z.avail_in = rsz;
                z.next_in = tmp;
                srcsz -= rsz;
            }
            else {
                break;
            }
        }

        if (inflate(&z,Z_SYNC_FLUSH) != Z_OK) break;
    }

    if (z.avail_out == 0) r = 0;

    inflateEnd(&z);
    free(tmp);
    return r;
fail2:
    inflateEnd(&z);
fail:
    free(tmp);
    return -1;
#undef TMP_SZ
}

