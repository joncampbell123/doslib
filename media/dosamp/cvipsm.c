
#if defined(TARGET_WINDOWS)
# include <windows.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "dosamp.h"
#include "cvip.h"

uint32_t convert_ip_stereo2mono(uint32_t samples,void dosamp_FAR * const proc_buf,const uint32_t buf_max,const uint8_t bits_per_sample) {
    /* in-place stereo to mono conversion (up to convert_rdbuf_len)
     * from file_codec channels (2) to play_codec channels (1) */
    if (bits_per_sample <= 8)
        return convert_ip_stereo2mono_u8(samples,proc_buf,buf_max);
    else if (bits_per_sample >= 16)
        return convert_ip_stereo2mono_s16(samples,proc_buf,buf_max);

    return 0;
}

