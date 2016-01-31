
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/dos/dos.h>

#if TARGET_MSDOS == 32
# error this is for 16 bit only
#endif

static unsigned char far *codebuf = NULL;
static unsigned char far *codebuf_fence = NULL;

static int need_patching(unsigned int vec) {
	uint32_t vector = *((uint32_t far*)MK_FP(0,(vec*4)));
	const unsigned char far *p = (const unsigned char far*)MK_FP(vector>>16UL,vector&0xFFFFUL);

	if (vector == 0) return 1;
	if (p == NULL) return 1;

	if (*p == 0xCF) {
		// IRET. Yep.
		return 1;
	}
	else if (p[0] == 0xFE && p[1] == 0x38) {
		// DOSBox callback. Probably not going to ACK the interrupt.
		return 1;
	}

	return 0;
}

static void patch_irq(unsigned int vec,unsigned int irq) {
	unsigned char far *p;

	if ((codebuf + 16) > codebuf_fence) {
		printf("ERROR: Out of patch space\n");
		return;
	}

	p = codebuf;
	*codebuf++ = 0x50;		// PUSH AX
	*codebuf++ = 0xB0;		// MOV AL,20
	*codebuf++ = 0x20;
	if (irq >= 8) {
	*codebuf++ = 0xE6;		// OUT A0,AL
	*codebuf++ = 0xA0;
	}
	*codebuf++ = 0xE6;		// OUT 20,AL
	*codebuf++ = 0x20;
	*codebuf++ = 0x58;		// POP AX
	*codebuf++ = 0xCF;		// IRET

	_cli();
	*((uint32_t far*)MK_FP(0,(vec*4))) = (uint32_t)MK_FP(FP_SEG(p),FP_OFF(p));
	_sti();
}

int main(int argc,char **argv) {
	unsigned int irq,patched = 0,vec;

	printf("IRQPATCH (C) 2016 Jonathan Campbell all rights reserved.\n");
	printf("IRQ interrupt patching utility to improve interrupt handling\n");
	printf("for DOS applications that chain to the previous handler.\n");

	codebuf = _fmalloc(0x1000U>>4U); // 4KB
	if (codebuf == NULL) {
		printf("Failed to alloc codebuf\n");
		return 1;
	}
	codebuf_fence = (unsigned char far*)MK_FP(FP_SEG(codebuf),FP_OFF(codebuf)+0x1000U);

	for (irq=2;irq < 8;irq++) {
		vec = 0x08 + irq;
		if (need_patching(vec)) {
			printf("Patching IRQ %u with PIC-friendly code\n",irq);
			patch_irq(vec,irq);
			patched++;
		}
	}
	for (irq=8;irq < 16;irq++) {
		vec = 0x70 + irq;
		if (need_patching(vec)) {
			printf("Patching IRQ %u with PIC-friendly code\n",irq);
			patch_irq(vec,irq);
			patched++;
		}
	}

	if (patched == 0) {
		printf("No IRQs patched.\n");
		return 0;
	}

	_dos_keep(0,0); /* discard our code */
	return 0;
}

