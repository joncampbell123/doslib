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

#if defined(TARGET_PC98)
/*nothing*/
#else

/* NTS: PCjr and Tandy fake video RAM using system memory.
 *      Video RAM memory I/O is redirected to system RAM at
 *      a page controlled by the BIOS. We can read what
 *      it is from the BIOS data area. Let's hope that
 *      byte is valid.
 *
 *      The reason we can't just use the B800 alias is that
 *      for some odd reason the IBM PCjr disregards address
 *      line A14 which makes it impossible to access the
 *      second half of 16KB through it, and therefore prevents
 *      proper use of 320x200x16 and 640x200x4 modes. */
VGA_RAM_PTR get_pcjr_mem(void) {
    unsigned short s;
    unsigned char b;

    if ((vga_state.vga_flags & (VGA_IS_PCJR))) {
        /* PCjr: Must locate the system memory area because the video mem alias is limited to 16KB */
#if TARGET_MSDOS == 32
        b = *((unsigned char*)(0x48A));
#else
        b = *((unsigned char far*)MK_FP(0x40,0x8A));
#endif

        s = ((b >> 3) & 7) << 10;

        /* SAFETY: If s == 0, then return 0xB800 */
        if (s == 0) s = 0xB800;
    }
    else {
        s = 0xB800;
    }

#if TARGET_MSDOS == 32
    return (VGA_RAM_PTR)(s << 4);
#else
    return (VGA_RAM_PTR)MK_FP(s,0);
#endif
}

#endif
