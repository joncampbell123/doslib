
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
extern bool		sdl_signal_to_quit;

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

static bool p_UserWantsToQuit(void) {
	return sdl_signal_to_quit;
}

static void priv_ProcessEvent(SDL_Event &ev) {
	if (ev.type == SDL_QUIT) {
		sdl_signal_to_quit = true;
	}
}

static void p_CheckEvents(void) {
	SDL_Event ev;

	if (SDL_PollEvent(&ev))
		priv_ProcessEvent(ev);
}

static void p_WaitEvent(const int wait_ms) {
	SDL_Event ev;

	if (SDL_WaitEventTimeout(&ev,wait_ms))
		priv_ProcessEvent(ev);
}

static bool p_BeginScreenDraw(void) {
	if (SDL_MUSTLOCK(sdl_game_surface) && SDL_LockSurface(sdl_game_surface) != 0)
		return false;
	if (sdl_game_surface->pixels == NULL)
		IFEFatalError("SDL2 game surface pixels == NULL"); /* that's a BUG if this happens! */
	if (sdl_game_surface->pitch < 0)
		IFEFatalError("SDL2 game surface pitch is negative");

	ifevidinfo_sdl2.buf_base = ifevidinfo_sdl2.buf_first_row = (unsigned char*)(sdl_game_surface->pixels);
	return true;
}

static void p_EndScreenDraw(void) {
	if (SDL_MUSTLOCK(sdl_game_surface))
		SDL_UnlockSurface(sdl_game_surface);

	ifevidinfo_sdl2.buf_base = ifevidinfo_sdl2.buf_first_row = NULL;
}

static void p_ShutdownVideo(void) {
	if (sdl_game_surface != NULL) {
		SDL_FreeSurface(sdl_game_surface);
		sdl_game_surface = NULL;
	}
	if (sdl_game_palette != NULL) {
		SDL_FreePalette(sdl_game_palette);
		sdl_game_palette = NULL;
	}
	if (sdl_window != NULL) {
		SDL_DestroyWindow(sdl_window);
		sdl_window_surface = NULL;
		sdl_window = NULL;
	}
	SDL_Quit();
}

ifeapi_t ifeapi_sdl2 = {
	"SDL2",
	p_SetPaletteColors,
	p_GetTicks,
	p_ResetTicks,
	p_UpdateFullScreen,
	p_GetVidInfo,
	p_UserWantsToQuit,
	p_CheckEvents,
	p_WaitEvent,
	p_BeginScreenDraw,
	p_EndScreenDraw,
	p_ShutdownVideo
};
#endif

