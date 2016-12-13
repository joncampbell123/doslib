
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
static unsigned char far *codebuf_base = NULL;
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

    {
        unsigned segn;

        if (_dos_allocmem(0x1000U>>4U,&segn) != 0) {
            printf("Failed to alloc codebuf\n");
            return 1;
        }
        codebuf = codebuf_base = (unsigned char far*)MK_FP(segn,0);
        codebuf_fence = (unsigned char far*)MK_FP(segn,0x1000U);
    }

	for (irq=3;irq < 8;irq++) {
		vec = 0x08 + irq;
		if (need_patching(vec)) {
			printf("Patching IRQ %u with PIC-friendly code\n",irq);
			patch_irq(vec,irq);
			patched++;
		}
	}
	for (irq=8;irq < 16;irq++) {
		vec = 0x70 + irq - 8;
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

    {
        unsigned int sz = (((unsigned int)(codebuf - codebuf_base)) + 0xFU) >> 4U;

        __asm {
            push    es
            push    ax
            push    bx
            mov     es,word ptr [codebuf+2]     ; segment part
            mov     bx,sz
            mov     ah,0x4A                     ; resize memory block
            int     21h
            pop     bx
            pop     ax
            pop     es
        }
    }

    {
        unsigned short resident_size = 0;

        /* auto-detect the size of this EXE by the MCB preceeding the PSP segment */
        /* do it BEFORE hooking in case shit goes wrong, then we can safely abort. */
        /* the purpose of this code is to compensate for Watcom C's lack of useful */
        /* info at runtime or compile time as to how large we are in memory. */
        {
            unsigned short env_seg=0;
            unsigned short psp_seg=0;
            unsigned char far *mcb;

            __asm {
                push    ax
                push    bx
                mov     ah,0x51     ; get PSP segment
                int     21h
                mov     psp_seg,bx
                pop     bx
                pop     ax
            }

            mcb = MK_FP(psp_seg-1,0);

            /* sanity check */
            if (!(*mcb == 'M' || *mcb == 'Z')/*looks like MCB*/ ||
                *((unsigned short far*)(mcb+1)) != psp_seg/*it's MY memory block*/) {
                printf("Can't figure out my resident size, aborting\n");
                return 1;
            }

            resident_size = *((unsigned short far*)(mcb+3)); /* number of paragraphs */
            if (resident_size < 17) {
                printf("Resident size is too small, aborting\n");
                return 1;
            }

            /* while we're at it, free our environment block as well, we don't need it */
            env_seg = *((unsigned short far*)MK_FP(psp_seg,0x2C));
            if (env_seg != 0) {
                if (_dos_freemem(env_seg) == 0) {
                    *((unsigned short far*)MK_FP(psp_seg,0x2C)) = 0;
                }
                else {
                    printf("WARNING: Unable to free environment block\n");
                }
            }
        }

		_dos_keep(0,resident_size);
	}

    return 0;
}

