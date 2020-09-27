
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

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/emm.h>
#include <hw/dos/himemsys.h>
#include <hw/vga/vga.h>
#include <hw/vga/vrl.h>
#include <hw/8237/8237.h>
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>
#include <hw/sndsb/sndsb.h>
#include <fmt/minipng/minipng.h>

#include "timer.h"
#include "vmode.h"
#include "fonts.h"
#include "vrlimg.h"
#include "dbgheap.h"
#include "fontbmp.h"
#include "unicode.h"
#include "commtmp.h"
#include "sin2048.h"
#include "vrldraw.h"
#include "seqcomm.h"
#include "keyboard.h"
#include "dumbpack.h"
#include "fzlibdec.h"
#include "fataexit.h"
#include "sorcpack.h"
#include "rotozoom.h"
#include "seqcanvs.h"
#include "cutscene.h"
#include "ldwavsn.h"

#include <hw/8042/8042.h>

int load_wav_into_buffer(unsigned long *length,unsigned long *srate,unsigned int *channels,unsigned int *bits,unsigned char FAR *buffer,unsigned long max_length,const char *path) {
#define FND_FMT     (1u << 0u)
#define FND_DATA    (1u << 1u)
    unsigned found = 0;
    off_t scan,stop;
    uint32_t len;
    int fd;

    *length = 0;
    *srate = 0;
    *channels = 0;
    *bits = 0;

    /* NTS: The RIFF structure of WAV isn't all that hard to handle, and uncompressed PCM data within isn't
     *      difficult to handle at all. WAV was invented back when Microsoft made sensibly designed file
     *      formats instead of overengineered crap like Windows Media. */
    fd = open(path,O_RDONLY | O_BINARY);
    if (fd < 0) return 1;

    /* 'RIFF' <length> 'WAVE' , length is overall length of WAV file */
    if (lseek(fd,0,SEEK_SET) != 0) goto errout;
    if (read(fd,common_tmp_small,12) != 12) goto errout;
    if (memcmp(common_tmp_small+0,"RIFF",4) != 0 || memcmp(common_tmp_small+8,"WAVE",4) != 0) goto errout;
    stop = *((uint32_t*)(common_tmp_small+4)) + 8; /* NTS: DOS is little endian and so are WAV structures. <length> is length of data following RIFF <length> */

    /* scan chunks within. Look for 'fmt ' and 'data' */
    scan = 12;
    while (scan < stop) {
        if (lseek(fd,scan,SEEK_SET) != scan) goto errout;
        if (read(fd,common_tmp_small,8) != 8) goto errout;
        len = *((uint32_t*)(common_tmp_small+4)); /* length of data following <fourcc> <length> */

        if (!memcmp(common_tmp_small+0,"fmt ",4)) {
            /* fmt contains WAVEFORMATEX */
            found |= FND_FMT;

            if (len >= 0x10) {
                if (read(fd,common_tmp_small,0x10) != 0x10) goto errout;
                /* typedef struct {
                    WORD  wFormatTag;               // +0x0
                    WORD  nChannels;                // +0x2
                    DWORD nSamplesPerSec;           // +0x4
                    DWORD nAvgBytesPerSec;          // +0x8
                    WORD  nBlockAlign;              // +0xC
                    WORD  wBitsPerSample;           // +0xE
                    WORD  cbSize;                   // +0x10
                 } WAVEFORMATEX; */
                if (*((uint16_t*)(common_tmp_small+0x00)) != 1) goto errout; /* wFormatTag == WAVE_FORMAT_PCM only! */
                *channels = *((uint16_t*)(common_tmp_small+0x02));
                *srate = *((uint32_t*)(common_tmp_small+0x04));
                *bits = *((uint16_t*)(common_tmp_small+0x0E));
            }
        }
        else if (!memcmp(common_tmp_small+0,"data",4)) {
            found |= FND_DATA;
            *length = (unsigned long)len;
            if (len > max_length) len = max_length;

            // FIXME: In 16-bit real mode this code can only load up to 64KB - 1 using read()
            if (len > 0xFFF0ul)
                len = 0xFFF0ul;

            if (len != 0ul) {
                if (read(fd,buffer,len) != len) goto errout; // WARNING: Assumes large memory model
            }
        }

        scan += 8ul + len;
    }

    if (found != (FND_DATA|FND_FMT)) goto errout;

    return 0;
errout:
    close(fd);
    return 1;
#undef FND_DATA
#undef FND_FMT
}

