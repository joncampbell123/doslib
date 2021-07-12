
#include <stdio.h>
#include <stdarg.h>

/* NTS: Do not use SDL2 if targeting MS-DOS or Windows 3.1/95, we will have our own framework to use for those targets */
#include <SDL2/SDL.h>

static char	fatal_tmp[256];

SDL_Window*	sdl_window = NULL;
SDL_Surface*	sdl_window_surface = NULL;
SDL_Surface*	sdl_game_surface = NULL;
SDL_Palette*	sdl_game_palette = NULL;

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

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		IFEFatalError("SDL2 failed to initialize");

	if ((sdl_window=SDL_CreateWindow("",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,640,480,SDL_WINDOW_SHOWN)) == NULL)
		IFEFatalError("SDL2 window creation failed");
	if ((sdl_window_surface=SDL_GetWindowSurface(sdl_window)) == NULL)
		IFEFatalError("SDL2 window surface failed");
	if ((sdl_game_surface=SDL_CreateRGBSurfaceWithFormat(0,640,480,8,SDL_PIXELFORMAT_INDEX8)) == NULL)
		IFEFatalError("SDL2 game surface (256-color) failed");
	if ((sdl_game_palette=SDL_AllocPalette(256)) == NULL)
		IFEFatalError("SDL2 game palette");
	if (SDL_SetSurfacePalette(sdl_game_surface,sdl_game_palette) != 0)
		IFEFatalError("SDL2 game palette set");

	IFEShutdownVideo();
	return 0;
}

