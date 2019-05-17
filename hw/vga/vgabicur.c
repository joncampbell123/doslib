#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <malloc.h>
#include <fcntl.h>
#include <ctype.h>
#include <i86.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#include <hw/dos/doswin.h>

#ifdef TARGET_WINDOWS
# include <hw/dos/winfcon.h>
# include <windows/apihelp.h>
# include <windows/dispdib/dispdib.h>
# include <windows/win16eb/win16eb.h>
#endif

#if defined(TARGET_PC98)
/*nothing*/
#else

void vga_sync_bios_cursor() {
#ifndef TARGET_WINDOWS
# if TARGET_MSDOS == 32
	*((unsigned char*)0x450) = vga_state.vga_pos_x;
	*((unsigned char*)0x451) = vga_state.vga_pos_y;
# else
	*((unsigned char far*)MK_FP(0x40,0x50)) = vga_state.vga_pos_x;
	*((unsigned char far*)MK_FP(0x40,0x51)) = vga_state.vga_pos_y;
# endif
#endif
}

#endif
