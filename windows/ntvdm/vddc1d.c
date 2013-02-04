
/* This is a Windows NT VDD driver */
#define NTVDM_VDD_DRIVER

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>
#include <conio.h>
#include <fcntl.h>
#include <dos.h>
#include <i86.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/dosbox.h>
#include <windows/ntvdm/ntvdmlib.h>
#include <windows/ntvdm/ntvdmvdd.h>

/* this is a DLL for Win32 */
#if TARGET_MSDOS == 16 || !defined(TARGET_WINDOWS)
# error this is a DLL for Win32 only
#endif

/* this variable is kind of a hack.
 * NTVDM.EXE allows the DOS program to call our dispatch routine.
 * BUT, there doesn't seem to be any kind of function we can use
 * to tell between a 32-bit and a 16-bit DOS program. So we have
 * to rely on a "handshake" where the DOS program tells us. */
static unsigned char			dos_is_32bit = 0;

BOOL WINAPI DllMain(HINSTANCE hInstanceDLL,DWORD fdwReason,LPVOID lpvReserved) {
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			break;
		case DLL_PROCESS_DETACH:
			break;
		case DLL_THREAD_ATTACH:
			break;
		case DLL_THREAD_DETACH:
			break;
	}

	return TRUE;
}

/* Microsoft documentation on this "init" routine is lacking */
__declspec(dllexport) void WINAPI Init() {
	/* 32-bit DOS builds set ECX == 0x32323232 */
	dos_is_32bit = (getECX() == 0x32323232);
	MessageBox(NULL,"I live!",dos_is_32bit ? "32" : "16",MB_OK);
}

__declspec(dllexport) void WINAPI Dispatch() {
	uint32_t command;
	char tmp[64];

	command = getEBX();
	if (command == 0x1234) {
		/* second chance: a 16-bit counterpart can change our perception here via ECX */
		if (getECX() == 0x32323232)
			dos_is_32bit = 1;
		else if (getECX() == 0x16161616)
			dos_is_32bit = 0;

		MessageBox(NULL,"Hello","",MB_OK);
		setEBX(0xABCDEF);
	}
	else if (command == 0xAA55AA55) {
		const char *check = "Testing, testing, 1 2 3";
		uint16_t segm;
		uint32_t ofs;
		unsigned char vMode;
		unsigned char *ptr = NULL;

		/* NTS: For whatever reason, Dispatch calls made from protected mode are ignored by NTVDM.EXE.
		 *      They have to be made from "real" aka v86 mode. So it is highly unlikely we'd get here
		 *      from protected mode */
		if (getMSW() & 1) {
			/* ignore */
		}
		else {
			segm = getES();
			vMode = dos_is_32bit ? VDM_PM : VDM_V86;
			ofs = dos_is_32bit ? getECX() : getCX();

			ptr = VdmMapFlat(segm,ofs,vMode);

			/* ES:CX = pointer to string */
			if (ptr != NULL && memcmp(ptr,check,strlen(check)+1) == 0)
				setEBX(0x55AA55AA);
			else
				setEBX(0x66666666);

			if (ptr != NULL) /* <- WARNING: This assumes whatever "ptr" is, it is a NULL-terminated ASCII */
				MessageBox(NULL,ptr,"You said:",MB_OK);

			if (ptr != NULL) {
				VdmUnmapFlat(segm,ofs,(DWORD)ptr,vMode);
			}
		}
	}
	else {
		sprintf(tmp,"0x%08lX\n",(unsigned long)getEBX());
		MessageBox(NULL,tmp,"Unknown command",MB_OK);
	}
}

