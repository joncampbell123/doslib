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

void vga_enable_256color_modex() {
	vga_write_sequencer(4,0x06);
	vga_write_sequencer(0,0x01);
	vga_write_CRTC(0x17,0xE3);
	vga_write_CRTC(0x14,0x00);
	vga_write_sequencer(0,0x03);
	vga_write_sequencer(VGA_SC_MAP_MASK,0xF);
}

