
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#include <hw/8254/8254.h>
#include <hw/vga/vgatty.h>
#include <hw/dos/doswin.h>

#include <hw/vga/gvg256.h>
#include <hw/vga/tvg256.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
# include <windows/apihelp.h>
# include <windows/dispdib/dispdib.h>
# include <windows/win16eb/win16eb.h>
#else
# include <hw/8254/8254.h>
#endif

int main() {
#if defined(TARGET_WINDOWS)
	HWND hwnd;
#endif
	unsigned char redraw;
	int c;

	probe_dos();
	detect_windows();

#if !defined(TARGET_WINDOWS)
	if (!probe_8254()) {
		printf("8254 not found (I need this for time-sensitive portions of the driver)\n");
		return 1;
	}
#endif

#if defined(TARGET_WINDOWS)
# ifdef WIN_STDOUT_CONSOLE
	hwnd = _win_hwnd();
# else
	hwnd = GetFocus();

	printf("WARNING: The Win32 console version is experimental. It generally works...\n");
	printf("         but can run into problems if focus is taken or if the console window\n");
	printf("         is started minimized or maximized. ENTER to continue, ESC to cancel.\n");
	printf("         If this console is full-screen, hit ALT+ENTER to switch to window.\n");
	do {
		c = getch();
		if (c == 27) return 1;
	} while (c != 13);
# endif

# if TARGET_WINDOWS == 32
	if (!InitWin16EB()) {
		MessageBox(hwnd,"Failed to init win16eb","Win16eb",MB_OK);
		return 1;
	}
# endif

	/* setup DISPDIB */
	if (DisplayDibLoadDLL()) {
		MessageBox(hwnd,dispDibLastError,"Failed to load DISPDIB.DLL",MB_OK);
		return 1;
	}

	if (DisplayDibCreateWindow(hwnd)) {
		MessageBox(hwnd,dispDibLastError,"Failed to create DispDib window",MB_OK);
		return 1;
	}

	if (DisplayDibGenerateTestImage()) {
		MessageBox(hwnd,dispDibLastError,"Failed to generate test card",MB_OK);
		return 1;
	}

	if (DisplayDibDoBegin()) {
		MessageBox(hwnd,dispDibLastError,"Failed to begin display",MB_OK);
		return 1;
	}
#endif

	if (!probe_vga()) {
#if defined(TARGET_WINDOWS)
		DisplayDibDoEnd();
		DisplayDibUnloadDLL();
		MessageBox(hwnd,"Probe failed","VGA lib",MB_OK);
#else
		printf("VGA probe failed\n");
#endif
		return 1;
	}

	int10_setmode(3);
	update_state_from_vga();

	redraw = 1;
	while (1) {
		if (redraw) {
			/* position the cursor to home */
			vga_moveto(0,0);
			vga_sync_bios_cursor();

			printf("ESC  Exit to DOS       A. 320x200x256 VGA\n");
		}

		c = getch();
		if (c == 27)
			break;
		else if (c == 'a' || c == 'A')
			v320x200x256_VGA_menu();
	}

#if defined(TARGET_WINDOWS)
	DisplayDibDoEnd();
	if (DisplayDibUnloadDLL())
		MessageBox(hwnd,dispDibLastError,"Failed to unload DISPDIB.DLL",MB_OK);

# if TARGET_MSDOS == 32
	FreeWin16EB();
# endif
#endif

	return 0;
}

