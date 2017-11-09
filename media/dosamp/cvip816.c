
#if defined(TARGET_WINDOWS)
# include <windows.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "dosamp.h"
#include "cvip.h"

uint32_t convert_ip_8_to_16(const uint32_t total_samples,void dosamp_FAR * const proc_buf,const uint32_t buf_max) {
    const uint32_t ret = total_samples * (uint32_t)2; /* return: how many bytes of converted audio */

    /* buffer range check! */
    assert(ret <= buf_max);

    /* in-place 16-bit to 8-bit conversion (up to buf_max) from file_codec (8) to play_codec (16).
     * due to data expansion, this converts backwards in place. */

    /* NTS: Caller is expected to normalize the proc_buf pointer in 16-bit MS-DOS builds so that
     *      FP_OFF(proc_buf) + buf_max does not overflow while processing */

#if defined(__WATCOMC__) && defined(__I86__) && TARGET_MSDOS == 16
    /* DS:SI = proc_buf + total_samples - 1
     * ES:DI = proc_buf + total_samples + total_samples - 2
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
        mov     cx,word ptr total_samples
        dec     cx                  ; CX = total samples - 1
        add     si,cx               ; therefore SI = proc_buf + total_samples - 1
        add     di,cx               ; therefore DI = proc_buf + (total_samples - 1) + (total_samples - 1)
        add     di,cx               ; ...or....... = proc_buf + total_samples + total_samples - 2
        inc     cx                  ; restore CX
l1:     lodsb
        mov     ah,al
        xor     al,al
        xor     ah,0x80
        stosw
        loop    l1
        pop     es
        pop     ds
        cld
    }
#else
    {
        int16_t dosamp_FAR * buf = (int16_t dosamp_FAR *)proc_buf + total_samples - 1;
        uint8_t dosamp_FAR * sp = (uint8_t dosamp_FAR *)proc_buf + total_samples - 1;
        uint32_t i = total_samples;

        while (i-- != 0UL)
            *buf-- = (int16_t)(((uint16_t)((*sp--) ^ 0x80U)) << 8U);
    }
#endif

    return ret;
}

