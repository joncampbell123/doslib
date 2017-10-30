
#include <stdio.h>
#include <stdint.h>
#ifdef LINUX
#include <endian.h>
#else
#include <hw/cpu/endian.h>
#endif
#ifndef LINUX
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <direct.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <malloc.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#ifndef LINUX
#include <dos.h>
#endif

#ifndef LINUX
#include <hw/dos/dos.h>
#include <hw/cpu/cpu.h>
#include <hw/8237/8237.h>       /* 8237 DMA */
#include <hw/8254/8254.h>       /* 8254 timer */
#include <hw/8259/8259.h>       /* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/cpu/cpurdtsc.h>
#include <hw/dos/doswin.h>
#include <hw/dos/tgusmega.h>
#include <hw/dos/tgussbos.h>
#include <hw/dos/tgusumid.h>
#include <hw/isapnp/isapnp.h>
#include <hw/sndsb/sndsbpnp.h>
#endif

#include "wavefmt.h"
#include "dosamp.h"
#include "timesrc.h"
#include "dosptrnm.h"
#include "filesrc.h"
#include "resample.h"
#include "cvip.h"

uint32_t convert_ip_16_to_8(const uint32_t total_samples,void dosamp_FAR * const proc_buf,const uint32_t buf_max) {
    /* in-place 16-bit to 8-bit conversion (up to buf_max)
     * from file_codec (16) to play_codec (8). this must happen AFTER
     * channel conversion, therefore use play_codec.number_of_channels */
    uint8_t dosamp_FAR * buf = (uint8_t dosamp_FAR *)proc_buf;

    /* buffer range check (source is 16-bit) */
    assert((total_samples*(uint32_t)2) <= buf_max);

    /* NTS: Caller is expected to normalize the proc_buf pointer in 16-bit MS-DOS builds so that
     *      FP_OFF(proc_buf) + buf_max does not overflow while processing */

#if defined(__WATCOMC__) && defined(__I86__) && TARGET_MSDOS == 16
    __asm {
        push    ds
        push    es
        cld
        lds     si,buf
        mov     di,si
        push    ds
        pop     es
        mov     cx,word ptr total_samples
l1:     lodsw               ; AX = 16-bit sample
        mov     al,ah       ; AL = (AX >> 8) ^ 0x80
        xor     al,0x80
        stosb
        loop    l1
        pop     es
        pop     ds
    }
#else
    {
        int16_t dosamp_FAR * sp = (int16_t dosamp_FAR *)proc_buf;
        uint32_t i = total_samples;

        while (i-- != 0UL)
            *buf++ = (uint8_t)((((uint16_t)(*sp++)) ^ 0x8000) >> 8U);
    }
#endif

    return total_samples;
}

