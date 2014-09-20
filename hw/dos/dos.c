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

/* DEBUG: Flush out calls that aren't there */
#ifdef TARGET_OS2
# define int86 ___EVIL___
# define int386 ___EVIL___
# define ntvdm_RegisterModule ___EVIL___
# define ntvdm_UnregisterModule ___EVIL___
# define _dos_getvect ___EVIL___
# define _dos_setvect ___EVIL___
#endif

struct mega_em_info megaem_info={0};
struct lib_dos_options lib_dos_option={0};

/* DOS "list of lists" pointer */
unsigned char FAR *dos_LOL=NULL;

/* DOS version info */
uint8_t dos_flavor = 0;
uint16_t dos_version = 0;
uint32_t freedos_kernel_version = 0;
const char *dos_version_method = NULL;
const char *windows_version_method = NULL;
#if TARGET_MSDOS == 32 && !defined(TARGET_OS2)
int8_t dpmi_no_0301h = -1; /* whether or not the DOS extender provides function 0301h */
#endif

unsigned short smartdrv_version = 0xFFFF;
int smartdrv_fd = -1;

#if defined(NTVDM_CLIENT) && !defined(TARGET_WINDOWS)
uint8_t ntvdm_dosntast_tried = 0;
uint16_t ntvdm_dosntast_handle = (~0U);
#endif

#if TARGET_MSDOS == 32
char *freedos_kernel_version_str = NULL;
#else
char far *freedos_kernel_version_str = NULL;
#endif

/* return value:
 *   0 = not running under Windows
 *   1 = running under Windows */
const char *windows_emulation_comment_str = NULL;
uint8_t windows_emulation = 0;
uint16_t windows_version = 0;		/* NOTE: 0 for Windows NT */
uint8_t windows_mode = 0;
uint8_t windows_init = 0;

unsigned char vcpi_probed = 0;
unsigned char vcpi_present = 0;
unsigned char vcpi_major_version,vcpi_minor_version;

const char *windows_mode_strs[WINDOWS_MAX] = {
	"None",
	"Real",
	"Standard",
	"Enhanced",
	"NT",
	"OS/2"
};

#if TARGET_MSDOS == 16 || (TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS))
uint16_t dpmi_flags=0;
uint16_t dpmi_version=0;
unsigned char dpmi_init=0;
uint32_t dpmi_entry_point=0;
unsigned char dpmi_present=0;
unsigned char dpmi_processor_type=0;
uint16_t dpmi_private_data_length_paragraphs=0;
uint16_t dpmi_private_data_segment=0xFFFF;	/* when we DO enter DPMI, we store the private data segment here. 0 = no private data. 0xFFFF = not yet entered */
unsigned char dpmi_entered = 0;	/* 0=not yet entered, 16=entered as 16bit, 32=entered as 32bit */
uint64_t dpmi_rm_entry = 0;
uint32_t dpmi_pm_entry = 0;

/* once having entered DPMI, keep track of the selectors registers given to us in p-mode */
uint16_t dpmi_pm_cs,dpmi_pm_ds,dpmi_pm_es,dpmi_pm_ss;

void __cdecl dpmi_enter_core(); /* Watcom's inline assembler is too limiting to carry out the DPMI entry and switch back */
#endif

#if TARGET_MSDOS == 32
int dpmi_linear_lock(uint32_t lin,uint32_t size) {
	int retv = 0;

	__asm {
		mov	ax,0x0600
		mov	cx,word ptr lin
		mov	bx,word ptr lin+2
		mov	di,word ptr size
		mov	si,word ptr size+2
		int	0x31
		jc	endf
		mov	retv,1
endf:
	}

	return retv;
}

int dpmi_linear_unlock(uint32_t lin,uint32_t size) {
	int retv = 0;

	__asm {
		mov	ax,0x0601
		mov	cx,word ptr lin
		mov	bx,word ptr lin+2
		mov	di,word ptr size
		mov	si,word ptr size+2
		int	0x31
		jc	endf
		mov	retv,1
endf:
	}

	return retv;
}

void *dpmi_phys_addr_map(uint32_t phys,uint32_t size) {
	uint32_t retv = 0;

	__asm {
		mov	ax,0x0800
		mov	cx,word ptr phys
		mov	bx,word ptr phys+2
		mov	di,word ptr size
		mov	si,word ptr size+2
		int	0x31
		jc	endf
		mov	word ptr retv,cx
		mov	word ptr retv+2,bx
endf:
	}

	return (void*)retv;
}

void dpmi_phys_addr_free(void *base) {
	__asm {
		mov	ax,0x0801
		mov	cx,word ptr base
		mov	bx,word ptr base+2
		int	0x31
	}
}

void *dpmi_alloc_dos(unsigned long len,uint16_t *selector) {
	unsigned short rm=0,pm=0,fail=0;

	/* convert len to paragraphs */
	len = (len + 15) >> 4UL;
	if (len >= 0xFF00UL) return NULL;

	__asm {
		mov	bx,WORD PTR len
		mov	ax,0x100
		int	0x31

		mov	rm,ax
		mov	pm,dx
		sbb	ax,ax
		mov	fail,ax
	}

	if (fail) return NULL;

	*selector = pm;
	return (void*)((unsigned long)rm << 4UL);
}

void dpmi_free_dos(uint16_t selector) {
	__asm {
		mov	ax,0x101
		mov	dx,selector
		int	0x31
	}
}
#endif

#if TARGET_MSDOS == 32
int _dos_xread(int fd,void *buffer,int bsz) { /* NTS: The DOS extender takes care of translation here for us */
	int rd = -1;
	__asm {
		mov	ah,0x3F
		mov	ebx,fd
		mov	ecx,bsz
		mov	edx,buffer
		int	0x21
		mov	ebx,eax
		sbb	ebx,ebx
		or	eax,ebx
		mov	rd,eax
	}
	return rd;
}
#else
int _dos_xread(int fd,void far *buffer,int bsz) {
	int rd = -1;
	__asm {
		mov	ah,0x3F
		mov	bx,fd
		mov	cx,bsz
		mov	dx,word ptr [buffer+0]
		mov	si,word ptr [buffer+2]
		push	ds
		mov	ds,si
		int	0x21
		pop	ds
		mov	bx,ax
		sbb	bx,bx
		or	ax,bx
		mov	rd,ax
	}
	return rd;
}
#endif

#if TARGET_MSDOS == 32
int _dos_xwrite(int fd,void *buffer,int bsz) { /* NTS: The DOS extender takes care of translation here for us */
	int rd = -1;
	__asm {
		mov	ah,0x40
		mov	ebx,fd
		mov	ecx,bsz
		mov	edx,buffer
		int	0x21
		mov	ebx,eax
		sbb	ebx,ebx
		or	eax,ebx
		mov	rd,eax
	}
	return rd;
}
#else
int _dos_xwrite(int fd,void far *buffer,int bsz) {
	int rd = -1;
	__asm {
		mov	ah,0x40
		mov	bx,fd
		mov	cx,bsz
		mov	dx,word ptr [buffer+0]
		mov	si,word ptr [buffer+2]
		push	ds
		mov	ds,si
		int	0x21
		pop	ds
		mov	bx,ax
		sbb	bx,bx
		or	ax,bx
		mov	rd,ax
	}
	return rd;
}
#endif

/* TODO: Windows 3.1/95/98/ME have a DPMI server underneath.
 *       It would be neato at some point if these functions were
 *       available for use from Windows 3.1 Win16/Win32, and
 *       Windows 95/98/ME Win32 */
#if TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
void dpmi_free_descriptor(uint16_t d) {
	union REGS regs = {0};
	regs.w.ax = 0x0001;	/* DPMI free descriptor */
	regs.w.bx = d;
	int386(0x31,&regs,&regs);
}

uint16_t dpmi_alloc_descriptors(uint16_t c) {
	union REGS regs = {0};
	regs.w.ax = 0x0000;	/* allocate descriptor */
	regs.w.cx = 1;		/* just one */
	int386(0x31,&regs,&regs);
	if (regs.w.cflag & 1) return 0;
	return regs.w.ax;
}

unsigned int dpmi_set_segment_base(uint16_t sel,uint32_t base) {
	union REGS regs = {0};
	regs.w.ax = 0x0007;	/* set segment base */
	regs.w.bx = sel;
	regs.w.cx = base >> 16UL;
	regs.w.dx = base;
	int386(0x31,&regs,&regs);
	if (regs.w.cflag & 1) return 0;
	return 1;
}

unsigned int dpmi_set_segment_limit(uint16_t sel,uint32_t limit) {
	union REGS regs = {0};
	regs.w.ax = 0x0008;	/* set segment limit */
	regs.w.bx = sel;
	regs.w.cx = limit >> 16UL;
	regs.w.dx = limit;
	int386(0x31,&regs,&regs);
	if (regs.w.cflag & 1) return 0;
	return 1;
}

unsigned int dpmi_set_segment_access(uint16_t sel,uint16_t access) {
	union REGS regs = {0};
	unsigned char c=0;

	/* the DPL/CPL value we give to the DPMI function below must match our privilege level, so
	 * get that value from our own selector */
	__asm {
		push	eax
		movzx	eax,sel
		and	al,3
		mov	c,al
		pop	eax
	}

	regs.w.ax = 0x0009;	/* set segment access rights */
	regs.w.bx = sel;
	regs.w.cx = (access & 0xFF9F) | (c << 5);	/* readable, code, CPL=same, present=1, 16-bit byte granular */
	int386(0x31,&regs,&regs);
	if (regs.w.cflag & 1) return 0;
	return 1;
}
#endif

#if TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
static unsigned int dpmi_test_rm_entry_call(struct dpmi_realmode_call *rc) {
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
static void mux_realmode_2F_call(struct dpmi_realmode_call *rc) {
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
static void mux_realmode_2F_call(struct dpmi_realmode_call far *rc) {
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

/* 16-bit real mode DOS or 16-bit protected mode Windows */
void probe_dpmi() {
#if defined(TARGET_OS2)
	/* OS/2 apps do not run under DPMI */
#elif TARGET_MSDOS == 32 && defined(TARGET_WINDOWS)
	/* Win32 apps do not bother with DPMI */
#else
	if (!dpmi_init) {
		/* BUGFIX: WINE (Wine Is Not an Emulator) can run Win16 applications
		 *         but does not emulate the various low level interrupts one
		 *         can call. To avoid crashing under WINE we must not use
		 *         direct interrupts. */
		if (windows_emulation == WINEMU_WINE) {
			dpmi_present = 0;
			dpmi_init = 1;
			return;
		}

		{
			unsigned char present=0,proc=0;
			uint16_t version=0,privv=0,flags=0;
			uint32_t entry=0;

			__asm {
				push	es
				mov	ax,0x1687
				int	2Fh
				or	ax,ax
				jnz	err1
				mov	present,1
				mov	flags,bx
				mov	proc,cl
				mov	version,dx
				mov	privv,si
				mov	word ptr [entry+0],di
				mov	word ptr [entry+2],es
				pop	es
err1:
			}

			dpmi_flags = flags;
			dpmi_present = present;
			dpmi_version = version;
			dpmi_entry_point = entry;
			dpmi_processor_type = proc;
			dpmi_private_data_segment = 0xFFFF;
			dpmi_private_data_length_paragraphs = privv;
		}

#if TARGET_MSDOS == 32 || defined(TARGET_WINDOWS)
		/* when we ask for the "entry point" we mean we want the real-mode entrypoint.
		 * apparently some DPMI servers like Windows XP+NTVDM.EXE translate ES:DI coming
		 * back to a protected mode entry point, which is not what we're looking for.
		 *
		 * Interesting fact: When compiled as a Win16 app, the DPMI call actually works,
		 * but returns an entry point appropriate for Win16 apps. So.... apparently we
		 * can enter DPMI protected mode from within Win16? Hmm.... that might be something
		 * fun to experiment with :) */
		if (dpmi_present) {
			struct dpmi_realmode_call rc={0};
			rc.eax = 0x1687;
			mux_realmode_2F_call(&rc);
			if ((rc.eax&0xFFFF) == 0) {
				dpmi_flags = rc.ebx&0xFFFF;
				dpmi_present = 1;
				dpmi_version = rc.edx&0xFFFF;
				dpmi_entry_point = (((uint32_t)rc.es << 16UL) & 0xFFFF0000UL) + (uint32_t)(rc.edi&0xFFFFUL);
				dpmi_processor_type = rc.ecx&0xFF;
				dpmi_private_data_segment = 0xFFFF;
				dpmi_private_data_length_paragraphs = rc.esi&0xFFFF;
			}
			else {
				dpmi_present = 0;
			}
		}
#endif

#if TARGET_MSDOS == 32 && !defined(TARGET_OS2)
		dpmi_no_0301h = 0;
#endif
		dpmi_init = 1;

#if TARGET_MSDOS == 32 && !defined(TARGET_OS2)
		if (dpmi_present) {
			/* Thanks to bullshit like DOS4/GW we have to test the extender to know
			   whether or not core routines we need are actually there or not. If they
			   are not, then alternative workarounds are required. The primary reason
			   for this test is to avoid HIMEM.SYS API code returning nonsensical values
			   caused by DOS4/GW not supporting such vital functions as DPMI 0301H:
			   Call far real-mode procedure. Knowing this should also fix the VESA BIOS
			   test bug where the protected-mode version is unable to use the BIOS's
			   direct-call window bank-switching function.

			   At least, this code so far can rely on DPMI function 0300H: call real-mode
			   interrupt.*/

			/* test #1: allocate a 16-bit region, put a RETF instruction there,
			   and ask the DPMI server to call it (0301H test).

				Success:
				Registers unchanged
				CF=0

				Failure (DOS4/GW):
				CF=1
				AX=0301H  (wait wha?) */
			{
				uint16_t sel = 0;
				struct dpmi_realmode_call rc={0};
				unsigned char *proc = dpmi_alloc_dos(16,&sel);
				if (proc != NULL) {
					*proc = 0xCB; /* <- RETF */

					rc.cs = ((size_t)proc) >> 4UL;
					rc.ip = ((size_t)proc) & 0xFUL;
					if (dpmi_test_rm_entry_call(&rc) != 0)
						dpmi_no_0301h = 1;

					dpmi_free_dos(sel);
				}
			}
		}
#endif
	}
#endif
}

void probe_dos() {
#if TARGET_MSDOS == 32 && 0
	assert(sizeof(struct dpmi_realmode_call) == 0x32);
	assert(offsetof(struct dpmi_realmode_call,ss) == 0x30);
	assert(offsetof(struct dpmi_realmode_call,cs) == 0x2C);
#endif

	if (dos_version == 0) {
#ifdef TARGET_WINDOWS
# if TARGET_MSDOS == 32
#  ifdef WIN386
/* =================== Windows 3.0/3.1 Win386 ================= */
		DWORD raw = GetVersion(); /* NTS: The Win16 version does not tell us if we're running under Windows NT */

		dos_version_method = "GetVersion";
		dos_version = (((raw >> 24UL) & 0xFFUL) << 8UL) | (((raw >> 16UL) & 0xFFUL) << 0UL);

		/* Windows NT/2000/XP NTVDM.EXE lies to us, reporting Windows 95 numbers and MS-DOS 5.0 */
		if (dos_version == 0x500) {
			uint16_t x = 0;

			/* Sorry Microsoft, but you make it hard for us to detect and we have to break your OS to find the info we need */
			__asm {
				mov	ax,0x3306
				mov	bx,0
				int	21h
				jc	err1
				mov	x,bx
err1:
			}

			if (x != 0 && x != 0x005) { /* Once pushed to reveal the true DOS version, NTVDM.EXE responds with v5.50 */
				dos_version = (x >> 8) | (x << 8);
				dos_version_method = "INT 21h AX=3306h/NTVDM.EXE";
			}
		}
#  else
/* =================== Windows 32-bit ================== */
		DWORD raw;
		/* GetVersion() 32-bit doesn't return the DOS version at all. The upper WORD has build number instead. */
		/* Instead, use GetVersionEx() to detect system. If system is Windows 3.1 or 9x/ME we might be able
		 * to get away with abusing the DPMI server deep within Windows to get what we want. Else, if it's
		 * Windows NT, we simply assume v5.50 */

		/* assume v5.0 */
		dos_version = 0x500;
		dos_version_method = "Guessing";

		/* use the Win32 version of GetVersion() to determine what OS we're under */
		raw = GetVersion();
		if (raw & 0x80000000UL) { /* Windows 9x/ME */
			/* Start by guessing the version number based on which version of Windows we're under */
			unsigned char major = raw & 0xFF,minor = (raw >> 8) & 0xFF,ok=0;

			dos_version_method = "Guessing by Windows version";
			if (major < 4) { /* Windows 3.1 Win32s */
				dos_version = 0x616;	/* Assume MS-DOS 6.22, though it could be 6.20 or even 6.00 */
			}
			else if (major == 4) { /* Windows 95/98/ME */
				if (minor >= 90)
					dos_version = 0x800; /* Windows ME (8.00) */
				else if (minor >= 10)
					dos_version = 0x70A; /* Windows 98 (7.10) */
				else
					dos_version = 0x700; /* Windows 95 */
			}

			/* Try: Windows 9x/ME QT_Thunk hack to call down into the Win16 layer's version of GetVersion() */
			if (!ok && major == 4 && Win9xQT_ThunkInit()) {
				DWORD fptr,raw=0;

				fptr = GetProcAddress16(win9x_kernel_win16,"GETVERSION");
				if (fptr != 0) {
					dos_version_method = "Read from Win16 GetVersion() [32->16 QT_Thunk]";

					{
						__asm {
							mov	edx,fptr
							mov	eax,dword ptr [QT_Thunk]

							; QT_Thunk needs 0x40 byte of data storage at [EBP]
							; give it some, right here on the stack
							push	ebp
							mov	ebp,esp
							sub	esp,0x40

							call	eax	; <- QT_Thunk

							; release stack storage
							mov	esp,ebp
							pop	ebp

							; take Win16 response in DX:AX translate to EAX
							shl	edx,16
							and	eax,0xFFFF
							or	eax,edx
							mov	raw,eax
						}
					}

					if (raw != 0) {
						dos_version = (((raw >> 24UL) & 0xFFUL) << 8UL) | (((raw >> 16UL) & 0xFFUL) << 0UL);
						ok = 1;
					}
				}
			}
			/* Tried: Windows 3.1 with Win32s. Microsoft Win32 documentation gleefully calls Dos3Call "obsolete",
			 *        yet inspection of the Win32s DLLs shows that W32SKRNL.DLL has a _Dos3Call@0 symbol in it
			 *        that acts just like the Win16 version, calling down into DOS, and most of the Win32s DLLs
			 *        rely on it quite heavily to implement Win32 functions (the GetSystemTime function for example
			 *        using it to call INT 21h AH=2Ah).
			 *
			 *        Some old MSDN documentation I have has a list of INT 21h calls and corresponding Win32
			 *        functions to use. Again of course, they skip right over "Get MS-DOS version", no help there.
			 *
			 *        Anyway, calling this function with AX=0x3306 or AH=0x30 yields no results. Apparently, Microsoft
			 *        implemented passing through file I/O, date/time, and code page conversions, yet never considered
			 *        people might use it for something like... asking DOS it's version number. Attempting to make
			 *        these calls yields zero in AX and BX, or for AX=3306, a false return number that would imply
			 *        MS-DOS v1.0 (EAX=1). So, _Dos3Call@0 is not an option.
			 *
			 *        But then that means we have absolutely no way to determine the DOS kernel version (short of
			 *        poking our nose into segments and memory locations we have no business being in!). We can't
			 *        use _Dos3Call@0, we can't use GetVersion() because the Win32 GetVersion() doesn't return
			 *        the DOS version, and we can't use Win95 style thunks because Win32s doesn't have a publicly
			 *        available and documented way to thunk down into Win16. We have absolutely jack shit to go by.
			 *
			 *        Hey, Microsoft... When you released Win32s in 1993, did you ever stop to consider someone
			 *        might want to do something as simple as query the DOS version? Why couldn't you guys have
			 *        done something straightforward like a "GetDOSVersion()" API function that works under
			 *        Windows 9x/ME and returns an error under NT? I know it's silly of me to ask this in 2012
			 *        when Windows 8 is around the corner and Win32s are long dead, but often it seems like you
			 *        guys really don't stop to think about things like that and you make really stupid mistakes
			 *        with your APIs. */
		}
		else {
			dos_version = 0x532; /* Windows NT v5.50 */
		}
#  endif
# elif TARGET_MSDOS == 16
/* =================== Windows 16-bit ================== */
		DWORD raw = GetVersion(); /* NTS: The Win16 version does not tell us if we're running under Windows NT */

		dos_version_method = "GetVersion";
		dos_version = (((raw >> 24UL) & 0xFFUL) << 8UL) | (((raw >> 16UL) & 0xFFUL) << 0UL);

		/* Windows NT/2000/XP NTVDM.EXE lies to us, reporting Windows 95 numbers and MS-DOS 5.0 */
		if (dos_version == 0x500) {
			uint16_t x = 0;

			/* Sorry Microsoft, but you make it hard for us to detect and we have to break your OS to find the info we need */
			__asm {
				mov	ax,0x3306
				mov	bx,0
				int	21h
				jc	err1
				mov	x,bx
err1:
			}

			if (x != 0 && x != 0x005) { /* Once pushed to reveal the true DOS version, NTVDM.EXE responds with v5.50 */
				dos_version = (x >> 8) | (x << 8);
				dos_version_method = "INT 21h AX=3306h/NTVDM.EXE";
			}
		}

		/* TODO: DOS "flavor" detection */
		/* TODO: If FreeDOS, get the kernel version and allocate a selector to point at FreeDOS's revision string */
# else
#  error dunno
# endif
#elif defined(TARGET_OS2)
/* =================== OS/2 ==================== */
		dos_version = (10 << 8) | 0;
		dos_version_method = "Blunt guess";

# if TARGET_MSDOS == 32
		{
			ULONG major=0,minor=0,rev=0;
			DosQuerySysInfo(QSV_VERSION_MAJOR,QSV_VERSION_MAJOR,&major,sizeof(major));
			DosQuerySysInfo(QSV_VERSION_MINOR,QSV_VERSION_MINOR,&minor,sizeof(minor));
			DosQuerySysInfo(QSV_VERSION_REVISION,QSV_VERSION_REVISION,&rev,sizeof(rev));
			if (major != 0) {
				dos_version_method = "DosQuerySysInfo (OS/2)";
				dos_version = (major << 8) | minor;
				/* TODO: store the revision value too somewhere! */
			}
		}
# elif TARGET_MSDOS == 16
		{
			USHORT x=0;
			DosGetVersion(&x);
			if (x != 0) {
				dos_version_method = "DosGetVersion (OS/2)";
				dos_version = x;
			}
		}
# else
#  error dunno
# endif
#else
/* =================== MS-DOS ================== */
		union REGS regs;

		regs.w.ax = 0x3000;
# if TARGET_MSDOS == 32
		int386(0x21,&regs,&regs);
# else
		int86(0x21,&regs,&regs);
# endif
		dos_version = (regs.h.al << 8) | regs.h.ah;
		dos_version_method = "INT 21h AH=30h";

		if (dos_version >= 0x500 && regs.h.bh == 0xFD) {
			dos_flavor = DOS_FLAVOR_FREEDOS;
			freedos_kernel_version = (((uint32_t)regs.h.ch) << 16UL) |
				(((uint32_t)regs.h.cl) << 8UL) |
				((uint32_t)regs.h.bl);

			/* now retrieve the FreeDOS kernel string */
			/* FIXME: Does this syscall have a way to return an error or indicate that it didn't return a string? */
			regs.w.ax = 0x33FF;
# if TARGET_MSDOS == 32
			int386(0x21,&regs,&regs);
# else
			int86(0x21,&regs,&regs);
# endif

# if TARGET_MSDOS == 32
			freedos_kernel_version_str = (unsigned char*)(((uint32_t)regs.w.dx << 4UL) + (uint32_t)regs.w.ax);
# else
			freedos_kernel_version_str = MK_FP(regs.w.dx,regs.w.ax);
# endif
		}
		else if (dos_version >= 0x200 && regs.h.bh == 0xFF)
			dos_flavor = DOS_FLAVOR_MSDOS;

		/* but, SETVER can arrange for DOS to lie to us. so get the real version via
		 * undocumented subfunctions (DOS 5.0+ or later, apparently) */
		regs.w.ax = 0x3306;		/* AH=0x33 AL=0x06 */
		regs.w.bx = 0;			/* in case early DOS versions fail to set CF set BX to zero */
# if TARGET_MSDOS == 32
		int386(0x21,&regs,&regs);
# else
		int86(0x21,&regs,&regs);
# endif
		if ((regs.w.cflag & 1) == 0 && regs.h.bl >= 5 && regs.h.bl <= 8) {
			dos_version = (regs.h.bl << 8) | regs.h.bh;
			dos_version_method = "INT 21h AX=3306h";
		}
#endif
	}
}

/* TESTING (whether or not it correctly detects the presence of Windows):
 *    Note that in some columns the API returns insufficient information and the
 *    API has to use it's best guess on the correct value, which might be
 *    inaccurate or wrong (marked: GUESSES).
 *
 *    For Windows NT/2000/XP/Vista/7 tests, the code does not have any way of
 *    knowing (yet) which version of the NT kernel is involved, so the best
 *    it can offer is "I am running under NT" (marked as ONLY NT)
 *
 *    OS, shell, configuration & mode         Detects   Correct mode    Correct version
 *    Microsoft Windows 3.0 (DOSBox 0.74)
 *       386 Enhanced Mode                    YES       YES             YES
 *       286 Standard Mode                    YES       GUESSES         YES
 *       8086 Real Mode                       YES       GUESSES         YES
 *    Microsoft Windows 3.1 (DOSBox 0.74)
 *       386 Enhanced Mode                    YES       YES             YES
 *       286 Standard Mode                    YES       YES             YES
 *    Microsoft Windows 3.11 (DOSBox 0.74)
 *       386 Enhanced Mode                    YES       YES             YES*
 *       286 Standard Mode                    YES       YES             YES*
 *         * = Despite being v3.11 it still reports itself as v3.1
 *    Microsoft Windows 95 (4.00.950) (DOS 7.00) (Qemu)
 *       Normal                               YES       YES*            YES (4.0)
 *       Safe mode                            YES       YES*            YES (4.0)
 *       Prevent DOS apps detecting Windows   NO        -               -
 *         * = Reports self as "enhanced mode" which is really the only mode supported
 *    Microsoft Windows 95 OSR2.5 (4.00.950 C) (DOS 7.10) (Qemu)
 *       Normal                               YES       YES             YES (4.0)
 *       Safe mode                            YES       YES             YES (4.0)
 *    Microsoft Windows 95 SP1 (4.00.950a) (DOS 7.00) (Qemu)
 *       Normal                               YES       YES             YES (4.0)
 *       Safe mode                            YES       YES             YES (4.0)
 *    Microsoft Windows 98 (4.10.1998) (DOS 7.10) (Qemu)
 *       Normal                               YES       YES             YES (4.10)
 *       Safe mode                            YES       YES             YES (4.10)
 *    Microsoft Windows 98 SE (4.10.2222 A) (DOS 7.10) (Qemu)
 *       Normal                               YES       YES             YES (4.10)
 *       Safe mode                            YES       YES             YES (4.10)
 *    Microsoft Windows ME (4.90.3000) (DOS 8.00) (Qemu)
 *       Normal                               YES       YES             YES (4.90)
 *       Safe mode                            YES       YES             YES (4.90)
 *    Microsoft Windows 2000 Professional (5.00.2195) (VirtualBox)
 *       Normal                               YES       N/A             ONLY NT
 *    Microsoft Windows XP Professional (5.1.2600) (VirtualBox)
 *       Normal                               YES       N/A             ONLY NT
 *    Microsoft Windows XP Professional SP1 (5.1.2600) (VirtualBox)
 *       Normal                               YES       N/A             ONLY NT
 *    Microsoft Windows XP Professional SP2 (5.1.2600) (VirtualBox)
 *       Normal                               YES       N/A             ONLY NT
 *    Microsoft Windows XP Professional SP3 (5.1.2600) (VirtualBox)
 *       Normal                               YES       N/A             ONLY NT
*/

#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 32
/* it's nice to know if we're running under WINE (and therefore possibly Linux)
 * as opposed to real Windows, so that we can adjust our techniques accordingly.
 * I doubt for example that WINE would support Windows NT's NTVDM.EXE BOP codes,
 * or that our Win9x compatible VxD enumeration would know not to try enumerating
 * drivers. */
void win32_probe_for_wine() { /* Probing for WINE from the Win32 layer */
	HMODULE ntdll;

	ntdll = LoadLibrary("NTDLL.DLL");
	if (ntdll) {
		const char *(__stdcall *p)() = (const char *(__stdcall *)())GetProcAddress(ntdll,"wine_get_version");
		if (p != NULL) {
			windows_emulation = WINEMU_WINE;
			windows_emulation_comment_str = p(); /* and the function apparently returns a string */
		}
		FreeLibrary(ntdll);
	}
}
#elif defined(TARGET_WINDOWS) && TARGET_MSDOS == 16
void win16_probe_for_wine() { /* Probing for WINE from the Win16 layer */
	DWORD callw,retv;

	if (!genthunk32_init()) return;
	if (genthunk32w_ntdll == 0) return;

	callw = __GetProcAddress32W(genthunk32w_ntdll,"wine_get_version");
	if (callw == 0) return;

	retv = __CallProcEx32W(CPEX_DEST_STDCALL/*nothing to convert*/,0/*0 param*/,callw);
	if (retv == 0) return;

	windows_emulation = WINEMU_WINE;
	{
		/* NTS: We assume that WINE, just like real Windows, will not move or relocate
		 *      NTDLL.DLL and will not move the string it just returned. */
		/* TODO: You need a function the host program can call to free the selector
		 *       you allocated here, in case it wants to reclaim resources */
		uint16_t sel;
		uint16_t myds=0;
		__asm mov myds,ds
		sel = AllocSelector(myds);
		if (sel != 0) {
			/* the selector is directed at the string, then retained in this
			 * code as a direct FAR pointer to WINE's version string */
			SetSelectorBase(sel,retv);
			SetSelectorLimit(sel,0xFFF);	/* WINE's version string is rarely longer than 14 chars */
			windows_emulation_comment_str = MK_FP(sel,0);
		}
	}
}
#endif

int detect_windows() {
#if defined(TARGET_WINDOWS)
# if TARGET_MSDOS == 32
#  ifdef WIN386
	/* Windows 3.0/3.1 with Win386 */
	if (!windows_init) {
		DWORD raw;

		windows_emulation = 0;
		windows_init = 1;
		windows_mode = WINDOWS_ENHANCED; /* most likely scenario is Windows 3.1 386 enhanced mode */

		raw = GetVersion();
		windows_version_method = "GetVersion";
		windows_version = (LOBYTE(LOWORD(raw)) << 8) | HIBYTE(LOWORD(raw));
		/* NTS: Microsoft documents GetVersion() as leaving bit 31 unset for NT, bit 31 set for Win32s and Win 9x/ME.
		 *      But that's not what the Win16 version of the function does! */

		if (dos_version == 0) probe_dos();

		/* Windows 3.1/9x/ME */
		raw = GetWinFlags();
		if (raw & WF_PMODE) {
			if (raw & WF_ENHANCED)
				windows_mode = WINDOWS_ENHANCED;
			else/* if (raw & WF_STANDARD)*/
				windows_mode = WINDOWS_STANDARD;
		}
		else {
			windows_mode = WINDOWS_REAL;
		}

		/* NTS: All 32-bit Windows systems since Windows NT 3.51 and Windows 95 return
		 *      major=3 minor=95 when Win16 applications query the version number. The only
		 *      exception to that rule is Windows NT 3.1, which returns major=3 minor=10,
		 *      the same version number returned by Windows 3.1. */
		if (windows_mode == WINDOWS_ENHANCED &&
			(dos_version >= 0x510 && dos_version <= 0x57f)/* MS-DOS v5.50 */ &&
			(windows_version == 0x035F /* Windows NT 4/2000/XP/Vista/7/8 */ ||
			 windows_version == 0x030A /* Windows NT 3.1/3.5x */)) {
			/* if the real DOS version is 5.50 then we're under NT */
			windows_mode = WINDOWS_NT;
		}

		switch (dos_version>>8) {
			case 10:	/* OS/2 1.x */
			case 20:	/* OS/2 2.x (low=0), and OS/2 Warp 3 (low=30), and OS/2 Warp 4 (low=40) */
				windows_version_method = "Deduce from DOS version";
				windows_version = dos_version;
				windows_mode = WINDOWS_OS2;
				break;
		};
	}
#  elif TARGET_WINDOWS >= 40 || defined(WINNT)
	/* Windows 95/98/ME/XP/2000/NT/etc. and Windows NT builds: We don't need to do anything.
	 * The fact we're running means Windows is present */
	/* TODO: Clarify which Windows: Are we running under NT? or 9X/ME? What version? */
	if (!windows_init) {
		OSVERSIONINFO ovi;

		windows_emulation = 0;
		windows_init = 1;
		memset(&ovi,0,sizeof(ovi));
		ovi.dwOSVersionInfoSize = sizeof(ovi);
		GetVersionEx(&ovi);

		windows_version_method = "GetVersionEx";
		windows_version = (ovi.dwMajorVersion << 8) | ovi.dwMinorVersion;
		if (ovi.dwPlatformId == VER_PLATFORM_WIN32_NT)
			windows_mode = WINDOWS_NT;
		else
			windows_mode = WINDOWS_ENHANCED; /* Windows 3.1 Win32s, or Windows 95/98/ME */

		win32_probe_for_wine();
	}
#  elif TARGET_WINDOWS == 31
	/* Windows 3.1 with Win32s target build. Note that such programs run in the Win32 layer on Win95/98/ME/NT/2000/etc. */
	/* TODO: Clarify which Windows, using the limited set of functions available under Win32s, or perhaps using GetProcAddress
	 *       to use the later GetVersionEx() functions offered by Win95/98/NT/etc. */
	if (!windows_init) {
		DWORD raw;

		windows_emulation = 0;
		windows_init = 1;
		windows_mode = WINDOWS_ENHANCED; /* Assume Windows 3.1 386 Enhanced Mode. This 32-bit app couldn't run otherwise */

		raw = GetVersion();
		windows_version_method = "GetVersion";
		windows_version = (LOBYTE(LOWORD(raw)) << 8) | HIBYTE(LOWORD(raw));
		if (!(raw & 0x80000000UL)) { /* FIXME: Does this actually work? */
			/* Windows NT/2000/XP/etc */
			windows_mode = WINDOWS_NT;
		}

		/* TODO: GetProcAddress() GetVersionEx() and get the REAL version number from Windows */

		win32_probe_for_wine();
	}
#  else
#   error Unknown 32-bit Windows variant
#  endif
# elif TARGET_MSDOS == 16
#  if TARGET_WINDOWS >= 30
	/* Windows 3.0/3.1, what we then want to know is what mode we're running under: Real? Standard? Enhanced?
	 * The API function GetWinFlags() only exists in 3.0 and higher, it doesn't exist under 2.x */
	/* TODO */
	if (!windows_init) {
		DWORD raw;

		windows_emulation = 0;
		windows_init = 1;
		windows_mode = WINDOWS_ENHANCED; /* most likely scenario is Windows 3.1 386 enhanced mode */

		raw = GetVersion();
		windows_version_method = "GetVersion";
		windows_version = (LOBYTE(LOWORD(raw)) << 8) | HIBYTE(LOWORD(raw));
		/* NTS: Microsoft documents GetVersion() as leaving bit 31 unset for NT, bit 31 set for Win32s and Win 9x/ME.
		 *      But that's not what the Win16 version of the function does! */

		if (dos_version == 0) probe_dos();

		/* Windows 3.1/9x/ME */
		raw = GetWinFlags();
		if (raw & WF_PMODE) {
			if (raw & WF_ENHANCED)
				windows_mode = WINDOWS_ENHANCED;
			else/* if (raw & WF_STANDARD)*/
				windows_mode = WINDOWS_STANDARD;
		}
		else {
			windows_mode = WINDOWS_REAL;
		}

		/* NTS: All 32-bit Windows systems since Windows NT 3.51 and Windows 95 return
		 *      major=3 minor=95 when Win16 applications query the version number. The only
		 *      exception to that rule is Windows NT 3.1, which returns major=3 minor=10,
		 *      the same version number returned by Windows 3.1. */
		if (windows_mode == WINDOWS_ENHANCED &&
			(dos_version >= 0x510 && dos_version <= 0x57f)/* MS-DOS v5.50 */ &&
			(windows_version == 0x035F /* Windows NT 4/2000/XP/Vista/7/8 */ ||
			 windows_version == 0x030A /* Windows NT 3.1/3.5x */)) {
			/* if the real DOS version is 5.50 then we're under NT */
			windows_mode = WINDOWS_NT;
		}

		switch (dos_version>>8) {
			case 10:	/* OS/2 1.x */
			case 20:	/* OS/2 2.x (low=0), and OS/2 Warp 3 (low=30), and OS/2 Warp 4 (low=40) */
				windows_version_method = "Deduce from DOS version";
				windows_version = dos_version;
				windows_mode = WINDOWS_OS2;
				break;
		};

		/* If we're running under Windows 9x/ME or Windows NT/2000 we can thunk our way into
		 * the Win32 world and call various functions to get a more accurate picture of the
		 * Windows system we are running on */
		/* NTS: Under Windows NT 3.51 or later this technique is the only way to get the
		 *      true system version number. The Win16 GetVersion() will always return
		 *      some backwards compatible version number except for NT 3.1:
		 *
		 *                   Win16             Win32
		 *               +==========================
		 *      NT 3.1   |   3.1               3.1
		 *      NT 3.51  |   3.1               3.51
		 *      NT 4.0   |   3.95              4.0
		 *      2000     |   3.95              5.0
		 *      XP       |   3.95              5.1
		 *      Vista    |   3.95              6.0
		 *      7        |   3.95              6.1
		 *      8        |   3.95              6.2
		 *
		 */
		if (genthunk32_init() && genthunk32w_kernel32_GetVersionEx != 0) {
			OSVERSIONINFO osv;

			memset(&osv,0,sizeof(osv));
			osv.dwOSVersionInfoSize = sizeof(osv);
			if (__CallProcEx32W(CPEX_DEST_STDCALL | 1/* convert param 1*/,
				1/*1 param*/,genthunk32w_kernel32_GetVersionEx,
				(DWORD)((void far*)(&osv))) != 0UL) {
				windows_version_method = "GetVersionEx [16->32 CallProcEx32W]";
				windows_version = (osv.dwMajorVersion << 8) | osv.dwMinorVersion;
				if (osv.dwPlatformId == 2/*VER_PLATFORM_WIN32_NT*/)
					windows_mode = WINDOWS_NT;
				else
					windows_mode = WINDOWS_ENHANCED;
			}
		}

		win16_probe_for_wine();
	}
#  elif TARGET_WINDOWS >= 20
	/* Windows 2.x or higher. Use GetProcAddress() to locate GetWinFlags() if possible, else assume real mode
	 * and find some other way to detect if we're running under the 286 or 386 enhanced versions of Windows 2.11 */
	/* TODO */
	if (!windows_init) {
		windows_init = 1;
		windows_version = 0x200;
		windows_mode = WINDOWS_REAL;
		windows_version_method = "Assuming";
	}
#  else
	/* Windows 1.x. No GetProcAddress, no GetWinFlags. Assume Real mode. */
	/* TODO: How exactly DO you get the Windows version in 1.1? */
	if (!windows_init) {
		windows_init = 1;
		windows_version = 0x101; /* Assume 1.1 */
		windows_mode = WINDOWS_REAL;
		windows_version_method = "Assuming";
	}
#  endif
# else
#  error Unknown Windows bit target
# endif
#elif defined(TARGET_OS2)
	/* OS/2 16-bit or 32-bit. Obviously as something compiled for OS/2, we're running under OS/2 */
	if (!windows_init) {
		windows_version_method = "I'm an OS/2 program, therefore the environment is OS/2";
		windows_version = dos_version;
		windows_mode = WINDOWS_OS2;
		windows_init = 1;
	}
#else
	/* MS-DOS 16-bit or 32-bit. MS-DOS applications must try various obscure interrupts to detect whether Windows is running */
	/* TODO: How can we detect whether we're running under OS/2? */
	if (!windows_init) {
		union REGS regs;

		windows_version = 0;
		windows_mode = 0;
		windows_init = 1;

		switch (dos_version>>8) {
			case 10:	/* OS/2 1.x */
			case 20:	/* OS/2 2.x (low=0), and OS/2 Warp 3 (low=30), and OS/2 Warp 4 (low=40) */
				windows_version_method = "Deduce from DOS version";
				windows_version = dos_version;
				windows_mode = WINDOWS_OS2;
				break;
		};

		if (windows_version == 0) {
			regs.w.ax = 0x160A;
#if TARGET_MSDOS == 32
			int386(0x2F,&regs,&regs);
#else
			int86(0x2F,&regs,&regs);
#endif
			if (regs.w.ax == 0x0000 && regs.w.bx >= 0x300 && regs.w.bx <= 0x700) { /* Windows 3.1 */
				windows_version = regs.w.bx;
				switch (regs.w.cx) {
					case 0x0002: windows_mode = WINDOWS_STANDARD; break;
					case 0x0003: windows_mode = WINDOWS_ENHANCED; break;
					default:     windows_version = 0; break;
				}

				if (windows_mode != 0)
					windows_version_method = "INT 2Fh AX=160Ah";
			}
		}

		if (windows_version == 0) {
			regs.w.ax = 0x4680;
#if TARGET_MSDOS == 32
			int386(0x2F,&regs,&regs);
#else
			int86(0x2F,&regs,&regs);
#endif
			if (regs.w.ax == 0x0000) { /* Windows 3.0 or DOSSHELL in real or standard mode */
			/* FIXME: Okay... if DOSSHELL triggers this test how do I tell between Windows and DOSSHELL? */
			/* Also, this call does NOT work when Windows 3.0 is in enhanced mode, and for Real and Standard modes
			 * does not tell us which mode is active.
			 *
			 * As far as I can tell there really is no way to differentiate whether it is running in Real or
			 * Standard mode, because on a 286 there is no "virtual 8086" mode. The only way Windows can run
			 * DOS level code is to thunk back into real mode. So for all purposes whatsoever, we might as well
			 * say that we're running in Windows Real mode because during that time slice we have complete control
			 * of the CPU. */
				windows_version = 0x300;
				windows_mode = WINDOWS_REAL;
				windows_version_method = "INT 2Fh AX=4680h";
			}
		}

		if (windows_version == 0) {
			regs.w.ax = 0x1600;
#if TARGET_MSDOS == 32
			int386(0x2F,&regs,&regs);
#else
			int86(0x2F,&regs,&regs);
#endif
			if (regs.h.al == 1 || regs.h.al == 0xFF) { /* Windows 2.x/386 */
				windows_version = 0x200;
				windows_mode = WINDOWS_ENHANCED;
			}
			else if (regs.h.al == 3 || regs.h.al == 4) {
				windows_version = (regs.h.al << 8) + regs.h.ah;
				windows_mode = WINDOWS_ENHANCED; /* Windows 3.0 */
			}

			if (windows_mode != 0)
				windows_version_method = "INT 2Fh AX=1600h";
		}

		if (windows_version == 0 && windows_mode == WINDOWS_NONE) {
		/* well... if the above fails, but the "true" DOS version is 5.50, then we're running under Windows NT */
		/* NOTE: Every copy of NT/2000/XP/Vista I have reports 5.50, but assuming that will continue is stupid.
		 *       Microsoft is free to change that someday. */
			if (dos_version == 0) probe_dos();
			if (dos_version >= 0x510 && dos_version <= 0x57f) { /* FIXME: If I recall Windows NT really does stick to v5.50, so this range check should be changed into == 5.50 */
				windows_mode = WINDOWS_NT;
				windows_version = 0;
				windows_version_method = "Assuming from DOS version number";
			}
		}

		/* now... if this is Windows NT, the next thing we can do is use NTVDM.EXE's
		 * BOP opcodes to load a "helper" DLL that allows us to call into Win32 */
# if defined(NTVDM_CLIENT) && !defined(TARGET_WINDOWS)
		if (windows_mode == WINDOWS_NT && ntvdm_dosntast_init()) {
			/* OK. Ask for the version number */
			OSVERSIONINFO ovi;

			memset(&ovi,0,sizeof(ovi));
			ovi.dwOSVersionInfoSize = sizeof(ovi);
			if (ntvdm_dosntast_getversionex(&ovi)) {
				windows_version_method = "GetVersionEx [NTVDM.EXE + DOSNTAST.VDD]";
				windows_version = (ovi.dwMajorVersion << 8) | ovi.dwMinorVersion;
				if (ovi.dwPlatformId == 2/*VER_PLATFORM_WIN32_NT*/)
					windows_mode = WINDOWS_NT;
				else
					windows_mode = WINDOWS_ENHANCED;
			}
		}
# endif
	}
#endif

	return (windows_mode != WINDOWS_NONE);
}

/* MS-DOS "list of lists" secret call */
#if TARGET_MSDOS == 32
# ifdef WIN386
unsigned char *dos_list_of_lists() {
	return NULL;/*not implemented*/
}
# else
static void dos_realmode_call(struct dpmi_realmode_call *rc) {
	__asm {
		mov	ax,0x0300
		mov	bx,0x0021
		xor	cx,cx
		mov	edi,rc		; we trust Watcom has left ES == DS
		int	0x31		; call DPMI
	}
}

unsigned char *dos_list_of_lists() {
	struct dpmi_realmode_call rc={0};

	rc.eax = 0x5200;
	dos_realmode_call(&rc);
	if (rc.flags & 1) return NULL; /* CF */
	return (dos_LOL = ((unsigned char*)((rc.es << 4) + (rc.ebx & 0xFFFFUL))));
}
# endif
#else
unsigned char far *dos_list_of_lists() {
	unsigned int s=0,o=0;

	__asm {
		mov	ah,0x52
		int	21h
		jc	notwork
		mov	s,es
		mov	o,bx
notwork:
	}

	return (dos_LOL = ((unsigned char far*)MK_FP(s,o)));
}
#endif

unsigned char FAR *dos_mcb_get_psp(struct dos_mcb_enum *e) {
	if (e->psp < 0x80 || e->psp == 0xFFFFU)
		return NULL;

#if TARGET_MSDOS == 32
	return (unsigned char*)((uint32_t)e->psp << 4UL);
#else
	return (unsigned char FAR*)MK_FP(e->psp,0);
#endif
}

int mcb_name_is_junk(char *s/*8 char*/) {
	int junk=0,i;
	unsigned char c;

	for (i=0;i < 8;i++) {
		c = (unsigned char)s[i];
		if (c == 0)
			break;
		else if (c < 32 || c >= 127)
			junk = 1;
	}

	return junk;
}

uint16_t dos_mcb_first_segment() {
	if (dos_LOL == NULL)
		return 0;

	return *((uint16_t FAR*)(dos_LOL-2)); /* NTS: This is not a mistake. You take the pointer given by DOS and refer to the WORD starting 2 bytes PRIOR. I don't know why they did that... */
}

void mcb_filter_name(struct dos_mcb_enum *e) {
	if (e->psp > 0 && e->psp < 0x80) { /* likely special DOS segment */
		if (!memcmp(e->name,"SC",2) || !memcmp(e->name,"SD",2))
			memset(e->name+2,0,6);
		else
			memset(e->name,0,8);
	}
	else if (mcb_name_is_junk(e->name)) {
		memset(e->name,0,8);
	}
}

int dos_mcb_next(struct dos_mcb_enum *e) {
	unsigned char FAR *mcb;
	unsigned int i;
	uint16_t nxt;

	if (e->type == 0x5A || e->segment == 0x0000U || e->segment == 0xFFFFU)
		return 0;
	if (e->counter >= 16384)
		return 0;

#if TARGET_MSDOS == 32
	mcb = (unsigned char*)((uint32_t)(e->segment) << 4UL);
	e->ptr = mcb + 16;
#else
	mcb = (unsigned char far*)(MK_FP(e->segment,0));
	e->ptr = (unsigned char far*)(MK_FP(e->segment+1U,0));
#endif

	e->cur_segment = e->segment;
	e->type = *((uint8_t FAR*)(mcb+0));
	e->psp = *((uint16_t FAR*)(mcb+1));
	e->size = *((uint16_t FAR*)(mcb+3));
	for (i=0;i < 8;i++) e->name[i] = mcb[i+8]; e->name[8] = 0;
	if (e->type != 0x5A && e->type != 0x4D) return 0;
	nxt = e->segment + e->size + 1;
	if (nxt <= e->segment) return 0;
	e->segment = nxt;
	return 1;
}

int dos_mcb_first(struct dos_mcb_enum *e) {
	if (dos_LOL == NULL)
		return 0;

	e->counter = 0;
	e->segment = dos_mcb_first_segment();
	if (e->segment == 0x0000U || e->segment == 0xFFFFU)
		return 0;

	return dos_mcb_next(e);
}

int dos_parse_psp(uint16_t seg,struct dos_psp_cooked *e) {
	unsigned int i,o=0;

#if TARGET_MSDOS == 32
	e->raw = (unsigned char*)((uint32_t)seg << 4UL);
#else
	e->raw = (unsigned char FAR*)MK_FP(seg,0);
#endif
	e->memsize = *((uint16_t FAR*)(e->raw + 0x02));
	e->callpsp = *((uint16_t FAR*)(e->raw + 0x16));
	e->env = *((uint16_t FAR*)(e->raw + 0x2C));
	for (i=0;i < (unsigned char)e->raw[0x80] && e->raw[0x81+i] == ' ';) i++; /* why is there all this whitespace at the start? */
	for (;i < (unsigned char)e->raw[0x80];i++) e->cmd[o++] = e->raw[0x81+i];
	e->cmd[o] = 0;
	return 1;
}

int dos_device_next(struct dos_device_enum *e) {
	e->raw = e->next;
	if (e->raw == NULL || e->count >= 512 || e->no == 0xFFFFU)
		return 0;

	e->no = *((uint16_t FAR*)(e->raw + 0x0));
	e->ns = *((uint16_t FAR*)(e->raw + 0x2));
	e->attr = *((uint16_t FAR*)(e->raw + 0x04));
	e->entry = *((uint16_t FAR*)(e->raw + 0x06));
	e->intent = *((uint16_t FAR*)(e->raw + 0x08));
	if (!(e->attr & 0x8000)) {
		/* block device */
		_fmemcpy(e->name,e->raw+0x0B,8); e->name[7] = 0;
		if (e->name[0] < 33 || e->name[0] >= 127) e->name[0] = 0;
	}
	else {
		/* char device */
		_fmemcpy(e->name,e->raw+0x0A,8); e->name[8] = 0;
	}
	e->count++;

#if TARGET_MSDOS == 32
	e->next = (unsigned char*)((((uint32_t)(e->ns)) << 4UL) + e->no);
#else
	e->next = (unsigned char FAR*)MK_FP(e->ns,e->no);
#endif

	return 1;
}

int dos_device_first(struct dos_device_enum *e) {
	unsigned int offset = 0x22; /* most common, DOS 3.1 and later */

	if (dos_LOL == NULL)
		return 0;

	if (dos_version < 0x200) /* We don't know where the first device is in DOS 1.x */
		return 0;
	else if (dos_version < 0x300)
		offset = 0x17;
	else if (dos_version == 0x300)
		offset = 0x28;

	e->no = 0;
	e->ns = 0;
	e->count = 0;
	e->raw = e->next = dos_LOL + offset;
	if (_fmemcmp(e->raw+0xA,"NUL     ",8) != 0) /* should be the NUL device driver */
		return 0;

	return dos_device_next(e);
}

/* NTS: According to the VCPI specification this call is only supposed to report
 *      the physical memory address for anything below the 1MB boundary. And so
 *      far EMM386.EXE strictly adheres to that rule by not reporting success for
 *      addresses >= 1MB. The 32-bit limitation is a result of the VCPI system
 *      call, since the physical address is returned in EDX. */
uint32_t dos_linear_to_phys_vcpi(uint32_t pn) {
	uint32_t r=0xFFFFFFFFUL;

	__asm {
		.586p
		mov	ax,0xDE06
		mov	ecx,pn
		int	67h
		or	ah,ah
		jnz	err1		; if AH == 0 then EDX = page phys addr
		mov	r,edx
err1:
	}

	return r;
}

/* TODO: Since VCPI/EMM386.EXE can affect 16-bit real mode, why not enable this API for 16-bit real mode too? */
/* TODO: Also why not enable this function for 16-bit protected mode under Windows 3.1? */
#if TARGET_MSDOS == 32
struct dos_linear_to_phys_info dos_ltp_info;
unsigned char dos_ltp_info_init=0;

/* WARNING: Caller must have called probe_dos() and detect_windows() */
int dos_ltp_probe() {
	if (!dos_ltp_info_init) {
		memset(&dos_ltp_info,0,sizeof(dos_ltp_info));

		/* part of our hackery needs to know what CPU we're running under */
		if (cpu_basic_level < 0)
			cpu_probe();

		probe_dos();

#if defined(TARGET_WINDOWS)
		/* TODO: Careful analsys of what version and mode Windows we're running under */
		/* start with the assumption that we don't know where we are and we can't translate to physical. */
		dos_ltp_info.vcpi_xlate = 0; /* TODO: It is said Windows 3.0 has VCPI at the core. Can we detect that? Use it? */
		dos_ltp_info.paging = (windows_mode <= WINDOWS_STANDARD ? 0 : 1); /* paging is not used in REAL or STANDARD modes */
		dos_ltp_info.dos_remap = dos_ltp_info.paging;
		dos_ltp_info.should_lock_pages = 1;
		dos_ltp_info.cant_xlate = 1;
		dos_ltp_info.using_pae = 0; /* TODO: Windows XP SP2 and later can and do use PAE. How to detect that? */
		dos_ltp_info.dma_dos_xlate = 0;

# if TARGET_MSDOS == 32
# else
		/* TODO: Use GetWinFlags() and version info */
# endif
#else
/* ================ MS-DOS specific =============== */
		/* we need to know if VCPI is present */
		probe_vcpi();

		/* NTS: Microsoft Windows 3.0/3.1 Enhanced mode and Windows 95/98/ME all trap access to the control registers.
		 *      But then they emulate the instruction in such a way that we get weird nonsense values.
		 *
		 *      Windows 95/98/ME: We get CR0 = 2. Why??
		 *      Windows 3.0/3.1: We get CR0 = 0.
		 *
		 *      So basically what Windows is telling us... is that we're 32-bit protected mode code NOT running in
		 *      protected mode? What? */
		if (windows_mode == WINDOWS_ENHANCED) {
			/* it's pointless, the VM will trap and return nonsense for control register contents */
			dos_ltp_info.cr0 = 0x80000001UL;
			dos_ltp_info.cr3 = 0x00000000UL;
			dos_ltp_info.cr4 = 0x00000000UL;
		}
		else if (windows_mode == WINDOWS_NT) {
			/* Windows NTVDM will let us read CR0, but CR3 and CR4 come up blank. So what's the point then? */
			uint32_t r0=0;

			__asm {
				xor	eax,eax
				dec	eax

				mov	eax,cr0
				mov	r0,eax
			}
			dos_ltp_info.cr0 = r0 | 0x80000001UL; /* paging and protected mode are ALWAYS enabled, even if NTVDM should lie to us */
			dos_ltp_info.cr3 = 0x00000000UL;
			dos_ltp_info.cr4 = 0x00000000UL;
		}
		else {
			uint32_t r0=0,r3=0,r4=0;
			__asm {
				xor	eax,eax
				dec	eax

				mov	eax,cr0
				mov	r0,eax

				mov	eax,cr3
				mov	r3,eax

				mov	eax,cr4
				mov	r4,eax
			}
			dos_ltp_info.cr0 = r0;
			dos_ltp_info.cr3 = r3;
			dos_ltp_info.cr4 = r4;
		}

		dos_ltp_info.vcpi_xlate = vcpi_present?1:0;			/* if no other methods available, try asking the VCPI server */
		dos_ltp_info.paging = (dos_ltp_info.cr0 >> 31)?1:0;		/* if bit 31 of CR0 is set, the extender has paging enabled */
		dos_ltp_info.dos_remap = vcpi_present?1:0;			/* most DOS extenders map 1:1 the lower 1MB, but VCPI can violate that */
		dos_ltp_info.should_lock_pages = dos_ltp_info.paging;		/* it's a good assumption if paging is enabled the extender probably pages to disk and may move things around */
		dos_ltp_info.cant_xlate = dos_ltp_info.paging;			/* assume we can't translate addresses yet if paging is enabled */
		dos_ltp_info.using_pae = (dos_ltp_info.cr4 & 0x20)?1:0;		/* take note if PAE is enabled */
		dos_ltp_info.dma_dos_xlate = dos_ltp_info.paging;		/* assume the extender translates DMA if paging is enabled */

		if (windows_mode == WINDOWS_ENHANCED || windows_mode == WINDOWS_NT) {
			dos_ltp_info.should_lock_pages = 1;		/* Windows is known to page to disk (the swapfile) */
			dos_ltp_info.dma_dos_xlate = 1;			/* Windows virtualizes the DMA controller */
			dos_ltp_info.cant_xlate = 1;			/* Windows provides us no way to determine the physical memory address from linear */
			dos_ltp_info.dos_remap = 1;			/* Windows remaps the DOS memory area. This is how it makes multiple DOS VMs possible */
			dos_ltp_info.vcpi_xlate = 0;			/* Windows does not like VCPI */
			dos_ltp_info.paging = 1;			/* Windows uses paging. Always */
		}

		/* this code is ill prepared for PAE modes for the time being, since PAE makes page table entries 64-bit
		 * wide instead of 32-bit wide. Then again, 99% of DOS out there probably couldn't handle PAE well either. */
		if (dos_ltp_info.using_pae)
			dos_ltp_info.cant_xlate = 1;

		/* if NOT running under Windows, and the CR3 register shows something, then we can translate by directly peeking at the page tables */
		if (windows_mode == WINDOWS_NONE && dos_ltp_info.cr3 >= 0x1000)
			dos_ltp_info.cant_xlate = 0;
#endif

		dos_ltp_info_init = 1;
	}

	return 1;
}

/* WARNINGS: Worst case scanario this function cannot translate anything at all.
 *           It will return 0xFFFFFFFFUL if it cannot determine the address.
 *           If paging is disabled, the linear address will be returned.
 *           If the environment requires you to lock pages, then you must do so
 *           before calling this function. Failure to do so will mean erratic
 *           behavior when the page you were working on moves out from under you! */

/* "There is no way in a DPMI environment to determine the physical address corresponding to a given linear address. This is part of the design of DPMI. You must design your application accordingly."
 *
 * Fuck you.
 * I need the damn physical address and you're not gonna stop me! */

/* NOTES:
 *
 *     QEMU + Windows 95 + EMM386.EXE:
 *
 *        I don't know if the DOS extender is doing this, or EMM386.EXE is enforcing it, but
 *        a dump of the first 4MB in the test program reveals our linear address space is
 *        randomly constructed from 16KB pages taken from all over extended memory. Some of
 *        them, the pages are arranged BACKWARDS. Yeah, backwards.
 *
 *        Anyone behind DOS4/GW and DOS32a care to explain that weirdness?
 *
 *        Also revealed, DOS4/GW follows the DPMI spec and refuses to map pages from conventional
 *        memory. So if DOS memory is not mapped 1:1 and the page tables are held down there we
 *        literally cannot read them! */
/* NOTE: The return value is 64-bit so that in the future, if we ever run under DOS with PAE or
 *       PSE-36 trickery, we can still report the correct address even if above 4GB. The parameter
 *       supplied however remains 32-bit, because as a 32-bit DOS program there's no way any
 *       linear address will ever exceed 4GB. */
uint64_t dos_linear_to_phys(uint32_t linear) {
	uint32_t off,ent;
	uint64_t ret = DOS_LTP_FAILED;
	unsigned char lock1=0,lock2=0;
	uint32_t *l1=NULL,*l2=NULL;

	if (!dos_ltp_info.paging)
		return linear;
	if (linear < 0x100000UL && !dos_ltp_info.dos_remap) /* if below 1MB boundary and DOS is not remapped, then no translation */
		return linear;

	/* if VCPI translation is to be used, and lower DOS memory is remapped OR the page requested is >= 1MB (or in adapter ROM/RAM), then call the VCPI server and ask */
	if (dos_ltp_info.vcpi_xlate && (dos_ltp_info.dos_remap || linear >= 0xC0000UL)) {
		ent = dos_linear_to_phys_vcpi(linear>>12);
		if (ent != 0xFFFFFFFFUL) return ent;
		/* Most likely requests for memory >= 1MB will fail, because VCPI is only required to
		 * provide the system call for lower 1MB DOS conventional memory */
	}

/* if we can't use VCPI and cannot translate, then give up. */
/* also, this code does not yet support PAE */
	if (dos_ltp_info.using_pae || dos_ltp_info.cant_xlate)
		return ret;

/* re-read control reg because EMM386, etc. is free to change it at any time */
	{
		uint32_t r3=0,r4=0;
		__asm {
			xor	eax,eax
			dec	eax

			mov	eax,cr3
			mov	r3,eax

			mov	eax,cr4
			mov	r4,eax
		}
		dos_ltp_info.cr3 = r3;
		dos_ltp_info.cr4 = r4;
	}

	/* OK then, we have to translate */
	off = linear & 0xFFFUL;
	linear >>= 12UL;
	if (dos_ltp_info.cr3 < 0x1000) /* if the contents of CR3 are not available, then we cannot translate */
		return ret;

	/* VCPI possibility: the page table might reside below 1MB in DOS memory, and remain unmapped. */
	if (dos_ltp_info.dos_remap && dos_ltp_info.vcpi_xlate && dos_ltp_info.cr3 < 0x100000UL) {
		lock1 = 0;
		if (dos_linear_to_phys_vcpi(dos_ltp_info.cr3>>12) == (dos_ltp_info.cr3&0xFFFFF000UL)) /* if VCPI says it's a 1:1 mapping down there, then it's OK */
			l1 = (uint32_t*)(dos_ltp_info.cr3 & 0xFFFFF000UL);
	}
	/* DOS4/GW and DOS32A Goodie: the first level of the page table is in conventional memory... and the extender maps DOS 1:1 :) */
	else if (!dos_ltp_info.dos_remap && dos_ltp_info.cr3 < 0x100000UL) {
		lock1 = 0;
		l1 = (uint32_t*)(dos_ltp_info.cr3 & 0xFFFFF000UL);
	}
	else {
		/* well, then we gotta map it */
		l1 = (uint32_t*)dpmi_phys_addr_map(dos_ltp_info.cr3 & 0xFFFFF000UL,4096);
		if (l1 != NULL) lock1 = 1;
	}

	if (l1 != NULL) {
		/* level 1 lookup */
		ent = l1[linear >> 10UL];
		if (ent & 1) { /* if the page is actually present... */
			/* if the CPU supports PSE (Page Size Extensions) and has them enabled (via CR4) and the page is marked PS=1 */
			if ((cpu_cpuid_features.a.raw[2/*EDX*/] & (1 << 3)) && (dos_ltp_info.cr4 & 0x10) && (ent & 0x80)) {
				/* it's a 4MB page, and we stop searching the page hierarchy here */
				ret = ent & 0xFFC00000UL; /* bits 31-22 are the actual page address */

				/* but wait: if the CPU supports PSE-36, then we need to readback address bits 35...32,
				 * or if an AMD64 processor, address bits 39...32 */
				/* FIXME: So, we can assume if the CPU supports 64-bit long mode, that we can use bits 39...32?
				 *        Perhaps this is a more in-depth check that we should be doing in the ltp_probe function? */
				if (cpu_cpuid_features.a.raw[2/*E2X*/] & (1 << 17)) { /* If PSE support exists */
					if (cpu_cpuid_features.a.raw[3/*ECX*/] & (1 << 29)) { /* If CPU supports 64-bit long mode */
						/* AMD64 compatible, up to 40 bits */
						ret |= ((uint64_t)((ent >> 13UL) & 0xFFUL)) << 32ULL;
					}
					else { /* else, assume Pentium III compatible, up to 36 bits */
						ret |= ((uint64_t)((ent >> 13UL) & 0xFUL)) << 32ULL;
					}
				}
			}
			else {
				/* VCPI possibility: the page table might reside below 1MB in DOS memory, and remain unmapped. */
				if (dos_ltp_info.dos_remap && dos_ltp_info.vcpi_xlate && ent < 0x100000UL) {
					lock2 = 0;
					if (dos_linear_to_phys_vcpi(ent>>12) == (ent&0xFFFFF000UL)) /* if VCPI says it's a 1:1 mapping down there, then it's OK */
						l2 = (uint32_t*)(ent & 0xFFFFF000UL);
				}
				/* again the second level is usually in DOS memory where we can assume 1:1 mapping */
				else if (!dos_ltp_info.dos_remap && !dos_ltp_info.dos_remap && ent < 0x100000UL) {
					lock2 = 0;
					l2 = (uint32_t*)(ent & 0xFFFFF000UL);
				}
				else {
					/* well, then we gotta map it */
					l2 = (uint32_t*)dpmi_phys_addr_map(ent & 0xFFFFF000UL,4096);
					if (l2 != NULL) lock2 = 1;
				}
			}
		}
	}

	if (l2 != NULL) {
		/* level 2 lookup */
		ent = l2[linear & 0x3FF];
		if (ent & 1) { /* if the page is actually present... */
			ret = ent & 0xFFFFF000UL;
		}
	}

	if (lock2) dpmi_phys_addr_free((void*)l2);
	if (lock1) dpmi_phys_addr_free((void*)l1);
	return ret;
}
#endif

#if TARGET_MSDOS == 32
void *dpmi_linear_alloc(uint32_t try_lin,uint32_t size,uint32_t flags,uint32_t *handle) {
	void *retv = 0;
	uint32_t han = 0;

	__asm {
		mov	ax,0x0504
		mov	ebx,try_lin
		mov	ecx,size
		mov	edx,flags
		int	0x31
		jc	endf
		mov	retv,ebx
		mov	han,esi
endf:
	}

	if (retv != NULL && handle != NULL)
		*handle = han;

	return retv;
}

int dpmi_linear_free(uint32_t handle) {
	int retv = 0;

	__asm {
		mov	ax,0x0502
		mov	di,word ptr handle
		mov	si,word ptr handle+2
		int	0x31
		jc	endf
		mov	retv,1
endf:
	}

	return retv;
}
#endif

#if !defined(TARGET_WINDOWS)
static int int67_null() {
	uint32_t ptr;

#if TARGET_MSDOS == 32
	ptr = ((uint32_t*)0)[0x67];
#else
	ptr = *((uint32_t far*)MK_FP(0,0x67*4));;
#endif

	return (ptr == 0);
}
#endif

int probe_vcpi() {
#if defined(TARGET_WINDOWS)
	if (!vcpi_probed) {
		/* NTS: Whoever said Windows 3.0 used VCPI at it's core, is a liar */
		vcpi_probed = 1;
		vcpi_present = 0;
	}
#else
/* =================== MSDOS ================== */
	unsigned char err=0xFF;

	if (!vcpi_probed) {
		vcpi_probed = 1;

		/* if virtual 8086 mode isn't active, then VCPI isn't there */
		/* FIXME: What about cases where VCPI might be there, but is inactive (such as: EMM386.EXE resident but disabled) */
		if (!cpu_v86_active)
			return 0;

		/* NOTE: VCPI can be present whether we're 16-bit real mode or
		 * 32-bit protected mode. Unlike DPMI we cannot assume it's
		 * present just because we're 32-bit. */

		/* Do not call INT 67h if the vector is uninitialized */
		if (int67_null())
			return 0;

		/* Do not attempt to probe for VCPI if Windows 3.1/95/98/ME
		 * is running. Windows 9x blocks VCPI and if called, interrupts
		 * our execution to inform the user that the program should be
		 * run in DOS mode. */
		detect_windows();
		if (windows_mode != WINDOWS_NONE)
			return 0;

		/* NTS: we load DS for each var because Watcom might put it in
		 *      another data segment, especially in Large memory model.
		 *      failure to account for this was the cause of mysterious
		 *      hangs and crashes. */
		__asm {
			; NTS: Save DS and ES because Microsoft EMM386.EXE
			;      appears to change their contents.
			push	ds
			push	es
			mov	ax,0xDE00
			int	67h
			mov	err,ah

			mov	ax,seg vcpi_major_version
			mov	ds,ax
			mov	vcpi_major_version,bh

			mov	ax,seg vcpi_minor_version
			mov	ds,ax
			mov	vcpi_minor_version,bl

			pop	es
			pop	ds
		}

		if (err != 0)
			return 0;

		vcpi_present = 1;
	}
#endif

	return vcpi_present;
}

#if !defined(TARGET_WINDOWS) && TARGET_MSDOS == 32

/* NTS: This only allows for exception interrupts 0x00-0x1F */
void far *dpmi_getexhandler(unsigned char n) {
	unsigned short s=0;
	unsigned int o=0;

	__asm {
		mov	ax,0x202
		mov	bl,n
		xor	cx,cx
		xor	dx,dx
		int	31h
		mov	s,cx
		mov	o,edx
	}

	return MK_FP(s,o);
}

/* NTS: This only allows for exception interrupts 0x00-0x1F */
int dpmi_setexhandler(unsigned char n,void far *x) {
	unsigned short s=FP_SEG(x);
	unsigned int o=FP_OFF(x);
	int c=1;

	__asm {
		mov	ax,0x203
		mov	bl,n
		mov	cx,s
		mov	edx,o
		int	31h
		jnc	ok
		mov	c,0
ok:
	}

	return c;
}

#endif

#if !defined(TARGET_WINDOWS) && TARGET_MSDOS == 16

/* NTS: This only allows for exception interrupts 0x00-0x1F */
void far *dpmi_getexhandler(unsigned char n) {
	unsigned short s=0,o=0;

	__asm {
		mov	ax,0x202
		mov	bl,n
		xor	cx,cx
		xor	dx,dx
		int	31h
		mov	s,cx
		mov	o,dx
	}

	return MK_FP(s,o);
}

/* NTS: This only allows for exception interrupts 0x00-0x1F */
int dpmi_setexhandler(unsigned char n,void far *x) {
	unsigned short s=FP_SEG(x),o=FP_OFF(x);
	int c=1;

	__asm {
		mov	ax,0x203
		mov	bl,n
		mov	cx,s
		mov	dx,o
		int	31h
		jnc	ok
		mov	c,0
ok:
	}

	return c;
}

#endif

/* Watcom C does not provide getvect/setvect for Win16, so we abuse the DPMI server within and provide one anyway */
#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 16

/* NTS: This only allows for exception interrupts 0x00-0x1F */
void far *win16_getexhandler(unsigned char n) {
	unsigned short s=0,o=0;

	__asm {
		mov	ax,0x202
		mov	bl,n
		xor	cx,cx
		xor	dx,dx
		int	31h
		mov	s,cx
		mov	o,dx
	}

	return MK_FP(s,o);
}

/* NTS: This only allows for exception interrupts 0x00-0x1F */
int win16_setexhandler(unsigned char n,void far *x) {
	unsigned short s=FP_SEG(x),o=FP_OFF(x);
	int c=1;

	__asm {
		mov	ax,0x203
		mov	bl,n
		mov	cx,s
		mov	dx,o
		int	31h
		jnc	ok
		mov	c,0
ok:
	}

	return c;
}

void far *win16_getvect(unsigned char n) {
	unsigned short s=0,o=0;

	__asm {
		mov	ax,0x204
		mov	bl,n
		xor	cx,cx
		xor	dx,dx
		int	31h
		mov	s,cx
		mov	o,dx
	}

	return MK_FP(s,o);
}

/* NTS: This only allows for exception interrupts 0x00-0x1F */
int win16_setvect(unsigned char n,void far *x) {
	unsigned short s=FP_SEG(x),o=FP_OFF(x);
	int c=1;

	__asm {
		mov	ax,0x205
		mov	bl,n
		mov	cx,s
		mov	dx,o
		int	31h
		jnc	ok
		mov	c,0
ok:
	}

	return c;
}

#endif

#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 32 && !defined(WIN386)

/* this library of code deals with the problem of getting ordinal-only exported functions out of KERNEL32.DLL in
 * Windows 9x/ME. GetProcAddress() won't do it for us, so instead, we forcibly read it out from memory ourself.
 * That's what you get for being a jack-ass about Win16 compatibility Microsoft */

/* Note that this code would work under Windows 9x/ME and NT/2000/XP, but it is only needed for 9x/ME.
 * Windows XP SP2 and later change the handle slightly to try and confuse code like this (they take the HANDLE
 * value of the module and set the least significant bit), and I have reason to believe Microsoft will eventually
 * outright change the interface to make the handle an actual opaque handle someday (while of course making it
 * utterly impossible for programs like us to get to the API functions we need to do our fucking job). Because they're
 * Microsoft, and that's what they do with the Windows API. */

/* How to use: Use the 32-bit GetModuleHandle() function to get the HMODULE value. In Windows so far, this HMODULE
 * value is literally the linear memory address where Windows loaded (or mapped) the base of the DLL image, complete
 * with MS-DOS header and PE image. This hack relies on that to then traverse the PE structure directly and forcibly
 * retrieve from the ordinal export table the function we desire. */

/* returns: DWORD* pointer to PE image's export ordinal table, *entries is filled with the number of entries, *base
 * is filled with the ordinal number of the first entry. */

static IMAGE_NT_HEADERS *Win32ValidateHModuleMSDOS_PE_Header(BYTE *p) {
	if (!memcmp(p,"MZ",2)) {
		/* then at offset 0x3C there should be the offset to the PE header */
		DWORD offset = *((DWORD*)(p+0x3C));
		if (offset < 0x40 || offset > 0x10000) return NULL;
		p += offset;
		if (IsBadReadPtr(p,4096)) return NULL;
		if (!memcmp(p,"PE\0\0",4)) {
			/* wait, before we celebrate, make sure it's sane! */
			IMAGE_NT_HEADERS *pp = (IMAGE_NT_HEADERS*)p;

			if (pp->FileHeader.Machine != IMAGE_FILE_MACHINE_I386)
				return NULL;
			if (pp->FileHeader.SizeOfOptionalHeader < 88) /* <- FIXME */
				return NULL;
			if (pp->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC)
				return NULL;

			return pp;
		}
	}

	return NULL;
}

static IMAGE_DATA_DIRECTORY *Win32GetDataDirectory(IMAGE_NT_HEADERS *p) {
	return p->OptionalHeader.DataDirectory;
}

DWORD *Win32GetExportOrdinalTable(HMODULE mod,DWORD *entries,DWORD *base,DWORD *base_addr) {
	IMAGE_EXPORT_DIRECTORY *exdir;
	IMAGE_DATA_DIRECTORY *dir;
	IMAGE_NT_HEADERS *ptr;

	/* Hack for Windows XP SP2: Clear the LSB, the OS sets it for some reason */
	mod = (HMODULE)((DWORD)mod & 0xFFFFF000UL);
	/* reset vars */
	*entries = *base = 0;
	if (mod == NULL) return NULL;

	/* the module pointer should point an image of the DLL in memory. Right at the pointer we should see
	 * the letters "MZ" and the MS-DOS stub EXE header */
	ptr = Win32ValidateHModuleMSDOS_PE_Header((BYTE*)mod);
	if (ptr == NULL) return NULL;

	/* OK, now locate the Data Directory. The number of entries is in ptr->OptionalHeader.NumberOfRvaAndSizes */
	dir = Win32GetDataDirectory(ptr);
	if (ptr == NULL) return NULL;

	/* the first directory is the Export Address Table */
	exdir = (IMAGE_EXPORT_DIRECTORY*)((DWORD)mod + (DWORD)dir->VirtualAddress);
	if (IsBadReadPtr(exdir,2048)) return NULL;

	*base = exdir->Base;
	*entries = exdir->NumberOfFunctions;
	*base_addr = (DWORD)mod;
	return (DWORD*)((DWORD)mod + exdir->AddressOfFunctions);
}

int Win32GetOrdinalLookupInfo(HMODULE mod,Win32OrdinalLookupInfo *info) {
	DWORD *x = Win32GetExportOrdinalTable(mod,&info->entries,&info->base,&info->base_addr);
	if (x == NULL) return 0;
	info->table = x;
	return 1;
}

void *Win32GetOrdinalAddress(Win32OrdinalLookupInfo *nfo,unsigned int ord) {
	if (nfo == NULL || nfo->table == NULL) return NULL;
	if (ord < nfo->base) return NULL;
	if (ord >= (nfo->base+nfo->entries)) return NULL;
	return (void*)((char*)nfo->table[ord-nfo->base] + nfo->base_addr);
}
#endif

#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 16

/* if provided by the system, these functions allow library and application code to call out to the Win32 world from Win16.
 * Which is absolutely necessary given that Win16 APIs tend to lie for compatibility reasons. */

DWORD		genthunk32w_ntdll = 0;
DWORD		genthunk32w_kernel32 = 0;
DWORD		genthunk32w_kernel32_GetVersion = 0;
DWORD		genthunk32w_kernel32_GetVersionEx = 0;
DWORD		genthunk32w_kernel32_GetLastError = 0;
BOOL		__GenThunksExist = 0;
BOOL		__GenThunksChecked = 0;
DWORD		(PASCAL FAR *__LoadLibraryEx32W)(LPCSTR lpName,DWORD reservedhfile,DWORD dwFlags) = NULL;
BOOL		(PASCAL FAR *__FreeLibrary32W)(DWORD hinst) = NULL;
DWORD		(PASCAL FAR *__GetProcAddress32W)(DWORD hinst,LPCSTR name) = NULL;
DWORD		(PASCAL FAR *__GetVDMPointer32W)(LPVOID ptr,UINT mask) = NULL;
DWORD		(PASCAL FAR *__CallProc32W)(DWORD procaddr32,DWORD convertMask,DWORD params,...) = NULL; /* <- FIXME: How to use? */
DWORD		(_cdecl _far *__CallProcEx32W)(DWORD params,DWORD convertMask,DWORD procaddr32,...) = NULL;

int genthunk32_init() {
	if (!__GenThunksChecked) {
		HMODULE kern32;

		genthunk32_free();
		__GenThunksExist = 0;
		__GenThunksChecked = 1;
		kern32 = GetModuleHandle("KERNEL");
		if (kern32 != NULL) {
			__LoadLibraryEx32W = (void far*)GetProcAddress(kern32,"LOADLIBRARYEX32W");
			__FreeLibrary32W = (void far*)GetProcAddress(kern32,"FREELIBRARY32W");
			__GetProcAddress32W = (void far*)GetProcAddress(kern32,"GETPROCADDRESS32W");
			__GetVDMPointer32W = (void far*)GetProcAddress(kern32,"GETVDMPOINTER32W");
			__CallProcEx32W = (void far*)GetProcAddress(kern32,"_CALLPROCEX32W"); /* <- Hey thanks Microsoft
												    maybe if your docs mentioned
												    the goddamn underscore I would
												    have an easier time linking to it */
			__CallProc32W = (void far*)GetProcAddress(kern32,"CALLPROC32W");

			if (__LoadLibraryEx32W && __FreeLibrary32W && __GetProcAddress32W && __GetVDMPointer32W &&
				__CallProc32W && __CallProcEx32W) {
				__GenThunksExist = 1;

				genthunk32w_kernel32 = __LoadLibraryEx32W("KERNEL32.DLL",0,0);
				if (genthunk32w_kernel32 != 0) {
					genthunk32w_kernel32_GetVersion =
						__GetProcAddress32W(genthunk32w_kernel32,"GetVersion");
					genthunk32w_kernel32_GetVersionEx =
						__GetProcAddress32W(genthunk32w_kernel32,"GetVersionExA");
					genthunk32w_kernel32_GetLastError =
						__GetProcAddress32W(genthunk32w_kernel32,"GetLastError");
				}

				genthunk32w_ntdll = __LoadLibraryEx32W("NTDLL.DLL",0,0);
			}
		}
	}

	return __GenThunksExist;
}

void genthunk32_free() {
	genthunk32w_kernel32_GetVersion = 0;
	genthunk32w_kernel32_GetVersionEx = 0;
	genthunk32w_kernel32_GetLastError = 0;
	if (genthunk32w_kernel32) {
		__FreeLibrary32W(genthunk32w_kernel32);
		genthunk32w_kernel32 = 0;
	}
}

#endif

#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 32 && !defined(WIN386)
/* API for exploiting the QT_Thunk Win32 -> Win16 thunking offered by Windows 9x/ME */
unsigned char		win9x_qt_thunk_probed = 0;
unsigned char		win9x_qt_thunk_available = 0;

void			(__stdcall *QT_Thunk)() = NULL;
DWORD			(__stdcall *LoadLibrary16)(LPSTR lpszLibFileName) = NULL;
VOID			(__stdcall *FreeLibrary16)(DWORD dwInstance) = NULL;
HGLOBAL16		(__stdcall *GlobalAlloc16)(UINT flags,DWORD size) = NULL;
HGLOBAL16		(__stdcall *GlobalFree16)(HGLOBAL16 handle) = NULL;
DWORD			(__stdcall *GlobalLock16)(HGLOBAL16 handle) = NULL;
BOOL			(__stdcall *GlobalUnlock16)(HGLOBAL16 handle) = NULL;
VOID			(__stdcall *GlobalUnfix16)(HGLOBAL16 handle) = NULL;
VOID			(__stdcall *GlobalFix16)(HGLOBAL16 handle) = NULL;
DWORD			(__stdcall *GetProcAddress16)(DWORD dwInstance, LPSTR lpszProcName) = NULL;
DWORD			win9x_kernel_win16 = 0;
DWORD			win9x_user_win16 = 0;

int Win9xQT_ThunkInit() {
	if (!win9x_qt_thunk_probed) {
		Win32OrdinalLookupInfo nfo;
		HMODULE kern32;

		win9x_qt_thunk_probed = 1;
		win9x_qt_thunk_available = 0;

		if (dos_version == 0) probe_dos();
		if (windows_mode == 0) detect_windows();
		if (windows_mode != WINDOWS_ENHANCED) return 0; /* This does not work under Windows NT */
		if (windows_version < 0x400) return 0; /* This does not work on Windows 3.1 Win32s (FIXME: Are you sure?) */

		/* This hack relies on undocumented Win16 support routines hidden in KERNEL32.DLL.
		 * They're so super seekret, Microsoft won't even let us get to them through GetProcAddress() */
		kern32 = GetModuleHandle("KERNEL32.DLL");
		if (windows_emulation == WINEMU_WINE) {
			/* FIXME: Direct ordinal lookup doesn't work. Returned
			 *        addresses point to invalid regions of KERNEL32.DLL.
			 *        I doubt WINE is even putting a PE-compatible image
			 *        of it out there.
			 *
			 *        WINE does allow us to GetProcAddress ordinals
			 *        (unlike Windows 9x which blocks it) but I'm not
			 *        really sure the returned functions are anything
			 *        like the Windows 9x equivalent. If we assume they
			 *        are, this code seems unable to get the address of
			 *        KRNL386.EXE's "GETVERSION" function.
			 *
			 *        So basically WINE's Windows 9x emulation is more
			 *        like Windows XP's Application Compatability modes
			 *        than any serious attempt at pretending to be
			 *        Windows 9x. And the entry points may well be
			 *        stubs or other random functions in the same way
			 *        that ordinal 35 is unrelated under Windows XP. */
			return 0;
		}
		else if (Win32GetOrdinalLookupInfo(kern32,&nfo)) {
			GlobalFix16 = (void*)Win32GetOrdinalAddress(&nfo,27);
			GlobalLock16 = (void*)Win32GetOrdinalAddress(&nfo,25);
			GlobalFree16 = (void*)Win32GetOrdinalAddress(&nfo,31);
			LoadLibrary16 = (void*)Win32GetOrdinalAddress(&nfo,35);
			FreeLibrary16 = (void*)Win32GetOrdinalAddress(&nfo,36);
			GlobalAlloc16 = (void*)Win32GetOrdinalAddress(&nfo,24);
			GlobalUnfix16 = (void*)Win32GetOrdinalAddress(&nfo,28);
			GlobalUnlock16 = (void*)Win32GetOrdinalAddress(&nfo,26);
			GetProcAddress16 = (void*)Win32GetOrdinalAddress(&nfo,37);
			QT_Thunk = (void*)GetProcAddress(kern32,"QT_Thunk");
		}
		else {
			GlobalFix16 = NULL;
			GlobalLock16 = NULL;
			GlobalFree16 = NULL;
			GlobalUnfix16 = NULL;
			LoadLibrary16 = NULL;
			FreeLibrary16 = NULL;
			GlobalAlloc16 = NULL;
			GlobalUnlock16 = NULL;
			GetProcAddress16 = NULL;
			QT_Thunk = NULL;
		}

		if (LoadLibrary16 && FreeLibrary16 && GetProcAddress16 && QT_Thunk) {
			/* Prove the API works by loading a reference to KERNEL */
			win9x_kernel_win16 = LoadLibrary16("KERNEL");
			if (win9x_kernel_win16 != 0) {
				win9x_qt_thunk_available = 1;
			}

			win9x_user_win16 = LoadLibrary16("USER");
		}
	}

	return win9x_qt_thunk_available;
}

void Win9xQT_ThunkFree() {
	if (win9x_kernel_win16) {
		FreeLibrary16(win9x_kernel_win16);
		win9x_kernel_win16 = 0;
	}
}
#endif

#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 16
unsigned char	ToolHelpProbed = 0;
HMODULE		ToolHelpDLL = 0;
BOOL		(PASCAL FAR *__TimerCount)(TIMERINFO FAR *t) = NULL;
BOOL		(PASCAL FAR *__InterruptUnRegister)(HTASK htask) = NULL;
BOOL		(PASCAL FAR *__InterruptRegister)(HTASK htask,FARPROC callback) = NULL;

int ToolHelpInit() {
	if (!ToolHelpProbed) {
		UINT oldMode;

		ToolHelpProbed = 1;

		/* BUGFIX: In case TOOLHELP.DLL is missing (such as when running under Windows 3.0)
		 *         this prevents sudden interruption by a "Cannot find TOOLHELP.DLL" error
		 *         dialog box */
		oldMode = SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
		ToolHelpDLL = LoadLibrary("TOOLHELP.DLL");
		SetErrorMode(oldMode);
		if (ToolHelpDLL != 0) {
			__TimerCount = (void far*)GetProcAddress(ToolHelpDLL,"TIMERCOUNT");
			__InterruptRegister = (void far*)GetProcAddress(ToolHelpDLL,"INTERRUPTREGISTER");
			__InterruptUnRegister = (void far*)GetProcAddress(ToolHelpDLL,"INTERRUPTUNREGISTER");
		}
	}

	return (ToolHelpDLL != 0) && (__TimerCount != NULL) && (__InterruptRegister != NULL) &&
		(__InterruptUnRegister != NULL);
}

void ToolHelpFree() {
	if (ToolHelpDLL) {
		FreeLibrary(ToolHelpDLL);
		ToolHelpDLL = 0;
	}
	__InterruptUnRegister = NULL;
	__InterruptRegister = NULL;
	__TimerCount = NULL;
}
#endif

#if TARGET_MSDOS == 16 && !defined(TARGET_WINDOWS)
int dpmi_private_alloc() {
	unsigned short sss=0;

	if (!dpmi_present || dpmi_private_data_segment != 0xFFFFU)
		return 1; /* OK, nothing to do */

	if (dpmi_private_data_length_paragraphs == 0) {
		dpmi_private_data_segment = 0;
		return 0;
	}

	__asm {
		push	ds
		mov	ah,0x48
		mov	bx,seg dpmi_private_data_length_paragraphs
		mov	ds,bx
		mov	bx,dpmi_private_data_length_paragraphs
		int	21h
		pop	ds
		jc	fail1
		mov	sss,ax
fail1:
	}

	if (sss == 0)
		return 0;

	dpmi_private_data_segment = sss;
	return 1;
}

/* NTS: This enters DPMI. There is no exit from DPMI. And if you re-enter DPMI by switching back to protected mode,
 *      you only serve to confuse the server somewhat.
 *
 *         Re-entry results:
 *           - Windows XP: Allows it, even going 16 to 32 bit mode, but the console window gets confused and drops our
 *             output when changing bit size.
 *           - Windows 9x: Allows it, doesn't allow changing bit mode, so once in 16-bit mode you cannot enter 32-bit mode.
 *             The mode persists until the DOS Box exits.
 *
 *      This also means that once you init in one mode, you cannot re-enter another mode. If you init in 16-bit DPMI,
 *      you cannot init into 32-bit DPMI.
 *
 *      If all you want is the best mode, call with mode == 0xFF instead to automatically select. */
int dpmi_enter(unsigned char mode) {
/* TODO: Cache results, only need to scan once */
	if (mode == 0xFF) {
		if ((cpu_basic_level == -1 || cpu_basic_level >= 3) && (dpmi_flags&1) == 1)
			mode = 32; /* if 32-bit capable DPMI server and 386 or higher, choose 32-bit */
		else
			mode = 16; /* for all else, choose 16-bit */
	}

	if (dpmi_entered != 0) {
		if (dpmi_entered != mode) return 0;
		return 1;
	}

	if (mode != 16 && mode != 32)
		return 0;
	if (mode == 32 && !(dpmi_flags & 1))
		return 0;
	if (dpmi_entry_point == 0)
		return 0;
	if (!dpmi_private_alloc())
		return 0;
	if (dpmi_private_data_length_paragraphs != 0 && dpmi_private_data_segment == 0)
		return 0;
	if (dpmi_private_data_segment == 0xFFFFU)
		return 0;

	dpmi_entered = mode;
	dpmi_enter_core();
	return (dpmi_entered != 0); /* NTS: dpmi_enter_core() will set dpmi_entered back to zero on failure */
}
#endif

#if !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
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

/* returns interrupt vector */
/* these functions are duplicates of the ones in the ULTRASND library
   because it matters to this library whether or not we're talking to
   Gravis Ultrasound and shitty SB emulation */
int gravis_sbos_detect() {
    unsigned char FAR *ex;
    uint16_t s,o;
    int i = 0x78;

    while (i < 0x90) {
#if TARGET_MSDOS == 32
	o = *((uint16_t*)(i*4U));
	s = *((uint16_t*)((i*4U)+2U));
#else
	o = *((uint16_t far*)MK_FP(0,(uint16_t)i*4U));
	s = *((uint16_t far*)MK_FP(0,((uint16_t)i*4U)+2U));
#endif

	if (o == 0xFFFF || s == 0x0000 || s == 0xFFFF) {
	    i++;
	    continue;
	}

	/* we're looking for "SBOS" signature */
#if TARGET_MSDOS == 32
	ex = (unsigned char*)((s << 4UL) + 0xA);
	if (memcmp(ex,"SBOS",4) == 0) return i;
#else
	ex = MK_FP(s,0xA);
	if (_fmemcmp(ex,"SBOS",4) == 0) return i;
#endif

	i++;
    }

    return -1;
}

/* returns interrupt vector */
int gravis_ultramid_detect() {
    unsigned char FAR *ex;
    uint16_t s,o;
    int i = 0x78;

    while (i < 0x90) {
#if TARGET_MSDOS == 32
	o = *((uint16_t*)(i*4U));
	s = *((uint16_t*)((i*4U)+2U));
#else
	o = *((uint16_t far*)MK_FP(0,(uint16_t)i*4U));
	s = *((uint16_t far*)MK_FP(0,((uint16_t)i*4U)+2U));
#endif

	if (o == 0xFFFF || s == 0x0000 || s == 0xFFFF) {
	    i++;
	    continue;
	}

	/* we're looking for "ULTRAMID" signature */
#if TARGET_MSDOS == 32
	ex = (unsigned char*)((s << 4UL) + 0x103);
	if (memcmp(ex,"ULTRAMID",8) == 0) return i;
#else
	ex = MK_FP(s,0x103);
	if (_fmemcmp(ex,"ULTRAMID",8) == 0) return i;
#endif

	i++;
    }

    return -1;
}
#endif /* !defined(TARGET_WINDOWS) */

#if TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
/* TODO: This should be moved into the hw/DOS library */
unsigned char			nmi_32_hooked = 0;
int				nmi_32_refcount = 0;
void				(interrupt *nmi_32_old_vec)() = NULL;
#endif

#if TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
/* NMI reflection (32-bit -> 16-bit)
   This code is VITAL if we want to work with SBOS and MEGA-EM
   from protected mode. */
static struct dpmi_realmode_call nmi_32_nr={0};
static void interrupt far nmi_32() {
	/* trigger a real-mode INT 02h */
	__asm {
		mov	ax,0x0300
		mov	bx,0x02
		xor	cx,cx
		mov	edi,offset nmi_32_nr	; we trust Watcom has left ES == DS
		int	0x31			; call DPMI
	}
}

void do_nmi_32_unhook() {
	if (nmi_32_refcount > 0)
		nmi_32_refcount--;

	if (nmi_32_refcount == 0) {
		if (nmi_32_hooked) {
			nmi_32_hooked = 0;
			_dos_setvect(0x02,nmi_32_old_vec);
			nmi_32_old_vec = NULL;
		}
	}
}

void do_nmi_32_hook() {
	if (nmi_32_refcount == 0) {
		if (!nmi_32_hooked) {
			nmi_32_hooked = 1;
			nmi_32_old_vec = _dos_getvect(0x02);
			_dos_setvect(0x02,nmi_32);
		}
	}
	nmi_32_refcount++;
}
#endif

#if defined(TARGET_MSDOS) && !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
/* Windows 9x/NT Close-awareness */
void dos_close_awareness_cancel() {
	__asm {
		.386p
		mov	ax,0x168F
		mov	dx,0x0300
		int	0x2F
	}
}

void dos_close_awareness_ack() {
	__asm {
		.386p
		mov	ax,0x168F
		mov	dx,0x0200
		int	0x2F
	}
}

int dos_close_awareness_enable(unsigned char en) {
	uint16_t r=0;

	en = (en != 0) ? 1 : 0;

	__asm {
		.386p
		mov	ax,0x168F
		xor	dx,dx
		mov	dl,en
		int	0x2F
		mov	r,ax
	}

	return (int)r;
}

int dos_close_awareness_query() {
	uint16_t r=0;

	__asm {
		.386p
		mov	ax,0x168F
		mov	dx,0x0100
		int	0x2F
		mov	r,ax
	}

	if (r == 0x168F)
		return -1;

	return (int)r;
}

int dos_close_awareness_available() {
	/* "close-awareness" is provided by Windows */
	return (windows_mode == WINDOWS_ENHANCED || windows_mode == WINDOWS_NT);
}

void dos_vm_yield() {
	__asm {
		mov	ax,0x1680	/* RELEASE VM TIME SLICE */
			xor	bx,bx		/* THIS VM */
			int	0x2F
	}
}
#endif

#if defined(TARGET_WINDOWS) || defined(TARGET_OS2)
#else
/* as seen in the ROM area: "BIOS: "+NUL+"VirtualBox 4.1.8_OSE" around 0xF000:0x0130 */
static const char *virtualbox_bios_str = "BIOS: ";
static const char *virtualbox_vb_str = "VirtualBox ";
#endif

/* the ROM area doesn't change. so remember our last result */
#if defined(TARGET_WINDOWS) || defined(TARGET_OS2)
#else
static signed char virtualbox_detect_cache = -1;
#endif

/* unlike DOSBox, VirtualBox's ROM BIOS contains it's version number, which we copy down here */
char virtualbox_version_str[64]={0};

int detect_virtualbox_emu() {
#if defined(TARGET_OS2)
	/* TODO: So... how does one read the ROM BIOS area from OS/2? */
	return 0;
#elif defined(TARGET_WINDOWS)
	/* TODO: I know that from within Windows there are various ways to scan the ROM BIOS area.
	 *       Windows 1.x and 2.x (if real mode) we can just use MK_FP as we do under DOS, but
	 *       we have to use alternate means if Windows is in protected mode. Windows 2.x/3.x protected
	 *       mode (and because of compatibility, Windows 95/98/ME), there are two methods open to us:
	 *       One is to use selector constants that are hidden away in KRNL386.EXE with names like _A0000,
	 *       _B0000, etc. They are data selectors that when loaded into the segment registers point to
	 *       their respective parts of DOS adapter ROM and BIOS ROM. Another way is to use the Win16
	 *       API to create a data selector that points to the BIOS. Windows 386 Enhanced mode may map
	 *       additional things over the unused parts of adapter ROM, but experience shows that it never
	 *       relocates or messes with the VGA BIOS or with the ROM BIOS,
	 *
	 *       My memory is foggy at this point, but I remember that under Windows XP SP2, one of the
	 *       above Win16 methods still worked even from under the NT kernel.
	 *
	 *       For Win32 applications, if the host OS is Windows 3.1 Win32s or Windows 95/98/ME, we can
	 *       take advantage of a strange quirk in the way the kernel maps the lower 1MB. For whatever
	 *       reason, the VGA RAM, adapter ROM, and ROM BIOS areas are left open even for Win32 applications
	 *       with no protection. Thus, a Win32 programmer can just make a pointer like
	 *       char *a = (char*)0xA0000 and scribble on legacy VGA RAM to his heart's content (though on
	 *       modern PCI SVGA hardware the driver probably instructs the card to disable VGA compatible
	 *       mapping). In fact, this ability to scribble on RAM directly is at the heart of one of Microsoft's
	 *       earliest and hackiest "Direct Draw" interfaces known as "DISPDIB.DLL", a not-to-well documented
	 *       library responsible for those Windows 3.1 multimedia apps and games that were somehow able to
	 *       run full-screen 320x200x256 color VGA despite being Windows GDI-based apps. Ever wonder how the
	 *       MCI AVI player was able to go full-screen when DirectX and WinG were not invented yet? Now you
	 *       know :)
	 *
	 *       There are some VFW codecs out there as well, that also like to abuse DISPDIB.DLL for "fullscreen"
	 *       modes. One good example is the old "Motion Pixels" codec, that when asked to go fullscreen,
	 *       uses DISPDIB.DLL and direct VGA I/O port trickery to effectively set up a 320x480 256-color mode,
	 *       which it then draws on "fake hicolor" style to display the video (though a bit dim since you're
	 *       sort of watching a video through a dithered mesh, but...)
	 *       
	 *       In case you were probably wondering, no, Windows NT doesn't allow Win32 applications the same
	 *       privilege. Win32 apps writing to 0xA0000 would page fault and crash. Curiously enough though,
	 *       NTVDM.EXE does seem to open up the 0xA0000-0xFFFFF memory area to Win16 applications if they
	 *       use the right selectors and API calls. */
	return 0;
#else
	int i,j;
# if TARGET_MSDOS == 32
	const char *scan;
# else
	const char far *scan;
# endif

	probe_dos();
	if (virtualbox_detect_cache >= 0)
		return (int)virtualbox_detect_cache;

	virtualbox_detect_cache=0;

# if TARGET_MSDOS == 32
	if (dos_flavor == DOS_FLAVOR_FREEDOS) {
		/* FIXME: I have no idea why but FreeDOS 1.0 has a strange conflict with DOS32a where if this code
		 *        tries to read the ROM BIOS it causes a GPF and crashes (or sometimes runs off into the
		 *        weeds leaving a little garbage on the screen). DOS32a's register dump seems to imply that
		 *        at one point our segment limits were suddenly limited to 1MB (0xFFFFF). I have no clue
		 *        what the hell is triggering it, but I know from testing we can avoid that crash by not
		 *        scanning. */
		if (freedos_kernel_version == 0x000024UL) /* 0.0.36 */
			return (virtualbox_detect_cache=0);
	}
# endif

	/* test #1: the ROM BIOS region just prior to 0xF0000 is all zeros.
	 * NTS: VirtualBox 4.1.8 also seems to have the ACPI tables at 0xE000:0x0000 */
# if TARGET_MSDOS == 32
	scan = (const char*)0xEFF00;
# else
	scan = (const char far*)MK_FP(0xE000,0xFF00);
# endif
	for (i=0;i < 256;i++) {
		if (scan[i] != 0)
			return virtualbox_detect_cache;
	}

	/* test #2: somewhere within the first 4KB, are the strings "BIOS: " and
	 * "VirtualBox " side by side separated by a NUL. The "VirtualBox" string is
	 * followed by the version number, and "_OSE" if the open source version. */
# if TARGET_MSDOS == 32
	scan = (const char*)0xF0000;
# else
	scan = (const char far*)MK_FP(0xF000,0x0000);
# endif
	for (i=0,j=0;i < 4096;i++) {
		if (scan[i] == virtualbox_bios_str[j]) {
			j++;
			if (virtualbox_bios_str[j] == 0 && scan[i+1] == 0) {
				/* good. found it. stop there. */
				i++;
				break;
			}
		}
		else {
			j=0;
		}
	}
	/* if we didn't find the first string, then give up */
	if (i >= 4096) return virtualbox_detect_cache;

	/* make sure the next string is "VirtualBox " */
	for (/*do not reset 'i'*/j=0;i < 4096;i++) {
		if (scan[i] == virtualbox_vb_str[j]) {
			j++;
			if (virtualbox_vb_str[j] == 0) {
				/* good. found it. stop there. */
				virtualbox_detect_cache = 1;
				break;
			}
		}
		else {
			j=0;
		}
	}
	if (i >= 4096) return virtualbox_detect_cache;

	/* 'i' now points at the version part of the string. copy it down */
	while (i < 4096 && scan[i] == ' ') i++;
	for (j=0;j < (sizeof(virtualbox_version_str)-1) && i < 4096;)
		virtualbox_version_str[j++] = scan[i++];
	virtualbox_version_str[j] = 0;

	return virtualbox_detect_cache;
#endif
}

#if TARGET_MSDOS == 16 && !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
/* TODO: Switch into DPMI protected mode, allocate and setup selectors, do memcpy to
 *       DOS realmode segment, then return to real mode. When the code is stable,
 *       move it into hw/dos/dos.c. The purpose of this code is to allow 16-bit realmode
 *       DOS programs to reach into DPMI linear address space, such as the Win 9x
 *       kernel area when run under the Win 9x DPMI server. */
int dpmi_lin2fmemcpy(unsigned char far *dst,uint32_t lsrc,uint32_t sz) {
	if (dpmi_entered == 32)
		return dpmi_lin2fmemcpy_32(dst,lsrc,sz);
	else if (dpmi_entered == 16) {
		_fmemset(dst,0,sz);
		return dpmi_lin2fmemcpy_16(dst,lsrc,sz);
	}

	return 0;
}

int dpmi_lin2fmemcpy_init() {
	probe_dpmi();
	if (!dpmi_present)
		return 0;
	if (!dpmi_enter(DPMI_ENTER_AUTO))
		return 0;

	return 1;
}
#endif

const char *windows_emulation_str(uint8_t e) {
	switch (e) {
		case WINEMU_NONE:	return "None";
		case WINEMU_WINE:	return "WINE (Wine Is Not an Emulator)";
		default:		break;
	}

	return "?";
}

#if defined(NTVDM_CLIENT) && !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
uint16_t ntvdm_dosntast_io_base = 0;

uint16_t ntvdm_dosntast_detect() {
	const char *str = "DOSNTAST.VDD";
	uint16_t str_len = 12;
	uint16_t handle = (~0U);
	unsigned int i,max=0x200;
#if TARGET_MSDOS == 32
	unsigned char *p = (unsigned char*)0x400;
#else
	unsigned char FAR *p = (unsigned char FAR*)MK_FP(0x40,0x00);
#endif

	for (i=0;i <= (max-str_len);i++) {
#if TARGET_MSDOS == 32
		if (memcmp(str,p+i,str_len) == 0)
			handle = *((uint16_t*)(p+i+str_len));
#else
		if (_fmemcmp(str,p+i,str_len) == 0)
			handle = *((uint16_t FAR*)(p+i+str_len));
#endif

		if (ntvdm_RM_ERR(handle))
			handle = DOSNTAST_HANDLE_UNASSIGNED;
		else
			break;
	}

	return handle;
}

int ntvdm_dosntast_load_vdd() {
	uint32_t t1=0,t2=0;

	/* TODO: Right now this works for the current path, or if it's in the Windows system dir.
	 *       Adopt a strategy where the user can also set an environment variable to say where
	 *       the DLL is. */
	ntvdm_dosntast_handle = ntvdm_RegisterModule("DOSNTAST.VDD","Init","Dispatch");
	if (ntvdm_RM_ERR(ntvdm_dosntast_handle)) return 0;

	/* test out the dispatch call: give the DLL back his handle */
#if TARGET_MSDOS == 32
	{
		struct dpmi_realmode_call rc={0};
		rc.ebx = DOSNTAST_INIT_REPORT_HANDLE_C;
		rc.ecx = ntvdm_dosntast_handle;
		ntvdm_DispatchCall_dpmi(ntvdm_dosntast_handle,&rc);
		t1 = rc.ebx;
		t2 = rc.ecx;
	}
#else
	t1 = ntvdm_dosntast_handle;
	__asm {
		.386p
		push	ebx
		push	ecx
		mov	ebx,DOSNTAST_INIT_REPORT_HANDLE
		mov	eax,t1
		mov	ecx,eax
		ntvdm_Dispatch_ins_asm_db
		mov	t1,ebx
		mov	t2,ecx
		pop	ecx
		pop	ebx
	}
#endif

	if (t1 != 0x55AA55AA || !(t2 >= 0x400 && t2 <= 0x600)) {
		ntvdm_UnregisterModule(ntvdm_dosntast_handle);
		return 0;
	}
#if TARGET_MSDOS == 32
	if (memcmp((void*)t2,"DOSNTAST.VDD\xFF\xFF",14)) {
		ntvdm_UnregisterModule(ntvdm_dosntast_handle);
		return 0;
	}
	*((uint16_t*)(t2+12)) = ntvdm_dosntast_handle;
#else
	if (_fmemcmp(MK_FP(t2>>4,t2&0xF),"DOSNTAST.VDD\xFF\xFF",14)) {
		ntvdm_UnregisterModule(ntvdm_dosntast_handle);
		return 0;
	}
	*((uint16_t FAR*)MK_FP((t2+12)>>4,(t2+12)&0xF)) = ntvdm_dosntast_handle;
#endif

	return (ntvdm_dosntast_handle != DOSNTAST_HANDLE_UNASSIGNED)?1:0;
}

unsigned int ntvdm_dosntast_waveOutGetNumDevs() {
	if (ntvdm_dosntast_io_base == 0)
		return 0;

	/* FUNCTION */
	outpw(ntvdm_dosntast_io_base+0,DOSNTAST_FUNCTION_WINMM);
	/* SUBFUNCTION */
	outpw(ntvdm_dosntast_io_base+1,DOSNTAST_FUN_WINMM_SUB_waveOutGetNumDevs);
	/* COMMAND */
	return inpw(ntvdm_dosntast_io_base+2);
}

uint32_t ntvdm_dosntast_waveOutGetDevCaps(uint32_t uDeviceID,WAVEOUTCAPS *pwoc,uint16_t cbwoc) {
	uint32_t retv=0,port;

	if (ntvdm_dosntast_io_base == 0)
		return 0;

	/* FUNCTION */
	outpw(ntvdm_dosntast_io_base+0,DOSNTAST_FUNCTION_WINMM);
	/* SUBFUNCTION */
	outpw(ntvdm_dosntast_io_base+1,DOSNTAST_FUN_WINMM_SUB_waveOutGetDevCaps);
	/* COMMAND */
	port = ntvdm_dosntast_io_base+2;

#if TARGET_MSDOS == 32
	__asm {
		.386p
		pushad
		/* we trust Watcom left ES == DS */
		mov		edx,port
		mov		eax,uDeviceID
		movzx		ebx,cbwoc
		mov		ecx,1
		mov		edi,pwoc
		rep		insb
		popad
	}
#elif defined(__LARGE__) || defined(__COMPACT__)
	__asm {
		.386p
		push		es
		pushad
		mov		edx,port
		mov		eax,uDeviceID
		movzx		ebx,cbwoc
		mov		ecx,1
		les		di,pwoc
		rep		insb
		popad
		pop		es
	}
#else
	__asm {
		.386p
		pushad
		push		es
		mov		ax,ds
		mov		es,ax
		mov		edx,port
		mov		eax,uDeviceID
		movzx		ebx,cbwoc
		mov		ecx,1
		mov		di,pwoc
		rep		insb
		pop		es
		popad
	}
#endif

	return retv;
}

int ntvdm_dosntast_MessageBox(const char *text) {
	uint16_t port;

	if (ntvdm_dosntast_io_base == 0)
		return 0;

	/* FUNCTION */
	outpw(ntvdm_dosntast_io_base+0,DOSNTAST_FUNCTION_GENERAL);
	/* SUBFUNCTION */
	outpw(ntvdm_dosntast_io_base+1,DOSNTAST_FUN_GEN_SUB_MESSAGEBOX);
	/* COMMAND */
	port = ntvdm_dosntast_io_base+2;
#if TARGET_MSDOS == 32
	__asm {
		.386p
		push	esi
		push	ecx
		push	edx
		cld
		movzx	edx,port
		mov	esi,text
		mov	ecx,1		/* NTS: duration dosn't matter, our DLL passes DS:SI directly to MessageBoxA */
		rep	outsb
		pop	edx
		pop	ecx
		pop	esi
	}
#elif defined(__LARGE__) || defined(__COMPACT__)
	__asm {
		.386p
		push	ds
		push	si
		push	cx
		push	dx
		cld
		mov	dx,port
		lds	si,text
		mov	cx,1
		rep	outsb
		pop	dx
		pop	cx
		pop	si
		pop	ds
	}
#else
	__asm {
		.386p
		push	si
		push	cx
		push	dx
		cld
		mov	dx,port
		mov	si,text
		mov	cx,1
		rep	outsb
		pop	dx
		pop	cx
		pop	si
	}
#endif

	return 1;
}

/* initialize the library.
 * if dont_load_dosntast is set, then it will not load the VDD driver but will use the driver if already loaded */
int ntvdm_dosntast_init() {
	uint32_t t1=0,t2=0;

	if (!ntvdm_dosntast_tried) {
		ntvdm_dosntast_tried = 1;
		ntvdm_dosntast_io_base = 0;

		if (lib_dos_option.dont_use_dosntast) {
			ntvdm_dosntast_handle = DOSNTAST_HANDLE_UNASSIGNED;
			return 0;
		}

		/* It turns out if you request the same DLL again and again, NTVDM.EXE will not return the
		 * same handle, it will allocate another one. To avoid exhausting it handles, we first
		 * detect whether the DLL is already loaded.
		 *
		 * We do this by scanning the 0x40-0x50 segments (the BIOS data area) for a signature value
		 * placed by the DLL. Following the signature is the handle value. */
		ntvdm_dosntast_handle = ntvdm_dosntast_detect();
		if (ntvdm_dosntast_handle == DOSNTAST_HANDLE_UNASSIGNED && !lib_dos_option.dont_load_dosntast)
			ntvdm_dosntast_load_vdd();

		/* we need to know the IO port base */
		if (ntvdm_dosntast_handle != DOSNTAST_HANDLE_UNASSIGNED) {
			if (!ntvdm_rm_code_alloc())
				return ntvdm_RM_ERR_NOT_AVAILABLE;

#if TARGET_MSDOS == 32
			{
				struct dpmi_realmode_call rc={0};
				rc.ebx = (uint32_t)(DOSNTAST_GET_IO_PORT_C); /* <= FIXME: "constant out of range" what the fuck are you talking about Watcom? */
				ntvdm_DispatchCall_dpmi(ntvdm_dosntast_handle,&rc);
				t1 = rc.ebx;
				t2 = rc.edx;
			}
#else
			t1 = ntvdm_dosntast_handle;
			__asm {
				.386p
				push	ebx
				push	edx
				mov	ebx,DOSNTAST_GET_IO_PORT
				mov	eax,t1
				ntvdm_Dispatch_ins_asm_db
				mov	t1,ebx
				mov	t2,edx
				pop	edx
				pop	ebx
			}
#endif

			if (t1 == 0x55AA55AAUL)
				ntvdm_dosntast_io_base = (uint16_t)t2;
		}
	}

	return (ntvdm_dosntast_handle != DOSNTAST_HANDLE_UNASSIGNED)?1:0;
}

void ntvdm_dosntast_unload() {
	if (ntvdm_dosntast_handle != DOSNTAST_HANDLE_UNASSIGNED) {
#if TARGET_MSDOS == 32
		{
			struct dpmi_realmode_call rc={0};
			rc.ebx = DOSNTAST_NOTIFY_UNLOAD_C;
			ntvdm_DispatchCall_dpmi(ntvdm_dosntast_handle,&rc);
		}
#else
		{
			const uint16_t h = ntvdm_dosntast_handle;

			__asm {
				.386p
				mov	ebx,DOSNTAST_NOTIFY_UNLOAD
				mov	ax,h
				ntvdm_Dispatch_ins_asm_db
			}
		}
#endif

		ntvdm_UnregisterModule(ntvdm_dosntast_handle);
		ntvdm_dosntast_handle = DOSNTAST_HANDLE_UNASSIGNED;
	}
}

uint32_t ntvdm_dosntast_GetTickCount() {
	uint32_t retv = 0xFFFFFFFFUL;

	if (ntvdm_dosntast_handle == DOSNTAST_HANDLE_UNASSIGNED)
		return 0; /* failed */
	if (!ntvdm_rm_code_alloc())
		return 0;

#if TARGET_MSDOS == 32
	{
		struct dpmi_realmode_call rc={0};
		rc.ebx = DOSNTAST_GET_TICK_COUNT_C;
		ntvdm_DispatchCall_dpmi(ntvdm_dosntast_handle,&rc);
		retv = rc.ebx;
	}
#else
	{
		const uint16_t h = ntvdm_dosntast_handle;

		__asm {
			.386p
			push	ebx
			mov	ebx,DOSNTAST_GET_TICK_COUNT
			mov	ax,h
			ntvdm_Dispatch_ins_asm_db
			mov	retv,ebx
			pop	ebx
		}
	}
#endif

	return retv;
}

unsigned int ntvdm_dosntast_getversionex(OSVERSIONINFO *ovi) {
	unsigned int retv=0;

	if (ntvdm_dosntast_handle == DOSNTAST_HANDLE_UNASSIGNED)
		return 0; /* failed */
	if (!ntvdm_rm_code_alloc())
		return 0;

#if TARGET_MSDOS == 32
	{
		uint16_t myds=0;
		struct dpmi_realmode_call rc={0};
		__asm mov myds,ds
		rc.ebx = DOSNTAST_GETVERSIONEX_C;
		rc.esi = (uint32_t)ovi;
		rc.ecx = 1;
		rc.ds = myds;
		ntvdm_DispatchCall_dpmi(ntvdm_dosntast_handle,&rc);
		retv = rc.ebx;
	}
#else
	{
		const uint16_t s = FP_SEG(ovi),o = FP_OFF(ovi),h = ntvdm_dosntast_handle;

		__asm {
			.386p
			push	ds
			push	esi
			push	ecx
			mov	ebx,DOSNTAST_GETVERSIONEX
			xor	esi,esi
			mov	ax,h
			mov	si,s
			mov	ds,si
			mov	si,o
			xor	cx,cx
			ntvdm_Dispatch_ins_asm_db
			mov	retv,bx
			pop	esi
			pop	ebx
			pop	ds
		}
	}
#endif

	return retv;
}

#endif

const char *dos_flavor_str(uint8_t f) {
	switch (f) {
		case DOS_FLAVOR_NONE:		return "None";
		case DOS_FLAVOR_MSDOS:		return "MS-DOS";
		case DOS_FLAVOR_FREEDOS:	return "FreeDOS";
	}

	return "?";
}

#if TARGET_MSDOS == 32 && defined(WIN386) /* Watcom Win386 does NOT translate LPARAM for us */
void far *win386_alt_winnt_MapAliasToFlat(DWORD farptr) {
	/* FIXME: This only works by converting a 16:16 pointer directly to 16:32.
	 *        It works perfectly fine in QEMU and DOSBox, but I seem to remember something
	 *        about the x86 architecture and possible ways you can screw up using 16-bit
	 *        data segments with 32-bit code. Are those rumors true? Am I going to someday
	 *        load up Windows 3.1/95 on an ancient PC and find out this code crashes
	 *        horribly or has random problems?
	 *
	 *        We need this alternate path for Windows NT/2000/XP/Vista/7 because NTVDM.EXE
	 *        grants Watcom386 a limited ~2GB limit for the flat data segment (usually
	 *        0x7B000000 or something like that) instead of the full 4GB limit the 3.x/9x/ME
	 *        kernels usually grant. This matters because without the full 4GB limit we can't
	 *        count on overflow/rollover to reach below our data segment base. Watcom386's
	 *        MapAliasToFlat() unfortunately assumes just that. If we were to blindly rely
	 *        on it, then we would work just fine under 3.x/9x/ME but crash under
	 *        NT/2000/XP/Vista/7 the minute we need to access below our data segment (such as,
	 *        when handling the WM_GETMINMAXINFO message the LPARAM far pointer usually
	 *        points somewhere into NTVDM.EXE's DOS memory area when we're usually located
	 *        at the 2MB mark or so) */
	return MK_FP(farptr>>16,farptr&0xFFFF);
}

void far *win386_help_MapAliasToFlat(DWORD farptr) {
	if (windows_mode == WINDOWS_NT)
		return win386_alt_winnt_MapAliasToFlat(farptr);

	return (void far*)MapAliasToFlat(farptr); /* MapAliasToFlat() returns near pointer, convert to far! */
}
#endif

int smartdrv_close() {
	if (smartdrv_fd >= 0) {
		close(smartdrv_fd);
		smartdrv_fd = -1;
	}

	return 0;
}

int smartdrv_flush() {
	if (smartdrv_version == 0xFFFF || smartdrv_version == 0)
		return 0;

	if (smartdrv_version >= 0x400) { /* SMARTDRV 4.xx and later */
#if TARGET_MSDOS == 32
		__asm {
			push	eax
			push	ebx
			mov	eax,0x4A10		; SMARTDRV
			mov	ebx,0x0001		; FLUSH BUFFERS (COMMIT CACHE)
			int	0x2F
			pop	ebx
			pop	eax
		}
#else
		__asm {
			push	ax
			push	bx
			mov	ax,0x4A10		; SMARTDRV
			mov	bx,0x0001		; FLUSH BUFFERS (COMMIT CACHE)
			int	0x2F
			pop	bx
			pop	ax
		}
#endif
	}
	else if (smartdrv_version >= 0x300 && smartdrv_version <= 0x3FF) { /* SMARTDRV 3.xx */
		char buffer[0x1];
#if TARGET_MSDOS == 32
#else
		char far *bptr = (char far*)buffer;
#endif
		int rd=0;

		if (smartdrv_fd < 0)
			return 0;

		buffer[0] = 0; /* flush cache */
#if TARGET_MSDOS == 32
		/* FIXME: We do not yet have a 32-bit protected mode version.
		 *        DOS extenders do not appear to translate AX=0x4403 properly */
#else
		__asm {
			push	ax
			push	bx
			push	cx
			push	dx
			push	ds
			mov	ax,0x4403		; IOCTL SMARTAAR CACHE CONTROL
			mov	bx,smartdrv_fd
			mov	cx,0x1			; 0x01 bytes
			lds	dx,word ptr [bptr]
			int	0x21
			jc	err1
			mov	rd,ax			; CF=0, AX=bytes written
err1:			pop	ds
			pop	dx
			pop	cx
			pop	bx
			pop	ax
		}
#endif

		if (rd == 0)
			return 0;
	}

	return 1;
}

int smartdrv_detect() {
	unsigned int rvax=0,rvbp=0;

	if (smartdrv_version == 0xFFFF) {
		/* Is Microsoft SMARTDRV 4.x or equivalent disk cache present? */

#if TARGET_MSDOS == 32
		__asm {
			push	eax
			push	ebx
			push	ecx
			push	edx
			push	esi
			push	edi
			push	ebp
			push	ds
			mov	eax,0x4A10		; SMARTDRV 4.xx INSTALLATION CHECK AND HIT RATIOS
			mov	ebx,0
			mov	ecx,0xEBAB		; "BABE" backwards
			xor	ebp,ebp
			int	0x2F			; multiplex (hope your DOS extender supports it properly!)
			pop	ds
			mov	ebx,ebp			; copy EBP to EBX. Watcom C uses EBP to refer to the stack!
			pop	ebp
			mov	rvax,eax		; we only care about EAX and EBP(now EBX)
			mov	rvbp,ebx
			pop	edi
			pop	esi
			pop	edx
			pop	ecx
			pop	ebx
			pop	eax
		}
#else
		__asm {
			push	ax
			push	bx
			push	cx
			push	dx
			push	si
			push	di
			push	bp
			push	ds
			mov	ax,0x4A10		; SMARTDRV 4.xx INSTALLATION CHECK AND HIT RATIOS
			mov	bx,0
			mov	cx,0xEBAB		; "BABE" backwards
			xor	bp,bp
			int	0x2F			; multiplex
			pop	ds
			mov	bx,bp			; copy BP to BX. Watcom C uses BP to refer to the stack!
			pop	bp
			mov	rvax,ax			; we only care about EAX and EBP(now EBX)
			mov	rvbp,bx
			pop	di
			pop	si
			pop	dx
			pop	cx
			pop	bx
			pop	ax
		}
#endif

		if ((rvax&0xFFFF) == 0xBABE && (rvbp&0xFFFF) >= 0x400 && (rvbp&0xFFFF) <= 0x5FF) {
			/* yup. SMARTDRV 4.xx! */
			smartdrv_version = (rvbp&0xFF00) + (((rvbp>>4)&0xF) * 10) + (rvbp&0xF); /* convert BCD to decimal */
		}

		if (smartdrv_version == 0xFFFF) {
			char buffer[0x28];
#if TARGET_MSDOS == 32
#else
			char far *bptr = (char far*)buffer;
#endif
			int rd=0;

			memset(buffer,0,sizeof(buffer));

			/* check for SMARTDRV 3.xx */
			smartdrv_fd = open("SMARTAAR",O_RDONLY);
			if (smartdrv_fd >= 0) {
				/* FIXME: The DOS library should provide a common function to do IOCTL read/write character "control channel" functions */
#if TARGET_MSDOS == 32
				/* FIXME: We do not yet have a 32-bit protected mode version.
				 *        DOS extenders do not appear to translate AX=0x4402 properly */
#else
				__asm {
					push	ax
					push	bx
					push	cx
					push	dx
					push	ds
					mov	ax,0x4402		; IOCTL SMARTAAR GET CACHE STATUS
					mov	bx,smartdrv_fd
					mov	cx,0x28			; 0x28 bytes
					lds	dx,word ptr [bptr]
					int	0x21
					jc	err1
					mov	rd,ax			; CF=0, AX=bytes read
err1:					pop	ds
					pop	dx
					pop	cx
					pop	bx
					pop	ax
				}
#endif

				/* NTS: Despite reading back 0x28 bytes of data, this IOCTL
				 *      returns AX=1 for some reason. */
				if (rd > 0 && rd <= 0x28) {
					if (buffer[0] == 1/*write through flag*/ && buffer[15] == 3/*major version number*/) { /* SMARTDRV 3.xx */
						smartdrv_version = ((unsigned short)buffer[15] << 8) + ((unsigned short)buffer[14]);
					}
					else {
						close(smartdrv_fd);
					}
				}
			}
		}

		/* didn't find anything. then no SMARTDRV */
		if (smartdrv_version == 0xFFFF)
			smartdrv_version = 0;
	}

	return (smartdrv_version != 0);
}

