
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
#include "cvrdbuf.h"
#include "cvip.h"

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

