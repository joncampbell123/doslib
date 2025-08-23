/* TODO: This should be moved to it's own library under the /windows subdirectory */

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

#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 16 && WINVER >= 0x200
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

