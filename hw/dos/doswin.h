
#ifndef __HW_DOS_DOSWIN_H
#define __HW_DOS_DOSWIN_H

#include <hw/cpu/cpu.h>
#include <stdint.h>

#if !defined(TARGET_WINDOWS) && !defined(TARGET_OS2) && !defined(TARGET_VXD)
/* NTVDM.EXE DOSNTAST.VDD call support */
#include <windows/ntvdm/ntvdmlib.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum {
	WINEMU_NONE=0,
	WINEMU_WINE
};

enum {
	WINDOWS_NONE=0,
	WINDOWS_REAL,
	WINDOWS_STANDARD,
	WINDOWS_ENHANCED,
	WINDOWS_NT,
	WINDOWS_OS2,			/* Not Windows, OS/2 */
					/* Exact meaning: If we're a DOS/Windows program, then we know we're running under OS/2
					   and OS/2 is emulating DOS/Windows. If we're an OS/2 program, then we're in our native
					   environment */
	WINDOWS_MAX
};

extern const char *windows_mode_strs[WINDOWS_MAX];
#define windows_mode_str(x)	windows_mode_strs[x]

extern uint8_t windows_mode;
extern uint16_t windows_version;
extern uint8_t windows_emulation;
extern const char *windows_version_method;
extern const char *windows_emulation_comment_str;

/* TODO: Someday, these will become variables */

/* whether the Windows emulation allows Win16 to call DPMI */
#define windows_emulation_includes_dpmi		0

int detect_windows();
const char *windows_emulation_str(uint8_t e);

#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 32 && !defined(WIN386) && !defined(TARGET_VXD)
# include <windows.h>
# include <stdint.h>

extern unsigned char		win9x_qt_thunk_probed;
extern unsigned char		win9x_qt_thunk_available;

typedef WORD HGLOBAL16; /* <- NTS: Taken from WINE header definitions */

extern void			(__stdcall *QT_Thunk)();
extern DWORD			(__stdcall *LoadLibrary16)(LPSTR lpszLibFileName);
extern VOID			(__stdcall *FreeLibrary16)(DWORD dwInstance);
extern HGLOBAL16		(__stdcall *GlobalAlloc16)(UINT flags,DWORD size);
extern HGLOBAL16		(__stdcall *GlobalFree16)(HGLOBAL16 handle);
extern DWORD			(__stdcall *GlobalLock16)(HGLOBAL16 handle);
extern BOOL			(__stdcall *GlobalUnlock16)(HGLOBAL16 handle);
extern VOID			(__stdcall *GlobalUnfix16)(HGLOBAL16 handle);
extern DWORD			(__stdcall *GetProcAddress16)(DWORD dwInstance, LPSTR lpszProcName);
extern VOID			(__stdcall *GlobalFix16)(HGLOBAL16 handle);

extern DWORD			win9x_kernel_win16;
extern DWORD			win9x_user_win16;

int Win9xQT_ThunkInit();
void Win9xQT_ThunkFree();
#endif

#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 16
# include <toolhelp.h>
extern HMODULE			ToolHelpDLL;
extern unsigned char		ToolHelpProbed;
extern BOOL			(PASCAL FAR *__TimerCount)(TIMERINFO FAR *t);
extern BOOL			(PASCAL FAR *__InterruptUnRegister)(HTASK htask);
extern BOOL			(PASCAL FAR *__InterruptRegister)(HTASK htask,FARPROC callback);

int ToolHelpInit();
void ToolHelpFree();
#endif

#if TARGET_MSDOS == 16 && defined(TARGET_WINDOWS)
DWORD Win16_KERNELSYM(const unsigned int ord);
unsigned int Win16_AHINCR(void);
unsigned int Win16_AHSHIFT(void);
#endif

#ifdef __cplusplus
}
#endif

#endif //__HW_DOS_DOSWIN_H

