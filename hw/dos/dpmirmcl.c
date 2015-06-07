/* dos.c
 *
 * Code to detect the surrounding DOS/Windows environment and support routines to work with it
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

#ifdef TARGET_WINDOWS
# include <windows.h>
#endif

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
#include <hw/dos/doswin.h>
#include <hw/dos/dosntvdm.h>

#if TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
unsigned int dpmi_test_rm_entry_call(struct dpmi_realmode_call *rc) {
	unsigned int res = 0;

	__asm {
		mov	ax,0x0301
		xor	bx,bx
		xor	cx,cx
		mov	edi,rc		; we trust Watcom has left ES == DS
		int	0x31		; call DPMI
		jnc	ok
		
		mov	res,1		; just incase some fucked up DPMI server returns CF=1 EAX=0
		or	eax,eax
		jz	ok
		
		mov	res,eax		; OK store the error code as-is
ok:
	}

	return res;
}

static unsigned char *alt_rm_call = NULL;
static uint16_t alt_rm_call_sel = 0;

/* using this hack, subvert INT 06h (invalid opcode exception)
   which the BIOS and DOS are unlikely to use during this hack */
#define ALT_INT 0x06

int dpmi_alternate_rm_call(struct dpmi_realmode_call *rc) {
	uint32_t oe;

	if (alt_rm_call == NULL) {
		alt_rm_call = dpmi_alloc_dos(32,&alt_rm_call_sel);
		if (alt_rm_call == NULL) {
			fprintf(stderr,"FATAL: DPMI alternate call: cannot alloc\n");
			return 0;
		}
	}

	/* Fuck you DOS4/GW! */
	/* prepare executable code */
	alt_rm_call[0] = 0x9A;	/* CALL FAR IMM */
	*((uint16_t*)(alt_rm_call+1)) = rc->ip;
	*((uint16_t*)(alt_rm_call+3)) = rc->cs;
	alt_rm_call[5] = 0xCF;	/* IRET */

	/* replace real-mode interrupt vector */
	_cli();
	oe = ((uint32_t*)0x00000000)[ALT_INT];
	((uint32_t*)0x00000000)[ALT_INT] =
		(((uint32_t)alt_rm_call >> 4UL) << 16UL) | /* seg */
		((uint32_t)alt_rm_call & 0xFUL); /* ofs */
	_sti();

	/* call it! */
	__asm {
		mov	ax,0x0300
		mov	bx,ALT_INT
		xor	cx,cx
		mov	edi,rc		; we trust Watcom has left ES == DS
		int	0x31		; call DPMI
	}

	/* restore interrupt vector */
	_cli();
	((uint32_t*)0x00000000)[ALT_INT] = oe;
	_sti();

	return 1;
}

int dpmi_alternate_rm_call_stacko(struct dpmi_realmode_call *rc) {
	uint32_t oe;

	if (alt_rm_call == NULL) {
		alt_rm_call = dpmi_alloc_dos(32,&alt_rm_call_sel);
		if (alt_rm_call == NULL) {
			fprintf(stderr,"FATAL: DPMI alternate call: cannot alloc\n");
			return 0;
		}
	}

	/* Fuck you DOS4/GW! */
	/* prepare executable code */
	{
		static unsigned char code[] = {
			0xFC,		/* CLD */
			0x8C,0xD0,	/* MOV AX,SS */
			0x8E,0xD8,	/* MOV DS,AX */
			0x8E,0xC0,	/* MOV ES,AX */
			0x89,0xE5,	/* MOV BP,SP */
			0x8D,0x76,0x06,	/* LEA SI,[BP+6] <- 6 byte interrupt stack */
			0x83,0xEC,0x20,	/* SUB SP,0x20 */
			0xB9,0x10,0x00,	/* MOV CX,0x10 */
			0x89,0xE7,	/* MOV DI,SP */
			0xF3,0xA5	/* REP MOVSW */
		};
		memcpy(alt_rm_call,code,0x16);
	}
	
	alt_rm_call[0x16] = 0x9A;	/* CALL FAR IMM */
	*((uint16_t*)(alt_rm_call+0x16+1)) = rc->ip;
	*((uint16_t*)(alt_rm_call+0x16+3)) = rc->cs;
	alt_rm_call[0x16+5] = 0x89;	/* MOV SP,BP */
	alt_rm_call[0x16+6] = 0xEC;
	alt_rm_call[0x16+7] = 0xCF;	/* IRET */

	/* replace real-mode interrupt vector */
	_cli();
	oe = ((uint32_t*)0x00000000)[ALT_INT];
	((uint32_t*)0x00000000)[ALT_INT] =
		(((uint32_t)alt_rm_call >> 4UL) << 16UL) | /* seg */
		((uint32_t)alt_rm_call & 0xFUL); /* ofs */
	_sti();

	/* call it! */
	__asm {
		mov	ax,0x0300
		mov	bx,ALT_INT
		xor	cx,cx
		mov	edi,rc		; we trust Watcom has left ES == DS
		int	0x31		; call DPMI
	}

	/* restore interrupt vector */
	_cli();
	((uint32_t*)0x00000000)[ALT_INT] = oe;
	_sti();

	return 1;
}
#undef ALT_INT
#endif

#if TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
void mux_realmode_2F_call(struct dpmi_realmode_call *rc) {
	__asm {
		mov	ax,0x0300
		mov	bx,0x002F
		xor	cx,cx
		mov	edi,rc		; we trust Watcom has left ES == DS
		int	0x31		; call DPMI
	}
}
#endif
#if TARGET_MSDOS == 16 && defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
void mux_realmode_2F_call(struct dpmi_realmode_call far *rc) {
	__asm {
		push	es
		mov	ax,0x0300
		mov	bx,0x002F
		xor	cx,cx
		mov	di,word ptr [rc+2]
		mov	es,di
		mov	di,word ptr [rc]
		int	0x31		; call DPMI
		pop	es
	}
}
#endif

