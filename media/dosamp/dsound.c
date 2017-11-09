
#if defined(TARGET_WINDOWS)
# define HW_DOS_DONT_DEFINE_MMSYSTEM
# include <windows.h>
#endif

#include <stdint.h>
#include <stdio.h>

#include "dosamp.h"
#include "dsound.h"

#if defined(HAS_DSOUND)
HMODULE             dsound_dll = NULL;
unsigned char       dsound_tried = 0;

HRESULT (WINAPI *__DirectSoundCreate)(LPGUID lpGuid,LPDIRECTSOUND* ppDS,LPUNKNOWN pUnkOuter) = NULL;

void free_dsound(void) {
    if (dsound_dll != NULL) {
        FreeLibrary(dsound_dll);
        dsound_dll = NULL;
    }

    dsound_tried = 0;
}

void dsound_atexit(void) {
    free_dsound();
}

int init_dsound(void) {
    if (dsound_dll == NULL) {
        if (!dsound_tried) {
            UINT oldMode;

            oldMode = SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
            dsound_dll = LoadLibrary("DSOUND.DLL");
            SetErrorMode(oldMode);

            if (dsound_dll != NULL) {
                atexit(dsound_atexit);
                __DirectSoundCreate=(HRESULT (WINAPI *)(LPGUID,LPDIRECTSOUND*,LPUNKNOWN))GetProcAddress(dsound_dll,"DirectSoundCreate");
            }

            dsound_tried = 1;
        }
    }

    return (dsound_dll != NULL) ? 1 : 0;
}
#endif

