
#if defined(USE_WIN32)
# include <windows.h>
# include <mmsystem.h>
#endif

#if defined(USE_DOSLIB)
# include <hw/cpu/cpu.h>
# include <hw/dos/dos.h>
# if defined(TARGET_PC98)
#  error PC-98 target removed
// REMOVED
# else
#  include <hw/vga/vga.h>
#  include <hw/vesa/vesa.h>
# endif
# include <hw/8254/8254.h>
# include <hw/8259/8259.h>
# include <hw/8042/8042.h>
# include <hw/dos/doswin.h>
# include <hw/dosboxid/iglib.h> /* for debugging */
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#if defined(USE_SDL2)
# if defined(__APPLE__) /* Brew got the headers wrong here */
#  include <SDL.h>
# else
#  include <SDL2/SDL.h>
# endif
#endif

#include "utils.h"
#include "debug.h"
#include "palette.h"

#if defined(USE_SDL2)
extern SDL_Color	sdl_pal[256];
extern SDL_Palette*	sdl_game_palette;

void IFESetPaletteColors(const unsigned int first,const unsigned int count,IFEPaletteEntry *pal) {
	unsigned int i;

	IFESETPALETTEASSERT(first,count);

	for (i=0;i < count;i++) {
		sdl_pal[i+first].r = pal[i].r;
		sdl_pal[i+first].g = pal[i].g;
		sdl_pal[i+first].b = pal[i].b;
		sdl_pal[i+first].a = 0xFFu;
	}

	if (SDL_SetPaletteColors(sdl_game_palette,sdl_pal,first,count) != 0)
		IFEFatalError("SDL2 game palette set colors");
}
#endif

#if defined(USE_WIN32)
extern bool		winScreenIsPal;
extern PALETTEENTRY	win_pal[256];
extern BITMAPINFO*	hwndMainDIB;
extern HPALETTE		hwndMainPAL;
extern HWND		hwndMain;

void IFESetPaletteColors(const unsigned int first,const unsigned int count,IFEPaletteEntry *pal) {
	unsigned int i;

	IFESETPALETTEASSERT(first,count);

	if (winScreenIsPal) {
		for (i=0;i < count;i++) {
			win_pal[i].peRed = pal[i].r;
			win_pal[i].peGreen = pal[i].g;
			win_pal[i].peBlue = pal[i].b;
			win_pal[i].peFlags = 0;
		}

		SetPaletteEntries(hwndMainPAL,first,count,win_pal);

		{
			HPALETTE oldPal;
			HDC hDC;

			hDC = GetDC(hwndMain);
			oldPal = SelectPalette(hDC,hwndMainPAL,FALSE);
			RealizePalette(hDC);
			SelectPalette(hDC,oldPal,FALSE);
			ReleaseDC(hwndMain,hDC);
			InvalidateRect(hwndMain,NULL,FALSE);
		}
	}
	else {
		for (i=0;i < count;i++) {
			hwndMainDIB->bmiColors[i+first].rgbRed = pal[i].r;
			hwndMainDIB->bmiColors[i+first].rgbGreen = pal[i].g;
			hwndMainDIB->bmiColors[i+first].rgbBlue = pal[i].b;
		}
	}
}
#endif

#if defined(USE_DOSLIB)
extern bool		vesa_8bitpal; /* 8-bit DAC */
extern unsigned char	vesa_pal[256*4];

void IFESetPaletteColors(const unsigned int first,const unsigned int count,IFEPaletteEntry *pal) {
	unsigned int i;

	IFESETPALETTEASSERT(first,count);

	/* IBM PC/AT */
	if (vesa_8bitpal) {
		for (i=0;i < count;i++) {
			/* VGA 8-bit RGB */
			vesa_pal[i*4u + 0u] = (unsigned char)(pal[i].r);
			vesa_pal[i*4u + 1u] = (unsigned char)(pal[i].g);
			vesa_pal[i*4u + 2u] = (unsigned char)(pal[i].b);
		}
	}
	else {
		for (i=0;i < count;i++) {
			/* VGA 6-bit RGB */
			vesa_pal[i*4u + 0u] = (unsigned char)(pal[i].r >> 2u);
			vesa_pal[i*4u + 1u] = (unsigned char)(pal[i].g >> 2u);
			vesa_pal[i*4u + 2u] = (unsigned char)(pal[i].b >> 2u);
		}
	}

	vesa_set_palette_data(first,count,(char*)vesa_pal);
}
#endif

