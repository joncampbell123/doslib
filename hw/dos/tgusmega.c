
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/tgusmega.h>

#if !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
struct mega_em_info megaem_info={0};

int gravis_mega_em_detect(struct mega_em_info *x) {
/* TODO: Cache results, only need to scan once */
    union REGS regs={0};
    regs.w.ax = 0xFD12;
    regs.w.bx = 0x3457;
#if TARGET_MSDOS == 32
    int386(0x21,&regs,&regs);
#else
    int86(0x21,&regs,&regs);
#endif
    if (regs.w.ax == 0x5678) {
	x->intnum = regs.h.cl;
	x->version = regs.w.dx;
	x->response = regs.w.bx;

	if (x->version == 0) {
	    if (x->response == 0x1235)
	    	x->version = 0x200;
	    else if (x->response == 0x1237)
	    	x->version = 0x300;
	}
	return 1;
    }
    return 0;
}
#endif

