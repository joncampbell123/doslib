
#if defined(TARGET_WINDOWS)
# include <windows.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "dosamp.h"
#include "cvip.h"

uint32_t convert_ip_mono2stereo_u8(uint32_t samples,void dosamp_FAR * const proc_buf,const uint32_t buf_max) {
    /* buffer range check */
    assert((samples * (uint32_t)2U) <= buf_max);

#if defined(__WATCOMC__) && defined(__I86__) && TARGET_MSDOS == 16
    /* DS:SI = proc_buf + samples - 1
     * ES:DI = proc_buf + samples + samples - 2
     * CX = samples
     *
     * ...
     *
     * b = (BYTE)DS:SI      SI--
     * w = b | (b << 8)
     * ES:DI = (WORD)w      DI -= 2
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
        add     si,cx
        add     di,cx
        add     di,cx
        dec     si
        sub     di,2
l1:     lodsb
        mov     ah,al
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
        uint8_t dosamp_FAR * buf = (uint8_t dosamp_FAR *)proc_buf + (samples * 2UL) - 1;
        uint8_t dosamp_FAR * sp = (uint8_t dosamp_FAR *)proc_buf + samples - 1;
        uint32_t i = samples;

        while (i-- != 0UL) {
            *buf-- = *sp;
            *buf-- = *sp;
            sp--;
        }
    }
#endif

    return (uint32_t)samples * (uint32_t)2U;
}

