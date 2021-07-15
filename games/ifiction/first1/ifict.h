
#include "palette.h"

struct ifeapi_t {
	char*							name;
	ifefunc_SetPaletteColors_t*				SetPaletteColors;
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

