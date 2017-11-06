
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
#include "shdropls.h"
#include "shdropwn.h"

#include "pof_gofn.h"
#include "pof_tty.h"

#if defined(TARGET_WINDOWS)
#include <shellapi.h>

static char *shell_queryfile(HDROP hDrop,UINT i) {
    char *str = NULL;
    UINT sz;

    sz = __DragQueryFile(hDrop, i, NULL, 0);
    if (sz != 0U) {
        str = malloc(sz+1+1);
        if (str != NULL) {
            str[sz] = 0;
            str[sz+1] = 0;
            if (__DragQueryFile(hDrop, i, str, sz + 1/*Despite MSDN docs, the last char written at [sz-1] is NUL*/) == 0U) {
                free(str);
                str = NULL;
            }
        }
    }

    return str;
}

unsigned int shell_dropfileshandler(HDROP hDrop) {
    struct shell_droplist_t **appendto;
    UINT i,files;
    char *str;

    appendto = &shell_droplist;
    while (*appendto != NULL) appendto = &((*appendto)->next);

    files = __DragQueryFile(hDrop, (UINT)(-1), NULL, 0);
    if (files != 0U) {
        for (i=0;i < files;i++) {
            str = shell_queryfile(hDrop, i);
            if (str != NULL) {
                /* assume *appendto == NULL */
                *appendto = calloc(1, sizeof(struct shell_droplist_t));
                if (*appendto != NULL) {
                    (*appendto)->file = str;
                    appendto = &((*appendto)->next);
                }
                else {
                    free(str);
                }
            }
        }
    }

    __DragFinish(hDrop);
    return 0;
}
#endif

