
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/dos/dosbox.h>
#include <hw/8237/8237.h>		/* 8237 DMA */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/dos/doswin.h>
#include <hw/dos/tgusmega.h>
#include <hw/dos/tgussbos.h>

/* Windows 9x VxD enumeration */
#include <windows/w9xvmm/vxd_enum.h>

/* uncomment this to enable debugging messages */
//#define DBG

#if defined(DBG)
# define DEBUG(x) (x)
#else
# define DEBUG(x)
#endif

#if TARGET_MSDOS == 32
signed char			sndsb_nmi_32_hook = -1;
#endif

#if TARGET_MSDOS == 32
int sb_nmi_32_auto_choose_hook() {
	if (sndsb_nmi_32_hook >= 0)
		return sndsb_nmi_32_hook;

	/* auto-detect SBOS/MEGA-EM and enable nmi reflection if present */
	if (gravis_mega_em_detect(&megaem_info) || gravis_sbos_detect() >= 0)
		return 1;

	return 0;
}
#endif

