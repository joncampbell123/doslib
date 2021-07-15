
#if defined(USE_DOSLIB)
# include <hw/cpu/cpu.h>
# include <hw/dos/dos.h>
# include <hw/vga/vga.h>
# include <hw/vesa/vesa.h>
# include <hw/8254/8254.h>
# include <hw/8259/8259.h>
# include <hw/8042/8042.h>
# include <hw/dos/doswin.h>
#endif
#if defined(USE_DOSLIB) || defined(USE_WIN32)
# include <conio.h>
# include <stdint.h>
# include <stdlib.h>
# include <hw/dosboxid/iglib.h> /* for debugging */
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include "utils.h"
#include "debug.h"

#if defined(USE_DOSLIB) || defined(USE_WIN32)
bool			dosbox_ig = false; /* DOSBox Integration Device detected */
#endif

bool			ifedbg_en = false;
char			fatal_tmp[256];

void IFEDBG(const char *msg,...) {
	if (ifedbg_en) {
		va_list va;

		va_start(va,msg);
		vsnprintf(fatal_tmp,sizeof(fatal_tmp)/*includes NUL byte*/,msg,va);
		va_end(va);

#if defined(USE_DOSLIB) || defined(USE_WIN32)
		if (dosbox_ig) {
			dosbox_id_debug_message("IFEDBG: ");
			dosbox_id_debug_message(fatal_tmp);
			dosbox_id_debug_message("\n");
		}
#endif
#if defined(USE_SDL2)
		fprintf(stderr,"IFEDBG: %s\n",fatal_tmp);
#endif
	}
}

#if defined(USE_DOSLIB)
void IFE_win95_tf_hang_check(void) {
	if (cpu_clear_TF()) IFEDBG("Windows 95 hang mitigation: TF (Trap Flag) was set, clearing");
}
#endif

