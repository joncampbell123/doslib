
#include <stdio.h>
#include <stdarg.h>

/* NTS: Do not use SDL2 if targeting MS-DOS or Windows 3.1/95, we will have our own framework to use for those targets */
#include <SDL2/SDL.h>

static char	fatal_tmp[256];

void IFEFatalError(const char *msg,...) {
	va_list va;

	va_start(va,msg);
	vsnprintf(fatal_tmp,sizeof(fatal_tmp)/*includes NUL byte*/,msg,va);
	va_end(va);

	SDL_Quit();

	fprintf(stderr,"Fatal error: %s\n",fatal_tmp);
	exit(127);
}

int main(int argc,char **argv) {
	//not used yet
	(void)argc;
	(void)argv;

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		IFEFatalError("SDL2 failed to initialize");
		return 1;
	}

	SDL_Quit();
	return 0;
}

