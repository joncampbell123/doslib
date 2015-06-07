
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

#if defined(NTVDM_CLIENT) && !defined(TARGET_WINDOWS)
uint8_t ntvdm_dosntast_tried = 0;
uint16_t ntvdm_dosntast_handle = (~0U);
#endif

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

