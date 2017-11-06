
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

#if defined(LINUX)
/* case sensitive */
int fs_comparechar(char c1,char c2) {
    return (c1 == c2);
}
#else
/* case insensitive */
int fs_comparechar(char c1,char c2) {
    return (tolower(c1) == tolower(c2));
}
#endif

