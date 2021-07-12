
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdint.h>

/* NTS: Do not use SDL2 if targeting MS-DOS or Windows 3.1/95, we will have our own framework to use for those targets */
#include <SDL2/SDL.h>

/* NTS: Do not assume the full 256-color palette, 256-color Windows uses 20 of them, leaving us with 236 of them.
 *      We *could* just render with 256 colors but of course that means some colors get combined, so, don't.
 *      Not a problem so much if using Windows GDI but if we're going to play with DirectX or the earlier hacky
 *      Windows 3.1 equivalents, we need to worry about that. */

static char	fatal_tmp[256];

SDL_Window*	sdl_window = NULL;
SDL_Surface*	sdl_window_surface = NULL;
SDL_Surface*	sdl_game_surface = NULL;
SDL_Palette*	sdl_game_palette = NULL;
SDL_Color	sdl_pal[256];
Uint32		sdl_ticks_base = 0; /* use Uint32 type provided by SDL2 here to avoid problems */

#pragma pack(push,1)
struct IFEPaletteEntry {
	uint8_t	r,g,b;
};
#pragma pack(pop)

void IFEFatalError(const char *msg,...);
void IFEUpdateFullScreen(void);

/* WARNING: Will wrap around after 49 days. You're not playing this game that long, are you?
 *          Anyway to avoid Windows-style crashes at 49 days, call IFEResetTicks() with the
 *          return value of IFEGetTicks() to periodically count from zero. */
uint32_t IFEGetTicks(void) {
	return uint32_t(SDL_GetTicks() - sdl_ticks_base);
}

void IFEResetTicks(const uint32_t base) {
	sdl_ticks_base = base; /* NTS: Use return value of IFEGetTicks() */
}

void IFEInitVideo(void) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		IFEFatalError("SDL2 failed to initialize");

	if (sdl_window == NULL && (sdl_window=SDL_CreateWindow("",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,640,480,SDL_WINDOW_SHOWN)) == NULL)
		IFEFatalError("SDL2 window creation failed");
	if (sdl_window_surface == NULL && (sdl_window_surface=SDL_GetWindowSurface(sdl_window)) == NULL)
		IFEFatalError("SDL2 window surface failed");
	if (sdl_game_surface == NULL && (sdl_game_surface=SDL_CreateRGBSurfaceWithFormat(0,640,480,8,SDL_PIXELFORMAT_INDEX8)) == NULL)
		IFEFatalError("SDL2 game surface (256-color) failed");
	if (sdl_game_palette == NULL && (sdl_game_palette=SDL_AllocPalette(256)) == NULL)
		IFEFatalError("SDL2 game palette");

	/* first color should be black, SDL2 will init palette to white for some reason */
	memset(sdl_pal,0,sizeof(SDL_Color));
	if (SDL_SetPaletteColors(sdl_game_palette,sdl_pal,0,1) != 0)
		IFEFatalError("SDL2 game palette set colors");

	/* apply palette to surface */
	if (SDL_SetSurfacePalette(sdl_game_surface,sdl_game_palette) != 0)
		IFEFatalError("SDL2 game palette set");

	/* make sure screen is cleared black */
	SDL_FillRect(sdl_game_surface,NULL,0);
	IFEUpdateFullScreen();
}

void IFEShutdownVideo(void) {
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

void IFESetPaletteColors(const unsigned int first,const unsigned int count,IFEPaletteEntry *pal) {
	unsigned int i;

	if (first >= 256u || count > 256u || (first+count) > 256u)
		IFEFatalError("SetPaletteColors out of range first=%u count=%u",first,count);

	for (i=0;i < count;i++) {
		sdl_pal[i+first].r = pal[i].r;
		sdl_pal[i+first].g = pal[i].g;
		sdl_pal[i+first].b = pal[i].b;
		sdl_pal[i+first].a = 0xFFu;
	}

	if (SDL_SetPaletteColors(sdl_game_palette,sdl_pal,first,count) != 0)
		IFEFatalError("SDL2 game palette set colors");
}

void IFEUpdateFullScreen(void) {
	if (SDL_BlitSurface(sdl_game_surface,NULL,sdl_window_surface,NULL) != 0)
		IFEFatalError("Game to window BlitSurface");

	if (SDL_UpdateWindowSurface(sdl_window) != 0)
		IFEFatalError("Window surface update");
}

void IFEFatalError(const char *msg,...) {
	va_list va;

	va_start(va,msg);
	vsnprintf(fatal_tmp,sizeof(fatal_tmp)/*includes NUL byte*/,msg,va);
	va_end(va);

	IFEShutdownVideo();

	fprintf(stderr,"Fatal error: %s\n",fatal_tmp);
	exit(127);
}

int main(int argc,char **argv) {
	//not used yet
	(void)argc;
	(void)argv;

	IFEInitVideo();

	IFEResetTicks(IFEGetTicks());
	while (IFEGetTicks() < 1000) { }

	{
		IFEPaletteEntry pal[256];
		unsigned int i;

		for (i=0;i < 64;i++) {
			pal[i].r = i*4u;
			pal[i].g = i*4u;
			pal[i].b = i*4u;

			pal[i+64u].r = i*4u;
			pal[i+64u].g = 0;
			pal[i+64u].b = 0;

			pal[i+128u].r = 0;
			pal[i+128u].g = i*4u;
			pal[i+128u].b = 0;

			pal[i+192u].r = 0;
			pal[i+192u].g = 0;
			pal[i+192u].b = i*4u;
		}

		IFESetPaletteColors(0,256,pal);
	}

	{
		unsigned int x,y;

		if (SDL_MUSTLOCK(sdl_game_surface) && SDL_LockSurface(sdl_game_surface) != 0)
			IFEFatalError("SDL2 game surface lock fail");
		if (sdl_game_surface->pixels == NULL)
			IFEFatalError("SDL2 game surface pixels == NULL");

		memset(sdl_game_surface->pixels,0,sdl_game_surface->pitch * sdl_game_surface->h);

		for (y=0;y < (unsigned int)sdl_game_surface->h;y++) {
			unsigned char *row = (unsigned char*)sdl_game_surface->pixels + (y * sdl_game_surface->pitch);
			for (x=0;x < (unsigned int)sdl_game_surface->w;x++) {
				if ((x & 0xF0) != 0xF0)
					row[x] = (y & 0xFF);
				else
					row[x] = 0;
			}
		}

		if (SDL_MUSTLOCK(sdl_game_surface))
			SDL_UnlockSurface(sdl_game_surface);

		IFEUpdateFullScreen();

		IFEResetTicks(IFEGetTicks());
		while (IFEGetTicks() < 3000) { }
	}

	IFEShutdownVideo();
	return 0;
}

