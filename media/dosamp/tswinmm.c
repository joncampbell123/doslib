
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

#include <windows.h>

#include "dosamp.h"
#include "timesrc.h"

#if defined(TARGET_WINDOWS)

int init_mmsystem(void);

/* previous count, and period (in case host changes timer) */
static uint32_t ts_mmsystem_time_pper = 0;

DWORD (WINAPI *__timeGetTime)(void) = NULL;

int dosamp_FAR dosamp_time_source_mmsystem_time_close(dosamp_time_source_t inst) {
    __timeGetTime = NULL;
    inst->open_flags = 0;
    inst->counter = 0;
    return 0;
}

int dosamp_FAR dosamp_time_source_mmsystem_time_open(dosamp_time_source_t inst) {
    if (inst->open_flags != 0)
        return 0;

    if (!init_mmsystem())
        return -1;

    __timeGetTime = (DWORD (WINAPI *)(void))GetProcAddress(mmsystem_dll,"TIMEGETTIME");
    if (__timeGetTime == NULL)
        return -1;

    inst->open_flags = (unsigned int)(-1);
    inst->clock_rate = 1000;
    ts_mmsystem_time_pper = __timeGetTime();
    return 0;
}

unsigned long long dosamp_FAR dosamp_time_source_mmsystem_time_poll(dosamp_time_source_t inst) {
    if (inst->open_flags == 0) return 0UL;

    {
        uint32_t ncnt = __timeGetTime();

        /* count */
        inst->counter += (unsigned long long)(ncnt - ts_mmsystem_time_pper);

        ts_mmsystem_time_pper = ncnt;
    }

    return inst->counter;
}

struct dosamp_time_source               dosamp_time_source_mmsystem_time = {
    .obj_id =                           dosamp_time_source_id_mmsystem_time,
    .open_flags =                       0,
    .clock_rate =                       0,
    .counter =                          0,
    .close =                            dosamp_time_source_mmsystem_time_close,
    .open =                             dosamp_time_source_mmsystem_time_open,
    .poll =                             dosamp_time_source_mmsystem_time_poll
};

#endif /* defined(TARGET_WINDOWS) */

