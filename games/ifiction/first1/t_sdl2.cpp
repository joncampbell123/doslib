
#if defined(USE_SDL2)
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#if defined(__APPLE__) /* Brew got the headers wrong here */
# include <SDL.h>
#else
# include <SDL2/SDL.h>
#endif

#include "ifict.h"
#include "utils.h"
#include "debug.h"
#include "fatal.h"
#include "palette.h"

extern SDL_Color	sdl_pal[256];
extern SDL_Palette*	sdl_game_palette;
extern Uint32		sdl_ticks_base;
extern SDL_Window*	sdl_window;
extern SDL_Surface*	sdl_window_surface;
extern SDL_Surface*	sdl_game_surface;
extern ifevidinfo_t	ifevidinfo_sdl2;

static void p_SetPaletteColors(const unsigned int first,const unsigned int count,IFEPaletteEntry *pal) {
	unsigned int i;

	priv_SetPaletteColorsRangeCheck(first,count);

	for (i=0;i < count;i++) {
		sdl_pal[i+first].r = pal[i].r;
		sdl_pal[i+first].g = pal[i].g;
		sdl_pal[i+first].b = pal[i].b;
		sdl_pal[i+first].a = 0xFFu;
	}

	if (SDL_SetPaletteColors(sdl_game_palette,sdl_pal,first,count) != 0)
		IFEFatalError("SDL2 game palette set colors");
}

static uint32_t p_GetTicks(void) {
	return uint32_t(SDL_GetTicks() - sdl_ticks_base);
}

static void p_ResetTicks(const uint32_t base) {
	sdl_ticks_base += base; /* NTS: Use return value of IFEGetTicks() */
}

static void p_UpdateFullScreen(void) {
	if (SDL_BlitSurface(sdl_game_surface,NULL,sdl_window_surface,NULL) != 0)
		IFEFatalError("Game to window BlitSurface");

	if (SDL_UpdateWindowSurface(sdl_window) != 0)
		IFEFatalError("Window surface update");
}

static ifevidinfo_t* p_GetVidInfo(void) {
	return &ifevidinfo_sdl2;
}

ifeapi_t ifeapi_sdl2 = {
	"SDL2",
	p_SetPaletteColors,
	p_GetTicks,
	p_ResetTicks,
	p_UpdateFullScreen,
	p_GetVidInfo
};
#endif

