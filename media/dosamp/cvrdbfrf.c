
#if defined(TARGET_WINDOWS)
# include <windows.h>
#endif

#if TARGET_MSDOS == 16
# include <dos.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdint.h>
#include <assert.h>

#include "dosamp.h"
#include "cvrdbuf.h"
#include "dosptrnm.h"
#include "resample.h"

uint32_t convert_rdbuf_resample_fast_to_8_mono(uint8_t dosamp_FAR *dst,uint32_t samples) {
#define resample_interpolate_func resample_interpolate8
#define sample_type_t uint8_t
#define sample_channels 1
#include "rsrdbtmf.h"
}

uint32_t convert_rdbuf_resample_fast_to_8_stereo(uint8_t dosamp_FAR *dst,uint32_t samples) {
#define resample_interpolate_func resample_interpolate8
#define sample_type_t uint8_t
#define sample_channels 2
#include "rsrdbtmf.h"
}

uint32_t convert_rdbuf_resample_fast_to_16_mono(int16_t dosamp_FAR *dst,uint32_t samples) {
#define resample_interpolate_func resample_interpolate16
#define sample_type_t int16_t
#define sample_channels 1
#include "rsrdbtmf.h"
}

uint32_t convert_rdbuf_resample_fast_to_16_stereo(int16_t dosamp_FAR *dst,uint32_t samples) {
#define resample_interpolate_func resample_interpolate16
#define sample_type_t int16_t
#define sample_channels 2
#include "rsrdbtmf.h"
}

