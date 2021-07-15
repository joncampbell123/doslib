
#if defined(USE_WIN32)
# include <windows.h>
# include <mmsystem.h>
#endif

#include "palette.h"
#include "keyboard.h"

struct ifevidinfo_t {
	unsigned int						width,height;
	unsigned char*						vram_base;	/* pointer to video RAM, if available and linear framebuffer (not bank switched) */
	unsigned long						vram_size;	/* size of video RAM, if available */
	unsigned int						vram_pitch;	/* pitch of video RAM */
	unsigned char*						buf_base;	/* pointer to offscreen memory to draw to */
	unsigned long						buf_size;	/* size of offscreen memory to draw to */
	unsigned long						buf_alloc;	/* allocated size of offscreen memory */
	unsigned char*						buf_first_row;	/* topmost scanline, left edge */
	int							buf_pitch;	/* bytes per scanline, can be negative if bottom-up DIB in Windows 3.1 */
};

typedef uint32_t ifefunc_GetTicks_t(void);
typedef void ifefunc_ResetTicks_t(const uint32_t base);
typedef void ifefunc_UpdateFullScreen_t(void);
typedef ifevidinfo_t* ifefunc_GetVidInfo_t(void);
typedef bool ifefunc_UserWantsToQuit_t(void);
typedef void ifefunc_CheckEvents_t(void);
typedef void ifefunc_WaitEvent_t(const int wait_ms);
typedef bool ifefunc_BeginScreenDraw_t(void);
typedef void ifefunc_EndScreenDraw_t(void);
typedef void ifefunc_ShutdownVideo_t(void);
typedef void ifefunc_InitVideo_t(void);
typedef void ifefunc_FlushKeyboardInput_t(void); /* NTS: This flushes the framework's queue AND the host environment's queue if possible */

struct ifeapi_t {
	const char*						name;
	ifefunc_SetPaletteColors_t*				SetPaletteColors;
	ifefunc_GetTicks_t*					GetTicks;
	ifefunc_ResetTicks_t*					ResetTicks;
	ifefunc_UpdateFullScreen_t*				UpdateFullScreen;
	ifefunc_GetVidInfo_t*					GetVidInfo;
	ifefunc_UserWantsToQuit_t*				UserWantsToQuit;
	ifefunc_CheckEvents_t*					CheckEvents;
	ifefunc_WaitEvent_t*					WaitEvent;
	ifefunc_BeginScreenDraw_t*				BeginScreenDraw;
	ifefunc_EndScreenDraw_t*				EndScreenDraw;
	ifefunc_ShutdownVideo_t*				ShutdownVideo;
	ifefunc_InitVideo_t*					InitVideo;
	ifefunc_FlushKeyboardInput_t*				FlushKeyboardInput;
};

extern ifeapi_t *ifeapi;

#if defined(USE_SDL2)
extern ifeapi_t ifeapi_sdl2;
# define ifeapi_default ifeapi_sdl2
#endif

#if defined(USE_WIN32)
extern ifeapi_t ifeapi_win32;
# define ifeapi_default ifeapi_win32
#endif

#if defined(USE_DOSLIB)
extern ifeapi_t ifeapi_doslib;
# define ifeapi_default ifeapi_doslib
#endif

#if defined(USE_WIN32)
bool priv_IFEWin32Init(HINSTANCE hInstance,HINSTANCE hPrevInstance/*doesn't mean anything in Win32*/,LPSTR lpCmdLine,int nCmdShow);
#else
bool priv_IFEMainInit(int argc,char **argv);
#endif

