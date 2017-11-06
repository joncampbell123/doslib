
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

