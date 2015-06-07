
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <malloc.h>
#include <stdio.h>
#include <conio.h>
#include <fcntl.h>
#include <dos.h>
#include <i86.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/dos/dosbox.h>
#include <windows/ntvdm/ntvdmlib.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

int main() {
#if defined(NTVDM_CLIENT)
	uint16_t disph;
	uint32_t t1=0;
#endif

	probe_dos();
	cpu_probe();
	printf("DOS version %x.%02u\n",dos_version>>8,dos_version&0xFF);
	printf("    Method: '%s'\n",dos_version_method);
	if (detect_windows()) {
		printf("I am running under Windows.\n");
		printf("    Mode: %s\n",windows_mode_str(windows_mode));
		printf("    Ver:  %x.%02u\n",windows_version>>8,windows_version&0xFF);
		printf("    Method: '%s'\n",windows_version_method);
	}
	else {
		printf("Not running under Windows or OS/2\n");
	}

#if defined(NTVDM_CLIENT)
	if (i_am_ntvdm_client()) {
		printf("I am an NTVDM.EXE client\n");

		disph = ntvdm_RegisterModule("VDDC1D.DLL","Init","Dispatch");
		if (ntvdm_RM_ERR(disph)) {
			printf("Unable to register module, err %d (%s)\n",(int16_t)disph,ntvdm_RM_ERR_str(disph));
			return 1;
		}

		/* trigger dispatch with test */
		/* FIXME: Win16 builds: If mouse, keyboard, etc. events happen during the Message box, NTVDM.EXE crashes on return */
#if TARGET_MSDOS == 32
		{
			struct dpmi_realmode_call rc={0};
			rc.ebx = 0x1234;
			rc.ecx = 0x32323232; /* this is a 32-bit build */
			ntvdm_DispatchCall_dpmi(disph,&rc);
			t1 = rc.ebx;
		}
#else
		__asm {
			.386p
			push	ebx
			push	ecx
			mov	ebx,0x1234
			mov	ecx,0x16161616 /* this is a 16-bit build */
			mov	ax,disph
			ntvdm_Dispatch_ins_asm_db
			mov	t1,ebx
			pop	ecx
			pop	ebx
		}
#endif
		printf("Dispatch response: EBX=0x%08lX. ",t1);
		if (t1 != 0xABCDEF) printf(" [WRONG!]");
		printf("\n");

		/* test #2: test the DLL's ability to read our memory */
		{
			const char FAR *readme = "Testing, testing, 1 2 3";

			/* dispatch will return 0x55AA55AA if it sees "Testing, testing, 1 2 3" */
#if TARGET_MSDOS == 32
			{
				uint16_t myds=0;
				struct dpmi_realmode_call rc={0};
				__asm {
					mov	ax,ds
					mov	myds,ax
				}
				rc.ebx = 0xAA55AA55;
				rc.ecx = (uint32_t)readme;
				rc.es = myds;
				ntvdm_DispatchCall_dpmi(disph,&rc);
				t1 = rc.ebx;
			}
#else
			__asm {
				.386p
				push	ebx
				push	ecx
				push	es
				mov	ebx,0xAA55AA55
				les	cx,readme
				mov	ax,disph
				ntvdm_Dispatch_ins_asm_db
				mov	t1,ebx
				pop	es
				pop	ecx
				pop	ebx
			}
#endif
			printf("Dispatch response: EBX=0x%08lX. ",t1);
			if (t1 != 0x55AA55AA) printf(" [WRONG!]");
			printf("\n");
		}

		ntvdm_UnregisterModule(disph);
	}
	else {
		printf("NTVDM.EXE not detected\n");
	}
#else
	printf("This program is not a client of NTVDM.EXE and cannot carry out it's tests\n");
#endif

	return 0;
}

