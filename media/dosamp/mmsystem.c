
#if defined(TARGET_WINDOWS)
# define HW_DOS_DONT_DEFINE_MMSYSTEM
# include <windows.h>
# include <mmsystem.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "dosamp.h"
#include "mmsystem.h"

#if defined(TARGET_WINDOWS)
/* dynamic loading of MMSYSTEM/WINMM */
HMODULE             mmsystem_dll = NULL;
unsigned char       mmsystem_tried = 0;

void free_mmsystem(void) {
    if (mmsystem_dll != NULL) {
        FreeLibrary(mmsystem_dll);
        mmsystem_dll = NULL;
    }

    mmsystem_tried = 0;
}

void mmsystem_atexit(void) {
    free_mmsystem();
}

int init_mmsystem(void) {
    if (mmsystem_dll == NULL) {
        if (!mmsystem_tried) {
            UINT oldMode;

            oldMode = SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
#if TARGET_MSDOS == 16
            mmsystem_dll = LoadLibrary("MMSYSTEM");
#else
            mmsystem_dll = LoadLibrary("WINMM.DLL");
#endif
            SetErrorMode(oldMode);

            if (mmsystem_dll != NULL)
                atexit(mmsystem_atexit);

            mmsystem_tried = 1;
        }
    }

    return (mmsystem_dll != NULL) ? 1 : 0;
}

int check_mmsystem_time(void) {
    if (!init_mmsystem())
        return 0;

#if TARGET_MSDOS == 16
    if (GetProcAddress(mmsystem_dll,"TIMEGETTIME") == NULL)
        return 0;
    if (GetProcAddress(mmsystem_dll,"TIMEBEGINPERIOD") == NULL)
        return 0;
    if (GetProcAddress(mmsystem_dll,"TIMEENDPERIOD") == NULL)
        return 0;
#else
    if (GetProcAddress(mmsystem_dll,"timeGetTime") == NULL)
        return 0;
    if (GetProcAddress(mmsystem_dll,"timeBeginPeriod") == NULL)
        return 0;
    if (GetProcAddress(mmsystem_dll,"timeEndPeriod") == NULL)
        return 0;
#endif

    return 1;
}
#endif

