
#include "palette.h"

typedef uint32_t ifefunc_GetTicks_t(void);
typedef void ifefunc_ResetTicks_t(const uint32_t base);

struct ifeapi_t {
	const char*						name;
	ifefunc_SetPaletteColors_t*				SetPaletteColors;
	ifefunc_GetTicks_t*					GetTicks;
	ifefunc_ResetTicks_t*					ResetTicks;
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

