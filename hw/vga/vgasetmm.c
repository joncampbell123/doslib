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

/* EGA/VGA only */
void vga_set_memory_map(unsigned char c) {
	unsigned char b;

	if (vga_state.vga_flags & VGA_IS_VGA) {
		b = vga_read_GC(6);
		vga_write_GC(6,(b & 0xF3) | (c << 2));
		update_state_vga_memory_map_select(c);
	}
	else if (vga_state.vga_flags & VGA_IS_EGA) {
		/* guessing code because you can't readback regs on EGA */
		b = int10_getmode();
		/* INT 10H text modes: set odd/even,   else, set alpha disable */
		vga_write_GC(6,0x02 | (c << 2));
		update_state_vga_memory_map_select(c);
	}
}

#endif

