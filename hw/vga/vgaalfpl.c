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

void vga_alpha_switch_to_font_plane() {
	vga_write_GC(0x4,0x02); /* NTS: Read Map Select: This is very important if the caller wants to read from the font plane without reading back gibberish */
	vga_write_GC(0x5,0x00);
	vga_write_GC(0x6,0x0C);
	vga_write_sequencer(0x2,0x04);
	vga_write_sequencer(0x4,0x06);
}

void vga_alpha_switch_from_font_plane() {
	vga_write_GC(0x4,0x00);
	vga_write_GC(0x5,0x10);
	vga_write_GC(0x6,0x0E);
	vga_write_sequencer(0x2,0x03);
	vga_write_sequencer(0x4,0x02);
}

#endif

