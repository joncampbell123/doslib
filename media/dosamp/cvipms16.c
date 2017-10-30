
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

uint32_t convert_ip_mono2stereo_s16(uint32_t samples,void dosamp_FAR * const proc_buf,const uint32_t buf_max) {
    /* buffer range check */
    assert((samples * (uint32_t)4U) <= buf_max);

#if defined(__WATCOMC__) && defined(__I86__) && TARGET_MSDOS == 16
    /* DS:SI = proc_buf + samples + samples - 1
     * ES:DI = proc_buf + samples + samples + samples + samples - 1
     * CX = samples
     */
    __asm {
        push    ds
        push    es
        std                         ; scan backwards
        lds     si,proc_buf
        mov     di,si
        push    ds
        pop     es
        mov     cx,word ptr samples
        push    cx
        add     cx,cx               ; CX *= 2
        add     si,cx               ; SI += samples * 2 (CX)
        add     di,cx               ; DI += samples * 4 (CX * 2)
        add     di,cx
        pop     cx
        sub     si,2
        sub     di,2
l1:     lodsw
        stosw
        stosw
        loop    l1
        pop     es
        pop     ds
        cld
    }
#else
    {
        /* in-place mono to stereo conversion (up to proc_buf_len)
         * from file_codec channels (1) to play_codec channels (2).
         * due to data expansion we process backwards. */
        int16_t dosamp_FAR * buf = (int16_t dosamp_FAR *)proc_buf + (samples * 2UL) - 1;
        int16_t dosamp_FAR * sp = (int16_t dosamp_FAR *)proc_buf + samples - 1;
        uint32_t i = samples;

        while (i-- != 0UL) {
            *buf-- = *sp;
            *buf-- = *sp;
            sp--;
        }
    }
#endif

    return (uint32_t)samples * (uint32_t)4U;
}

