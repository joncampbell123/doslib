
#include "palette.h"

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

struct ifeapi_t {
	const char*						name;
	ifefunc_SetPaletteColors_t*				SetPaletteColors;
	ifefunc_GetTicks_t*					GetTicks;
	ifefunc_ResetTicks_t*					ResetTicks;
	ifefunc_UpdateFullScreen_t*				UpdateFullScreen;
	ifefunc_GetVidInfo_t*					GetVidInfo;
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

