
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

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

void IFEFatalError(const char *msg,...);

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
	if (SDL_SetSurfacePalette(sdl_game_surface,sdl_game_palette) != 0)
		IFEFatalError("SDL2 game palette set");
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

void IFE_UpdateFullScreen(void) {
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

	{
		unsigned int i;
		SDL_Color pal[256];

		for (i=0;i < 64;i++) {
			pal[i].r = i*4u;
			pal[i].g = i*4u;
			pal[i].b = i*4u;
			pal[i].a = 0xFF;

			pal[i+64u].r = i*4u;
			pal[i+64u].g = 0;
			pal[i+64u].b = 0;
			pal[i+64u].a = 0xFF;

			pal[i+128u].r = 0;
			pal[i+128u].g = i*4u;
			pal[i+128u].b = 0;
			pal[i+128u].a = 0xFF;

			pal[i+192u].r = 0;
			pal[i+192u].g = 0;
			pal[i+192u].b = i*4u;
			pal[i+192u].a = 0xFF;
		}

		if (SDL_SetPaletteColors(sdl_game_palette,pal,0,256) != 0)
			IFEFatalError("SDL2 game palette set colors");
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

		IFE_UpdateFullScreen();

		sleep(3);
	}

	IFEShutdownVideo();
	return 0;
}

