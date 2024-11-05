#ifndef TARGET_WINDOWS
# error This is Windows code, not DOS
#endif

#include <windows.h>

/* Bethesda joke */

int PASCAL WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow) {
	(void)hInstance;
	(void)hPrevInstance;
	(void)lpCmdLine;
	(void)nCmdShow;

	MessageBox(
		NULL,
		"A Bethesda.net account is required to run this legacy operating system\n"
		"\n"
		"Please connect to the internet to continue.",
		"Always-on requirement",
		MB_OK|MB_ICONHAND|MB_SYSTEMMODAL);

	return 0;
}

