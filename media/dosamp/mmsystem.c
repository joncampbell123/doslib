
#if defined(TARGET_WINDOWS)
# define HW_DOS_DONT_DEFINE_MMSYSTEM
# include <windows.h>
# include <commdlg.h>
# include <mmsystem.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#ifdef LINUX
#include <signal.h>
#include <endian.h>
#include <dirent.h>
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

#ifndef LINUX
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
#endif

#include "wavefmt.h"
#include "dosamp.h"
#include "timesrc.h"
#include "dosptrnm.h"
#include "filesrc.h"
#include "resample.h"
#include "cvrdbuf.h"
#include "cvip.h"
#include "trkrbase.h"
#include "tmpbuf.h"
#include "snirq.h"
#include "sndcard.h"
#include "termios.h"
#include "cstr.h"

#include "sc_sb.h"
#include "sc_oss.h"
#include "sc_alsa.h"
#include "sc_winmm.h"
#include "sc_dsound.h"

#include "dsound.h"
#include "winshell.h"
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

