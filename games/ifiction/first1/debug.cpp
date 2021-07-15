
#if defined(USE_WIN32)
# include <windows.h>
# include <mmsystem.h>
#endif

#if defined(USE_DOSLIB)
# include <hw/cpu/cpu.h>
# include <hw/dos/dos.h>
# if defined(TARGET_PC98)
#  error PC-98 target removed
// REMOVED
# else
#  include <hw/vga/vga.h>
#  include <hw/vesa/vesa.h>
# endif
# include <hw/8254/8254.h>
# include <hw/8259/8259.h>
# include <hw/8042/8042.h>
# include <hw/dos/doswin.h>
# include <hw/dosboxid/iglib.h> /* for debugging */
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#if defined(USE_SDL2)
# if defined(__APPLE__) /* Brew got the headers wrong here */
#  include <SDL.h>
# else
#  include <SDL2/SDL.h>
# endif
#endif

#include "utils.h"
#include "debug.h"

#if defined(USE_DOSLIB)
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
	}
	else {
		fatal_tmp[0] = 0;
	}

#if defined(USE_DOSLIB)
	if (dosbox_ig) {
		dosbox_id_debug_message("IFEDBG: ");
		dosbox_id_debug_message(fatal_tmp);
		dosbox_id_debug_message("\n");
	}
#endif
}

