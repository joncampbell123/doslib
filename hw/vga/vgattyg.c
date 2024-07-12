
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
#include <hw/vga/vgatty.h>
#include <hw/vga/vgagui.h>

#ifdef TARGET_WINDOWS
# include <hw/dos/winfcon.h>
# include <windows/apihelp.h>
# include <windows/dispdib/dispdib.h>
# include <windows/win16eb/win16eb.h>
#endif

unsigned char vga_break_signal = 0;

int vga_getch() {
    unsigned int c;

#if defined(TARGET_PC98)
    // WARNING: For extended codes this routine assumes you've set the pc98 mapping else it won't work.
    //          The mapping is reprogrammed so that function and editor keys are returned as 0x7F <code>
    //          where the <code> is assigned by us.
    //
    //          Remember that on PC-98 MS-DOS the bytes returned by function and editor keys are by
    //          default ANSI codes but can be changed via INT DCh functions, so we re-define them within
    //          the program as 0x7F <code> (same idea as VZ.EXE)
    //
    //          The INT DCh interface used for this accepts NUL-terminated strings, which means it is
    //          not possible to define IBM PC scan code like strings like 0x00 0x48, so 0x7F is used
    //          instead.
    c = (unsigned int)getch();
    if (c == 0x7Fu) c = (c << 8u) + (unsigned int)getch();
#else
    // IBM PC: Extended codes are returned as 0x00 <scan code>, else normal ASCII
    c = (unsigned int)getch();
    if (c == 0x00u) c = (unsigned int)getch() << 8u;
#endif

    return (int)c;
}

