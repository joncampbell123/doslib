
#if defined(TARGET_WINDOWS)
# include <windows.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "dosamp.h"
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

