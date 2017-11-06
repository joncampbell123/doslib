
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
#include "fs.h"

#include "pof_gofn.h"

#if defined(TARGET_WINDOWS)
char *prompt_open_file_windows_gofn(void) {
    GETOPENFILENAMEPROC gofn;

    /* GetOpenFileName() did not appear until Windows 3.1 */
    if (!init_commdlg())
        return NULL;
    if ((gofn=commdlg_getopenfilenameproc()) == NULL)
        return NULL;

    {
        char tmp[300];
        OPENFILENAME of;

        memset(&of,0,sizeof(of));
        memset(tmp,0,sizeof(tmp));

        of.lStructSize = sizeof(of);
#ifdef USE_WINFCON
        of.hwndOwner = _win_hwnd();
        of.hInstance = _win_hInstance;
#else
        of.hInstance = GetModuleHandle(NULL);
#endif
        of.lpstrFilter =
            "All supported files\x00*.wav\x00"
            "WAV files\x00*.wav\x00"
            "All files\x00*.*\x00";
        of.nFilterIndex = 1;
        if (wav_file != NULL) strncpy(tmp,wav_file,sizeof(tmp)-1);
        of.lpstrFile = tmp;
        of.nMaxFile = sizeof(tmp)-1;
        of.lpstrTitle = "Select file to play";
        of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (!gofn(&of))
            return NULL;

        return strdup(tmp);
    }
}
#endif

