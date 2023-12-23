
#include <stdio.h>
#include <stdint.h>
#include <windows.h>
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

#include "wavefmt.h"
#include "dosamp.h"
#include "timesrc.h"

#if defined(HAS_QPC)

static LARGE_INTEGER ts_qpc_prev;

BOOL (WINAPI * __QueryPerformanceFrequency)(LARGE_INTEGER *) = NULL;
BOOL (WINAPI * __QueryPerformanceCounter)(LARGE_INTEGER *) = NULL;

int dosamp_FAR dosamp_time_source_qpc_close(dosamp_time_source_t inst) {
    inst->open_flags = 0;
    ts_qpc_prev.QuadPart = 0;
    return 0;
}

int dosamp_FAR dosamp_time_source_qpc_open(dosamp_time_source_t inst) {
    LARGE_INTEGER t;

    if (__QueryPerformanceFrequency(&t) == 0) return -1;
    inst->clock_rate = (uint64_t)t.QuadPart;

    if (__QueryPerformanceCounter(&t) == 0) return -1;
    ts_qpc_prev.QuadPart = (uint64_t)t.QuadPart;

    inst->open_flags = (unsigned int)(-1);
    return 0;
}

unsigned long long dosamp_FAR dosamp_time_source_qpc_poll(dosamp_time_source_t inst) {
    LARGE_INTEGER t;

    if (inst->open_flags == 0) return 0UL;

    if (__QueryPerformanceCounter(&t) == 0) return -1;

    inst->counter += t.QuadPart - ts_qpc_prev.QuadPart;

    ts_qpc_prev = t;

    return inst->counter;
}

struct dosamp_time_source               dosamp_time_source_qpc = {
    .obj_id =                           dosamp_time_source_id_qpc,
    .open_flags =                       0,
    .clock_rate =                       0,
    .counter =                          0,
    .poll_requirement =                 0, /* counter is 64-bit */
    .close =                            dosamp_time_source_qpc_close,
    .open =                             dosamp_time_source_qpc_open,
    .poll =                             dosamp_time_source_qpc_poll
};

int dosamp_time_source_qpc_available(const dosamp_time_source_t clk) {
    HMODULE kernel32 = GetModuleHandle("KERNEL32.DLL");

    (void)clk;

    if (kernel32 == NULL) return 0;

    __QueryPerformanceFrequency = (BOOL (WINAPI *)(LARGE_INTEGER *))GetProcAddress(kernel32,"QueryPerformanceFrequency");
    __QueryPerformanceCounter = (BOOL (WINAPI *)(LARGE_INTEGER *))GetProcAddress(kernel32,"QueryPerformanceCounter");

    if (__QueryPerformanceFrequency == NULL || __QueryPerformanceCounter == NULL)
       return 0;

    return 1;
}


#endif /* defined(HAS_RDTSC) */

