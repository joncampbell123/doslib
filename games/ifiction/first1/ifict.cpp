
#if defined(USE_WIN32)
# include <windows.h>
# include <mmsystem.h>
#endif

#if defined(USE_DOSLIB)
# include <hw/cpu/cpu.h>
# include <hw/dos/dos.h>
# include <hw/vga/vga.h>
# include <hw/vesa/vesa.h>
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

/* NTS: Do not assume the full 256-color palette, 256-color Windows uses 20 of them, leaving us with 236 of them.
 *      We *could* just render with 256 colors but of course that means some colors get combined, so, don't.
 *      Not a problem so much if using Windows GDI but if we're going to play with DirectX or the earlier hacky
 *      Windows 3.1 equivalents, we need to worry about that. */

static char	fatal_tmp[256];

#if defined(USE_SDL2)
SDL_Window*	sdl_window = NULL;
SDL_Surface*	sdl_window_surface = NULL;
SDL_Surface*	sdl_game_surface = NULL;
SDL_Palette*	sdl_game_palette = NULL;
SDL_Color	sdl_pal[256];
Uint32		sdl_ticks_base = 0; /* use Uint32 type provided by SDL2 here to avoid problems */
bool		sdl_signal_to_quit = false;
#endif

#if defined(USE_WIN32)
const char*	hwndMainClassName = "IFICTIONWIN32";
HINSTANCE	myInstance = NULL;
HWND		hwndMain = NULL;
bool		winQuit = false;
bool		win95 = false; /* Is Windows 95 or higher */
bool		winIsDestroying = false;
bool		winScreenIsPal = false;
BITMAPINFO*	hwndMainDIB = NULL;
HPALETTE	hwndMainPAL = NULL;
HPALETTE	hwndMainPALPrev = NULL;
PALETTEENTRY	win_pal[256];
unsigned char*	win_dib = NULL;
unsigned char*	win_dib_first_row = NULL;
int		win_dib_pitch = 0;
DWORD		win32_tick_base = 0;
#endif

#if defined(USE_DOSLIB)
unsigned char*	vesa_lfb = NULL; /* video memory, linear framebuffer */
unsigned char*	vesa_lfb_offscreen = NULL; /* system memory framebuffer, to copy to video memory */
uint32_t	vesa_lfb_physaddr = 0;
uint32_t	vesa_lfb_map_size = 0;
uint32_t	vesa_lfb_stride = 0;
uint16_t	vesa_lfb_height = 0;
uint16_t	vesa_lfb_width = 0;
bool		vesa_setmode = false;
bool		vesa_8bitpal = false; /* 8-bit DAC */
uint16_t	vesa_mode = 0;

uint32_t	pit_count = 0;
uint16_t	pit_prev = 0;

unsigned char	vesa_pal[256*4];

bool		dosbox_ig = false; /* DOSBox Integration Device detected */
#endif

#if defined(USE_DOSLIB)
void cpu_clear_TF(void) { /* EFLAGS bit 8, TF */
	__asm {
		push	eax
		pushfd
		pop	eax
		and	ah,0xFE
		push	eax
		popfd
		pop	eax
	}
}
#endif

#pragma pack(push,1)
struct IFEPaletteEntry {
	uint8_t	r,g,b;
};
#pragma pack(pop)

void IFEDBG(const char *msg,...);
void IFEFatalError(const char *msg,...);
void IFEUpdateFullScreen(void);
void IFECheckEvents(void);

/* WARNING: Will wrap around after 49 days. You're not playing this game that long, are you?
 *          Anyway to avoid Windows-style crashes at 49 days, call IFEResetTicks() with the
 *          return value of IFEGetTicks() to periodically count from zero. */
uint32_t IFEGetTicks(void) {
#if defined(USE_SDL2)
	return uint32_t(SDL_GetTicks() - sdl_ticks_base);
#elif defined(USE_WIN32)
	return uint32_t(timeGetTime() - win32_tick_base);
#elif defined(USE_DOSLIB)
	uint32_t w,p;
	{
		uint16_t pit_cur = read_8254(T8254_TIMER_INTERRUPT_TICK);
		pit_count += (uint32_t)((uint16_t)(pit_prev - pit_cur)); /* 8254 counts DOWN, not UP, typecast to ensure 16-bit rollover */
		pit_prev = pit_cur;

		/* convert ticks to milliseconds */
		w = pit_count / (uint32_t)T8254_REF_CLOCK_HZ;
		p = ((pit_count % (uint32_t)T8254_REF_CLOCK_HZ) * (uint32_t)1000ul) / (uint32_t)T8254_REF_CLOCK_HZ;
	}

	return (w * (uint32_t)1000ul) + p;
#endif
}

void IFEResetTicks(const uint32_t base) {
#if defined(USE_SDL2)
	sdl_ticks_base += base; /* NTS: Use return value of IFEGetTicks() */
#elif defined(USE_WIN32)
	win32_tick_base += base;
#elif defined(USE_DOSLIB)
	/* convert milliseconds back to timer ticks and adjust */
	pit_count -= (base / (uint32_t)1000ul) * (uint32_t)T8254_REF_CLOCK_HZ;
	pit_count -= ((base % (uint32_t)1000ul) * (uint32_t)T8254_REF_CLOCK_HZ) / (uint32_t)1000ul;
#endif
}

void IFEInitVideo(void) {
#if defined(USE_SDL2)
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
	IFECheckEvents();
#elif defined(USE_WIN32)
	if (hwndMain == NULL) {
		int sw,sh;

		sw = GetSystemMetrics(SM_CXSCREEN);
		sh = GetSystemMetrics(SM_CYSCREEN);

		{
			HDC hDC = GetDC(NULL); /* the screen */
			if ((GetDeviceCaps(hDC,RASTERCAPS) & RC_PALETTE) && (GetDeviceCaps(hDC,BITSPIXEL) == 8))
				winScreenIsPal = true;
			else
				winScreenIsPal = false;
			ReleaseDC(NULL,hDC);
		}

		{
			DWORD dwStyle = WS_OVERLAPPED|WS_SYSMENU|WS_CAPTION|WS_BORDER;
			RECT um;

			um.top = 0;
			um.left = 0;
			um.right = 640;
			um.bottom = 480;
			if (sw == 640 && sh == 480) {
				/* make it borderless, which is impossible if Windows sees a WS_OVERLAPPED style combo */
				dwStyle = WS_POPUP;
			}
			else {
				AdjustWindowRect(&um,dwStyle,FALSE);
			}

			hwndMain = CreateWindow(hwndMainClassName,"",dwStyle,
				CW_USEDEFAULT,CW_USEDEFAULT,um.right - um.left,um.bottom - um.top,
				NULL,NULL,
				myInstance,
				NULL);
		}

		if (hwndMain == NULL)
			IFEFatalError("CreateWindow failed");

		{
			int ww,wh;

			{
				RECT um = {0,0,0,0};
				GetWindowRect(hwndMain,&um);
				ww = (um.right - um.left);
				wh = (um.bottom - um.top);
			}
			SetWindowPos(hwndMain,NULL,(sw - ww) / 2,(sh - wh) / 2,0,0,SWP_NOSIZE|SWP_SHOWWINDOW);
		}

		hwndMainDIB = (BITMAPINFO*)calloc(1,sizeof(BITMAPINFOHEADER) + (256 * 2/*16-bit integers, DIB_PAL_COLORS*/) + 4096/*for good measure*/);
		if (hwndMainDIB == NULL)
			IFEFatalError("hwndMainDIB malloc fail");

		hwndMainDIB->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		hwndMainDIB->bmiHeader.biWidth = 640;
		if (win95)
			hwndMainDIB->bmiHeader.biHeight = -480; /* top down, Windows 95 */
		else
			hwndMainDIB->bmiHeader.biHeight = 480; /* bottom up, Windows 3.1 */
		hwndMainDIB->bmiHeader.biPlanes = 1;
		hwndMainDIB->bmiHeader.biBitCount = 8;
		hwndMainDIB->bmiHeader.biCompression = 0;
		hwndMainDIB->bmiHeader.biSizeImage = 640*480; /* NTS: pitch is width rounded to a multiple of 4 */
		hwndMainDIB->bmiHeader.biClrUsed = 256;
		hwndMainDIB->bmiHeader.biClrImportant = 256;
		if (winScreenIsPal) { /* map 1:1 to logical palette */
			uint16_t *p = (uint16_t*)( &(hwndMainDIB->bmiColors[0]) );
			LOGPALETTE* pal;
			unsigned int i;

			for (i=0;i < 256;i++) p[i] = (uint16_t)i;

			pal = (LOGPALETTE*)calloc(1,sizeof(LOGPALETTE) + (sizeof(PALETTEENTRY) * 256) + 4096);
			if (pal == NULL) IFEFatalError("LOGPALETTE malloc fail");

			pal->palVersion = 0x300;
			pal->palNumEntries = 256;
			for (i=0;i < 256;i++) {
				pal->palPalEntry[i].peRed = (unsigned char)i;
				pal->palPalEntry[i].peGreen = (unsigned char)i;
				pal->palPalEntry[i].peBlue = (unsigned char)i;
				pal->palPalEntry[i].peFlags = 0;
			}

			hwndMainPAL = CreatePalette(pal);
			if (hwndMainPAL == NULL)
				IFEFatalError("CreatePalette fail");

			free((void*)pal);
		}
		else {
			hwndMainPALPrev = NULL;
			hwndMainPAL = NULL;
		}

		win_dib = (unsigned char*)malloc(hwndMainDIB->bmiHeader.biSizeImage);
		if (win_dib == NULL)
			IFEFatalError("win_dib malloc fail");

		/* NTS: Windows 3.1 with Win32s does not provide any function to draw top-down DIBs.
		 *      Windows 95 however does support top down DIBs, so this code should consider auto-detecting that.
		 *      To simplify other code, pitch and first row are computed here. */
		if ((int)(hwndMainDIB->bmiHeader.biHeight) < 0) {
			win_dib_first_row = win_dib;
			win_dib_pitch = (int)hwndMainDIB->bmiHeader.biWidth;
		}
		else {
			win_dib_first_row = win_dib + (hwndMainDIB->bmiHeader.biSizeImage - hwndMainDIB->bmiHeader.biWidth); /* FIXME: What about future code for != 8bpp? Also alignment. */
			win_dib_pitch = -((int)hwndMainDIB->bmiHeader.biWidth);
		}

		IFEUpdateFullScreen();
		IFECheckEvents();
	}
#elif defined(USE_DOSLIB)
	/* Find 640x480 256-color mode.
	 * Linear framebuffer required (we'll support older bank switched stuff later) */
	{
		const uint32_t wantf1 = VESA_MODE_ATTR_HW_SUPPORTED | VESA_MODE_ATTR_GRAPHICS_MODE | VESA_MODE_ATTR_LINEAR_FRAMEBUFFER_AVAILABLE;
		struct vbe_mode_info mi = {0};
		uint16_t found_mode = 0;
		unsigned int entry;
		uint16_t mode;

		for (entry=0;entry < 4096;entry++) {
			mode = vbe_read_mode_entry(vbe_info->video_mode_ptr,entry);
			if (mode == 0xFFFF) break;

			if (vbe_read_mode_info(mode,&mi)) {
				vbe_fill_in_mode_info(mode,&mi);
				if ((mi.mode_attributes & wantf1) == wantf1 && mi.x_resolution == 640 && mi.y_resolution == 480 &&
					mi.bits_per_pixel == 8 && mi.memory_model == 0x04/*packed pixel*/ &&
					mi.phys_base_ptr != 0x00000000ul && mi.phys_base_ptr != 0xFFFFFFFFul &&
					mi.bytes_per_scan_line >= 640) {
					vesa_lfb_physaddr = mi.phys_base_ptr;
					vesa_lfb_map_size = mi.bytes_per_scan_line * mi.y_resolution;
					vesa_lfb_stride = mi.bytes_per_scan_line;
					vesa_lfb_width = mi.x_resolution;
					vesa_lfb_height = mi.y_resolution;
					found_mode = mode;
					break;
				}
			}
		}

		if (found_mode == 0)
			IFEFatalError("VESA BIOS video mode (640 x 480 256-color) not found");

		vesa_mode = found_mode;
	}

	if (!vesa_setmode) {
		/* Set the video mode */
		if (!vbe_set_mode((uint16_t)(vesa_mode | VBE_MODE_LINEAR),NULL))
			IFEFatalError("Unable to set VESA video mode");

		/* we set the mode, set the flag so FatalError can unset it properly */
		vesa_setmode = true;

		/* use 8-bit DAC if available */
		if (vbe_info->capabilities & VBE_CAP_8BIT_DAC) {
			if (vbe_set_dac_width(8) == 8)
				vesa_8bitpal = true;
		}

		/* As a 32-bit DOS program atop DPMI we cannot assume a 1:1 mapping between linear and physical,
		 * though plenty of DOS extenders will do just that if EMM386.EXE is not loaded */
		vesa_lfb = (unsigned char*)dpmi_phys_addr_map(vesa_lfb_physaddr,vesa_lfb_map_size);
		if (vesa_lfb == NULL)
			IFEFatalError("DPMI VESA LFB map fail");

		/* video memory is slow, and unlike Windows and SDL2, vesa_lfb points directly at video memory.
		 * Provide an offscreen copy of video memory to work from for compositing. */
		vesa_lfb_offscreen = (unsigned char*)malloc(vesa_lfb_map_size);
		if (vesa_lfb_offscreen == NULL)
			IFEFatalError("DPMI VESA LFB shadow malloc fail");
	}
#endif
}

void IFEShutdownVideo(void) {
#if defined(USE_SDL2)
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
#elif defined(USE_WIN32)
	if (hwndMainPAL != NULL) {
		DeleteObject((HGDIOBJ)hwndMainPAL);
		hwndMainPALPrev = NULL;
		hwndMainPAL = NULL;
	}
	if (hwndMainDIB != NULL) {
		free((void*)hwndMainDIB);
		hwndMainDIB = NULL;
	}
	if (hwndMain != NULL) {
		winIsDestroying = true;
		DestroyWindow(hwndMain);
		winIsDestroying = false;
		hwndMain = NULL;
	}
	if (win_dib != NULL) {
		free((void*)win_dib);
		win_dib = NULL;
	}
#elif defined(USE_DOSLIB)
	_sti();
	if (vesa_lfb_offscreen != NULL) {
		free((void*)vesa_lfb_offscreen);
		vesa_lfb_offscreen = NULL;
	}
	if (vesa_lfb != NULL) {
		dpmi_phys_addr_free((void*)vesa_lfb);
		vesa_lfb = NULL;
	}
	if (vesa_setmode) {
		vesa_setmode = false;
		int10_setmode(3); /* back to 80x25 text */
	}
#endif
}

void IFESetPaletteColors(const unsigned int first,const unsigned int count,IFEPaletteEntry *pal) {
	unsigned int i;

	if (first >= 256u || count > 256u || (first+count) > 256u)
		IFEFatalError("SetPaletteColors out of range first=%u count=%u",first,count);

#if defined(USE_SDL2)
	for (i=0;i < count;i++) {
		sdl_pal[i+first].r = pal[i].r;
		sdl_pal[i+first].g = pal[i].g;
		sdl_pal[i+first].b = pal[i].b;
		sdl_pal[i+first].a = 0xFFu;
	}

	if (SDL_SetPaletteColors(sdl_game_palette,sdl_pal,first,count) != 0)
		IFEFatalError("SDL2 game palette set colors");
#elif defined(USE_WIN32)
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
#elif defined(USE_DOSLIB)
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
#endif
}

void IFEUpdateFullScreen(void) {
#if defined(USE_SDL2)
	if (SDL_BlitSurface(sdl_game_surface,NULL,sdl_window_surface,NULL) != 0)
		IFEFatalError("Game to window BlitSurface");

	if (SDL_UpdateWindowSurface(sdl_window) != 0)
		IFEFatalError("Window surface update");
#elif defined(USE_WIN32)
	{
		HDC hDC = GetDC(hwndMain);
		HPALETTE oldPal = SelectPalette(hDC,hwndMainPAL,FALSE);

		SetDIBitsToDevice(hDC,
			/*dest x/y*/0,0,
			abs((int)hwndMainDIB->bmiHeader.biWidth),
			abs((int)hwndMainDIB->bmiHeader.biHeight),
			/*src x/y*/0,0,
			/*starting scan/clines*/0,abs((int)hwndMainDIB->bmiHeader.biHeight),
			win_dib,
			hwndMainDIB,
			winScreenIsPal ? DIB_PAL_COLORS : DIB_RGB_COLORS);

		SelectPalette(hDC,oldPal,FALSE);
		ReleaseDC(hwndMain,hDC);
	}
#elif defined(USE_DOSLIB)
	memcpy(vesa_lfb,vesa_lfb_offscreen,vesa_lfb_map_size);
#endif
}

void IFETestRGBPalette() {
	IFEPaletteEntry pal[256];
	unsigned int i;

	for (i=0;i < 64;i++) {
		pal[i].r = (unsigned char)(i*4u);
		pal[i].g = (unsigned char)(i*4u);
		pal[i].b = (unsigned char)(i*4u);

		pal[i+64u].r = (unsigned char)(i*4u);
		pal[i+64u].g = 0u;
		pal[i+64u].b = 0u;

		pal[i+128u].r = 0u;
		pal[i+128u].g = (unsigned char)(i*4u);
		pal[i+128u].b = 0u;

		pal[i+192u].r = 0u;
		pal[i+192u].g = 0u;
		pal[i+192u].b = (unsigned char)(i*4u);
	}

	IFESetPaletteColors(0,256,pal);
}

bool IFEUserWantsToQuit(void) {
#if defined(USE_SDL2)
	return sdl_signal_to_quit;
#elif defined(USE_WIN32)
	return winQuit;
#else
	return false;
#endif
}

/* WARNING: Pitch is signed only because Windows 3.1 does not allow top-down DIBs */
int IFEScreenDrawPitch(void) {
#if defined(USE_SDL2)
	return (int)(sdl_game_surface->pitch);
#elif defined(USE_WIN32)
	return win_dib_pitch;
#elif defined(USE_DOSLIB)
	return vesa_lfb_stride;
#endif
}

/* check your pointers! */
bool IFEScreenDrawPointerRangeCheck(unsigned char *p) {
#if defined(USE_SDL2)
	return (p >= sdl_game_surface->pixels && p < ((unsigned char*)sdl_game_surface->pixels + (sdl_game_surface->pitch * sdl_game_surface->h)));
#elif defined(USE_WIN32)
	return (p >= win_dib && p < (win_dib + hwndMainDIB->bmiHeader.biSizeImage));
#elif defined(USE_DOSLIB)
	return (p >= vesa_lfb && p < (vesa_lfb + vesa_lfb_map_size));
#endif
}

/* Point to first row. You need pitch in display order too, which might be a negative number.
 * The only reason for this damn code is Windows 3.1 which provides no way whatsoever to draw top down DIBs */
unsigned char *IFEScreenDrawPointer(void) {
#if defined(USE_SDL2)
	return (unsigned char*)(sdl_game_surface->pixels);
#elif defined(USE_WIN32)
	return win_dib_first_row;
#elif defined(USE_DOSLIB)
	return vesa_lfb_offscreen;
#endif
}

unsigned int IFEScreenWidth(void) {
#if defined(USE_SDL2)
	return (unsigned int)(sdl_game_surface->w);
#elif defined(USE_WIN32)
	return (unsigned int)abs((int)hwndMainDIB->bmiHeader.biWidth);
#elif defined(USE_DOSLIB)
	return vesa_lfb_width;
#endif
}

unsigned int IFEScreenHeight(void) {
#if defined(USE_SDL2)
	return (unsigned int)(sdl_game_surface->h);
#elif defined(USE_WIN32)
	return (unsigned int)abs((int)hwndMainDIB->bmiHeader.biHeight);
#elif defined(USE_DOSLIB)
	return vesa_lfb_height;
#endif
}

bool IFEBeginScreenDraw(void) {
#if defined(USE_SDL2)
	if (SDL_MUSTLOCK(sdl_game_surface) && SDL_LockSurface(sdl_game_surface) != 0)
		return false;
	if (sdl_game_surface->pixels == NULL)
		IFEFatalError("SDL2 game surface pixels == NULL"); /* that's a BUG if this happens! */
	if (sdl_game_surface->pitch < 0)
		IFEFatalError("SDL2 game surface pitch is negative");

	return true;
#elif defined(USE_WIN32)
	return true; // nothing to do
#elif defined(USE_DOSLIB)
	return true; // nothing to do
#endif
}

void IFEEndScreenDraw(void) {
#if defined(USE_SDL2)
	if (SDL_MUSTLOCK(sdl_game_surface))
		SDL_UnlockSurface(sdl_game_surface);
#elif defined(USE_WIN32)
	// nothing to do
#elif defined(USE_DOSLIB)
	// nothing to do
#endif
}

#if defined(USE_SDL2)
void IFEProcessEvent(SDL_Event &ev) {
	if (ev.type == SDL_QUIT) {
		sdl_signal_to_quit = true;
	}
}
#endif

void IFECheckEvents(void) {
#if defined(USE_SDL2)
	SDL_Event ev;

	if (SDL_PollEvent(&ev))
		IFEProcessEvent(ev);
#elif defined(USE_WIN32)
	MSG msg;

	if (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
#endif
}

void IFEWaitEvent(const int wait_ms) {
#if defined(USE_SDL2)
	SDL_Event ev;

	if (SDL_WaitEventTimeout(&ev,wait_ms))
		IFEProcessEvent(ev);
#elif defined(USE_WIN32)
	MSG msg;

	(void)wait_ms;

	if (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
#else
	(void)wait_ms; // TODO
#endif
}

void IFETestRGBPalettePattern(void) {
	unsigned char *firstrow;
	unsigned int x,y,w,h;
	int pitch;

	if (!IFEBeginScreenDraw())
		IFEFatalError("BeginScreenDraw TestRGBPalettePattern");
	if ((firstrow=IFEScreenDrawPointer()) == NULL)
		IFEFatalError("ScreenDrawPointer==NULL TestRGBPalettePattern");

	w = IFEScreenWidth();
	h = IFEScreenHeight();
	pitch = IFEScreenDrawPitch();
	for (y=0;y < h;y++) {
		unsigned char *row = firstrow + ((int)y * pitch);
		for (x=0;x < w;x++) {
			if ((x & 0xF0u) != 0xF0u)
				row[x] = (unsigned char)(y & 0xFFu);
			else
				row[x] = 0;
		}
	}

	IFEEndScreenDraw();
	IFEUpdateFullScreen();
}

void IFEDBG(const char *msg,...) {
#if defined(USE_DOSLIB)
	if (dosbox_ig) {
		va_list va;

		va_start(va,msg);
		vsnprintf(fatal_tmp,sizeof(fatal_tmp)/*includes NUL byte*/,msg,va);
		va_end(va);
	}
	else {
		fatal_tmp[0] = 0;
	}
#else
	(void)msg;
#endif

#if defined(USE_DOSLIB)
	if (dosbox_ig) {
		dosbox_id_debug_message("IFEDBG: ");
		dosbox_id_debug_message(fatal_tmp);
		dosbox_id_debug_message("\n");
	}
#endif
}

void IFEFatalError(const char *msg,...) {
	va_list va;

	va_start(va,msg);
	vsnprintf(fatal_tmp,sizeof(fatal_tmp)/*includes NUL byte*/,msg,va);
	va_end(va);

	IFEShutdownVideo();

#if defined(USE_WIN32)
	MessageBox(NULL/*FIXME*/,fatal_tmp,"Fatal error",MB_OK|MB_ICONEXCLAMATION);
#else
	fprintf(stderr,"Fatal error: %s\n",fatal_tmp);
#endif
	exit(127);
}

void IFENormalExit(void) {
	IFEShutdownVideo();
	exit(0);
}

#if defined(USE_WIN32)
LRESULT CALLBACK hwndMainProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam) {
	switch (uMsg) {
		case WM_CREATE:
			break;
		case WM_QUIT:
			winQuit = true;
			break;
		case WM_DESTROY:
			if (!winIsDestroying)
				winQuit = true;
			break;
		case WM_QUERYNEWPALETTE:
			if (winScreenIsPal && hwndMainPAL != NULL) {
				HPALETTE oldPal;
				HDC hDC;

				hDC = GetDC(hwnd);
				oldPal = SelectPalette(hDC,hwndMainPAL,FALSE);
				RealizePalette(hDC);
				SelectPalette(hDC,oldPal,FALSE);
				ReleaseDC(hwnd,hDC);
				InvalidateRect(hwnd,NULL,FALSE);
				return TRUE;
			}
			break;
		case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC hDC = BeginPaint(hwnd,&ps);

				if (winScreenIsPal && hwndMainPAL != NULL) {
					HPALETTE oldPal;

					oldPal = SelectPalette(hDC,hwndMainPAL,FALSE);
					RealizePalette(hDC);
					SelectPalette(hDC,oldPal,FALSE);
				}

				EndPaint(hwnd,&ps);
				IFEUpdateFullScreen();
			}
			break;
		case WM_ACTIVATE:
			break;
		default:
			return DefWindowProc(hwnd,uMsg,wParam,lParam);
	}

	return 0;
}
#endif

#if defined(USE_WIN32)
int PASCAL WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance/*doesn't mean anything in Win32*/,LPSTR lpCmdLine,int nCmdShow) {
	//not used yet
	(void)hInstance;
	(void)hPrevInstance;
	(void)lpCmdLine;
	(void)nCmdShow;

	myInstance = hInstance;

	/* some performance and rendering improvements are possible if Windows 95 (aka Windows 4.0) or higher */
	if ((GetVersion()&0xFF) >= 4)
		win95 = true;
	else
		win95 = false;

	if (!hPrevInstance) {
		WNDCLASS wnd;

		memset(&wnd,0,sizeof(wnd));
		wnd.style = CS_HREDRAW|CS_VREDRAW;
		wnd.lpfnWndProc = hwndMainProc;
		wnd.hInstance = hInstance;
		wnd.lpszClassName = hwndMainClassName;

		if (!RegisterClass(&wnd)) {
			MessageBox(NULL,"RegisterClass failed","",MB_OK|MB_ICONEXCLAMATION);
			return 1;
		}
	}
#else
int main(int argc,char **argv) {
	//not used yet
	(void)argc;
	(void)argv;

# if defined(USE_DOSLIB)
	cpu_probe();
	probe_dos();
	detect_windows();
	probe_dpmi();
	probe_vcpi();

	if (!probe_8254())
		IFEFatalError("8254 timer not detected");
	if (!probe_8259())
		IFEFatalError("8259 interrupt controller not detected");
	if (!k8042_probe())
		IFEFatalError("8042 keyboard controller not found");
	if (!probe_vga())
		IFEFatalError("Unable to detect video card");
	if (!(vga_state.vga_flags & VGA_IS_VGA))
		IFEFatalError("Standard VGA not detected");
	if (!vbe_probe() || vbe_info == NULL || vbe_info->video_mode_ptr == 0)
		IFEFatalError("VESA BIOS extensions not detected");

	if (probe_dosbox_id()) {
		printf("DOSBox Integration Device detected\n");
		dosbox_ig = true;

		IFEDBG("Using DOSBox Integration Device for debug info. This should appear in your DOSBox/DOSBox-X log file");
	}

	/* make sure the timer is ticking at 18.2Hz. */
	write_8254_system_timer(0);

	/* establish the base line timer tick */
	pit_prev = read_8254(T8254_TIMER_INTERRUPT_TICK);

	/* Windows 95 bug: After reading the 8254, the TF flag (Trap Flag) is stuck on, and this
	 * program runs extremely slowly. Clear the TF flag. It may have something to do with
	 * read_8254 or any other code that does PUSHF + CLI + POPF to protect against interrupts.
	 * POPF is known to not cause a fault on 386-level IOPL3 protected mode, and therefore
	 * a VM monitor has difficulty knowing whether interrupts are enabled, so perhaps setting
	 * TF when the VM executes the CLI instruction is Microsoft's hack to try to work with
	 * DOS games regardless. */
	cpu_clear_TF();
# endif // DOSLIB
#endif

	IFEInitVideo();

	IFEResetTicks(IFEGetTicks());
	while (IFEGetTicks() < 1000) {
		if (IFEUserWantsToQuit()) IFENormalExit();
		IFEWaitEvent(100);
	}

	IFETestRGBPalette();
	IFETestRGBPalettePattern();
	IFEResetTicks(IFEGetTicks());
	while (IFEGetTicks() < 3000) {
		if (IFEUserWantsToQuit()) IFENormalExit();
		IFEWaitEvent(100);
	}

	IFENormalExit();
	return 0;
}

