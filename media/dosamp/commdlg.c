
#if defined(TARGET_WINDOWS)
# define HW_DOS_DONT_DEFINE_MMSYSTEM
# include <windows.h>
# include <commdlg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "commdlg.h"

#if defined(TARGET_WINDOWS)
/* dynamic loading of COMMDLG */
HMODULE             commdlg_dll = NULL;
unsigned char       commdlg_tried = 0;

GETOPENFILENAMEPROC commdlg_getopenfilenameproc(void) {
#if TARGET_MSDOS == 16
    if (commdlg_dll != NULL)
        return (GETOPENFILENAMEPROC)GetProcAddress(commdlg_dll,"GETOPENFILENAME");
#else
    if (commdlg_dll != NULL)
        return (GETOPENFILENAMEPROC)GetProcAddress(commdlg_dll,"GetOpenFileNameA");
#endif

    return NULL;
}

void free_commdlg(void) {
    if (commdlg_dll != NULL) {
        FreeLibrary(commdlg_dll);
        commdlg_dll = NULL;
    }

    commdlg_tried = 0;
}

void commdlg_atexit(void) {
    free_commdlg();
}

int init_commdlg(void) {
    if (commdlg_dll == NULL) {
        if (!commdlg_tried) {
            UINT oldMode;

            oldMode = SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
#if TARGET_MSDOS == 16
            commdlg_dll = LoadLibrary("COMMDLG");
#else
            commdlg_dll = LoadLibrary("COMDLG32.DLL");
#endif
            SetErrorMode(oldMode);

            if (commdlg_dll != NULL)
                atexit(commdlg_atexit);

            commdlg_tried = 1;
        }
    }

    return (commdlg_dll != NULL) ? 1 : 0;
}
#endif

