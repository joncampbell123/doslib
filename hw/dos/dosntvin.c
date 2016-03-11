
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

#if defined(NTVDM_CLIENT) && !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
uint8_t ntvdm_dosntast_tried = 0;
uint16_t ntvdm_dosntast_io_base = 0;
uint16_t ntvdm_dosntast_handle = (~0U);

/* initialize the library.
 * if dont_load_dosntast is set, then it will not load the VDD driver but will use the driver if already loaded */
unsigned int ntvdm_dosntast_init() {
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
#endif

