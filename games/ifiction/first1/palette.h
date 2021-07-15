
#include <stdint.h>

#pragma pack(push,1)
struct IFEPaletteEntry {
	uint8_t	r,g,b;
};
#pragma pack(pop)

void IFESetPaletteColors(const unsigned int first,const unsigned int count,IFEPaletteEntry *pal);

typedef void ifefunc_SetPaletteColors_t(const unsigned int first,const unsigned int count,IFEPaletteEntry *pal);

extern ifefunc_SetPaletteColors_t *ifefunc_SetPaletteColors;

#if defined(USE_SDL2)
extern ifefunc_SetPaletteColors_t ifefunc_SetPaletteColors_sdl2;
# define ifefunc_SetPaletteColors_default ifefunc_SetPaletteColors_sdl2
#endif

#if defined(USE_WIN32)
extern ifefunc_SetPaletteColors_t ifefunc_SetPaletteColors_win32;
# define ifefunc_SetPaletteColors_default ifefunc_SetPaletteColors_win32
#endif

#if defined(USE_DOSLIB)
extern ifefunc_SetPaletteColors_t ifefunc_SetPaletteColors_doslib;
# define ifefunc_SetPaletteColors_default ifefunc_SetPaletteColors_doslib
#endif


