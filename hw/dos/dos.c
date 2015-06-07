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

/* DEBUG: Flush out calls that aren't there */
#ifdef TARGET_OS2
# define int86 ___EVIL___
# define int386 ___EVIL___
# define ntvdm_RegisterModule ___EVIL___
# define ntvdm_UnregisterModule ___EVIL___
# define _dos_getvect ___EVIL___
# define _dos_setvect ___EVIL___
#endif

struct lib_dos_options lib_dos_option={0};

/* DOS version info */
uint8_t dos_flavor = 0;
uint16_t dos_version = 0;
uint32_t freedos_kernel_version = 0;
const char *dos_version_method = NULL;
#if TARGET_MSDOS == 32 && !defined(TARGET_OS2)
int8_t dpmi_no_0301h = -1; /* whether or not the DOS extender provides function 0301h */
#endif

#if TARGET_MSDOS == 32
char *freedos_kernel_version_str = NULL;
#else
char far *freedos_kernel_version_str = NULL;
#endif

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

