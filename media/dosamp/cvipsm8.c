
#include <stdio.h>
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
#include <dos.h>

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

#include "wavefmt.h"
#include "dosamp.h"
#include "timesrc.h"
#include "dosptrnm.h"
#include "filesrc.h"
#include "resample.h"
#include "cvip.h"

uint32_t convert_ip_stereo2mono_u8(uint32_t samples,void dosamp_FAR * const proc_buf,const uint32_t buf_max) {
    /* in-place stereo to mono conversion (up to convert_rdbuf_len)
     * from file_codec channels (2) to play_codec channels (1) */
    uint8_t dosamp_FAR * buf = (uint8_t dosamp_FAR *)proc_buf;

    /* buffer range check (stereo to mono) */
    assert((samples*(uint32_t)2U) <= buf_max);

#if defined(__WATCOMC__) && defined(__I86__) && TARGET_MSDOS == 16
    __asm {
        push    ds
        push    es
        cld
        lds     si,buf
        mov     di,si
        push    ds
        pop     es
        mov     cx,word ptr samples
l1:     lodsw               ; AX = two samples
        mov     bh,ah       ; BH = second sample, AL = first sample
        xor     ah,ah       ; AH = 0
        add     al,bh       ; first sample + second sample
        adc     ah,0        ; (carry)
        shr     ax,1        ; AX >>= 1
        stosb               ; store one sample
        loop    l1
        pop     es
        pop     ds
    }
#else
    {
        uint8_t dosamp_FAR * sp = buf;
        uint32_t i = samples;

        while (i-- != 0UL) {
            *buf++ = (uint8_t)(((unsigned int)sp[0] + (unsigned int)sp[1] + 1U) >> 1U);
            sp += 2;
        }
    }
#endif

    return samples;
}

