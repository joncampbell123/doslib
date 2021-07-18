
#if defined(USE_SDL2)
#include <sys/types.h>
#include <sys/stat.h>
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

#include <vector>

SDL_Window*			sdl_window = NULL;
SDL_Surface*			sdl_window_surface = NULL;
SDL_Surface*			sdl_game_surface = NULL;
SDL_Palette*			sdl_game_palette = NULL;
SDL_Color			sdl_pal[256];
Uint32				sdl_ticks_base = 0; /* use Uint32 type provided by SDL2 here to avoid problems */
bool				sdl_signal_to_quit = false;
ifevidinfo_t			ifevidinfo_sdl2;
IFEMouseStatus			ifemousestat;
bool				mousecap_on = false;

static const SDL_TouchID	no_touch_id = (SDL_TouchID)(0xFFFFFFFFul);
static const SDL_FingerID	no_finger_id = (SDL_FingerID)(0xFFFFFFFFul);

/* SDL2 supports touchscreen devices. Allow touchscreen input to fake mouse input */
SDL_FingerID			touchscreen_finger_lock = no_finger_id;
SDL_TouchID			touchscreen_touch_lock = no_touch_id;

/* update region management */
unsigned int			upd_ystep = 8;
std::vector<SDL_Rect>		upd_yspan;

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
	size_t i;

	if (SDL_BlitSurface(sdl_game_surface,NULL,sdl_window_surface,NULL) != 0)
		IFEFatalError("Game to window BlitSurface");

	if (SDL_UpdateWindowSurface(sdl_window) != 0)
		IFEFatalError("Window surface update");

	/* clear update region list */
	for (i=0;i < upd_yspan.size();i++)
		upd_yspan[i].x = upd_yspan[i].w = upd_yspan[i].h = 0;
}

static ifevidinfo_t* p_GetVidInfo(void) {
	return &ifevidinfo_sdl2;
}

static bool p_UserWantsToQuit(void) {
	return sdl_signal_to_quit;
}

static void SDLKeySymToFill(IFEKeyEvent &ke,const SDL_KeyboardEvent &ev) {
#define MAP(x,y) \
	case x: ke.code = (uint32_t)y; break;
#define MSN(x) \
	case SDL_SCANCODE_##x: ke.code = (uint32_t)(IFEKEY_##x); break;

	switch (ev.keysym.scancode) {
		MSN(RETURN);
		MSN(ESCAPE);
		MSN(BACKSPACE);
		MSN(TAB);
		MSN(SPACE);
		MSN(A);
		MSN(B);
		MSN(C);
		MSN(D);
		MSN(E);
		MSN(F);
		MSN(G);
		MSN(H);
		MSN(I);
		MSN(J);
		MSN(K);
		MSN(L);
		MSN(M);
		MSN(N);
		MSN(O);
		MSN(P);
		MSN(Q);
		MSN(R);
		MSN(S);
		MSN(T);
		MSN(U);
		MSN(V);
		MSN(W);
		MSN(X);
		MSN(Y);
		MSN(Z);
		MSN(0);
		MSN(1);
		MSN(2);
		MSN(3);
		MSN(4);
		MSN(5);
		MSN(6);
		MSN(7);
		MSN(8);
		MSN(9);
		MSN(COMMA);
		MSN(PERIOD);
		default: break;
	}
#undef MSN
#undef MAP
}

static void priv_ProcessKeyboardEvent(const SDL_KeyboardEvent &ev) {
	IFEKeyEvent ke;

	memset(&ke,0,sizeof(ke));
	ke.raw_code = (uint32_t)ev.keysym.scancode;
	ke.flags = (ev.state == SDL_PRESSED) ? IFEKeyEvent_FLAG_DOWN : 0;
	SDLKeySymToFill(ke,ev);

	if (ev.keysym.mod & KMOD_LSHIFT)
		ke.flags |= IFEKeyEvent_FLAG_LSHIFT;
	if (ev.keysym.mod & KMOD_RSHIFT)
		ke.flags |= IFEKeyEvent_FLAG_RSHIFT;
	if (ev.keysym.mod & KMOD_LCTRL)
		ke.flags |= IFEKeyEvent_FLAG_LCTRL;
	if (ev.keysym.mod & KMOD_RCTRL)
		ke.flags |= IFEKeyEvent_FLAG_RCTRL;
	if (ev.keysym.mod & KMOD_LALT)
		ke.flags |= IFEKeyEvent_FLAG_LALT;
	if (ev.keysym.mod & KMOD_RALT)
		ke.flags |= IFEKeyEvent_FLAG_RALT;
	if (ev.keysym.mod & KMOD_NUM)
		ke.flags |= IFEKeyEvent_FLAG_NUMLOCK;
	if (ev.keysym.mod & KMOD_CAPS)
		ke.flags |= IFEKeyEvent_FLAG_CAPS;

	if (!IFEKeyQueue.add(ke))
		IFEDBG("ProcessKeyboardEvent: Queue full");

	IFEKeyboardProcessRawToCooked(ke);
}

void priv_ProcessMouseMotion(const SDL_MouseMotionEvent &ev) {
	/* ignore fake touchscreen mouse motion, in favor of touch screen events, and if not faking from mouse motion */
	if (ev.which == SDL_TOUCH_MOUSEID || ev.which == touchscreen_touch_lock || touchscreen_finger_lock != no_finger_id || touchscreen_touch_lock != no_touch_id) return;

	ifemousestat.x = ev.x;
	ifemousestat.y = ev.y;

	IFEDBG("Mouse move x=%d y=%d status=%08x which=%08x",
		ifemousestat.x,
		ifemousestat.y,
		(unsigned int)ifemousestat.status,
		(unsigned int)ev.which);
}

void priv_CaptureMouse(const bool cap) {
	if (mousecap_on != cap) {
		SDL_CaptureMouse((SDL_bool)cap);
		mousecap_on = cap;
	}
}

void priv_ProcessMouseButton(const SDL_MouseButtonEvent &ev) {
	IFEMouseEvent me;

	/* ignore fake touchscreen mouse motion, in favor of touch screen events */
	if (ev.which == SDL_TOUCH_MOUSEID || ev.which == touchscreen_touch_lock || touchscreen_finger_lock != no_finger_id || touchscreen_touch_lock != no_touch_id) return;

	memset(&me,0,sizeof(me));
	me.pstatus = ifemousestat.status;

	ifemousestat.x = ev.x;
	ifemousestat.y = ev.y;

	if (ev.button == SDL_BUTTON_LEFT) {
		if (ev.state == SDL_PRESSED)
			ifemousestat.status |= IFEMouseStatus_LBUTTON;
		else if (ev.state == SDL_RELEASED)
			ifemousestat.status &= ~IFEMouseStatus_LBUTTON;
	}
	else if (ev.button == SDL_BUTTON_MIDDLE) {
		if (ev.state == SDL_PRESSED)
			ifemousestat.status |= IFEMouseStatus_MBUTTON;
		else if (ev.state == SDL_RELEASED)
			ifemousestat.status &= ~IFEMouseStatus_MBUTTON;
	}
	else if (ev.button == SDL_BUTTON_RIGHT) {
		if (ev.state == SDL_PRESSED)
			ifemousestat.status |= IFEMouseStatus_RBUTTON;
		else if (ev.state == SDL_RELEASED)
			ifemousestat.status &= ~IFEMouseStatus_RBUTTON;
	}

	if (ifemousestat.status & (IFEMouseStatus_LBUTTON|IFEMouseStatus_MBUTTON|IFEMouseStatus_RBUTTON))
		priv_CaptureMouse(true);
	else
		priv_CaptureMouse(false);

	me.x = ifemousestat.x;
	me.y = ifemousestat.y;
	me.status = ifemousestat.status;

	IFEDBG("Mouse event x=%d y=%d pstatus=%08x status=%08x which=%08x",
		ifemousestat.x,
		ifemousestat.y,
		(unsigned int)me.pstatus,
		(unsigned int)me.status,
		(unsigned int)ev.which);

	if (!IFEMouseQueue.add(me))
		IFEDBG("Mouse event overrun");
}

static void UpdateMousePosFromTouchFinger(const SDL_TouchFingerEvent &ev) {
	int x = (int)(ev.x * ifevidinfo_sdl2.width);
	int y = (int)(ev.y * ifevidinfo_sdl2.height);

	if (x < 0) x = 0;
	if (y < 0) y = 0;
	if (x > (int)(ifevidinfo_sdl2.width-1)) x = (int)(ifevidinfo_sdl2.width-1);
	if (y > (int)(ifevidinfo_sdl2.height-1)) y = (int)(ifevidinfo_sdl2.height-1);

	ifemousestat.x = x;
	ifemousestat.y = y;

	IFEDBG("Touch mouse move x=%d y=%d status=%08x finger=%08x touch=%08x",
		ifemousestat.x,
		ifemousestat.y,
		(unsigned int)ifemousestat.status,
		(unsigned int)ev.fingerId,
		(unsigned int)ev.touchId);
}

static void priv_ProcessTouchscreenFinger(const SDL_TouchFingerEvent &ev) {
	if (ev.type == SDL_FINGERDOWN) {
		/* if finger touch, no current finger touch registered, and none of the mouse buttons are down */
		if (touchscreen_finger_lock == no_finger_id && touchscreen_touch_lock == no_touch_id &&
			(ifemousestat.status & (IFEMouseStatus_LBUTTON|IFEMouseStatus_MBUTTON|IFEMouseStatus_RBUTTON)) == 0) {
			ifemousestat.status |= IFEMouseStatus_LBUTTON;
			touchscreen_finger_lock = ev.fingerId;
			touchscreen_touch_lock = ev.touchId;
			UpdateMousePosFromTouchFinger(ev);
		}
	}
	else if (ev.type == SDL_FINGERUP) {
		if (touchscreen_finger_lock == ev.fingerId && touchscreen_touch_lock == ev.touchId) {
			ifemousestat.status &= ~IFEMouseStatus_LBUTTON;
			touchscreen_finger_lock = no_finger_id;
			touchscreen_touch_lock = no_touch_id;
			UpdateMousePosFromTouchFinger(ev);
		}
	}
	else if (ev.type == SDL_FINGERMOTION) {
		if (touchscreen_finger_lock == ev.fingerId && touchscreen_touch_lock == ev.touchId) {
			UpdateMousePosFromTouchFinger(ev);
		}
	}
}

static void priv_ProcessEvent(const SDL_Event &ev) {
	switch (ev.type) {
		case SDL_QUIT:
			sdl_signal_to_quit = true;
			break;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			priv_ProcessKeyboardEvent(ev.key);
			break;
		case SDL_MOUSEMOTION:
			priv_ProcessMouseMotion(ev.motion);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			priv_ProcessMouseButton(ev.button);
			break;
		case SDL_FINGERDOWN:
		case SDL_FINGERUP:
		case SDL_FINGERMOTION:
			priv_ProcessTouchscreenFinger(ev.tfinger);
			break;
		default:
			break;
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
	priv_CaptureMouse(false);
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
	upd_yspan.clear();
	SDL_Quit();
}

static void p_InitVideo(void) {
	size_t i;

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		IFEFatalError("SDL2 failed to initialize");

	mousecap_on = false;
	touchscreen_touch_lock = no_touch_id;
	touchscreen_finger_lock = no_finger_id;
	memset(&ifevidinfo_sdl2,0,sizeof(ifevidinfo_sdl2));
	memset(&ifemousestat,0,sizeof(ifemousestat));

	ifevidinfo_sdl2.width = 640;
	ifevidinfo_sdl2.height = 480;

	upd_yspan.resize((ifevidinfo_sdl2.height + upd_ystep - 1) / upd_ystep);
	for (i=0;i < upd_yspan.size();i++) {
		upd_yspan[i].x = upd_yspan[i].w = upd_yspan[i].h = 0;
		upd_yspan[i].y = (int)(i * upd_ystep);
	}

	if (sdl_window == NULL && (sdl_window=SDL_CreateWindow("",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,640,480,SDL_WINDOW_SHOWN)) == NULL)
		IFEFatalError("SDL2 window creation failed");
	if (sdl_window_surface == NULL && (sdl_window_surface=SDL_GetWindowSurface(sdl_window)) == NULL)
		IFEFatalError("SDL2 window surface failed");
	if (sdl_game_surface == NULL && (sdl_game_surface=SDL_CreateRGBSurfaceWithFormat(0,640,480,8,SDL_PIXELFORMAT_INDEX8)) == NULL)
		IFEFatalError("SDL2 game surface (256-color) failed");
	if (sdl_game_palette == NULL && (sdl_game_palette=SDL_AllocPalette(256)) == NULL)
		IFEFatalError("SDL2 game palette");

	ifevidinfo_sdl2.buf_alloc = ifevidinfo_sdl2.buf_size = sdl_game_surface->pitch * sdl_game_surface->h;
	ifevidinfo_sdl2.buf_pitch = sdl_game_surface->pitch;

	/* first color should be black, SDL2 will init palette to white for some reason */
	memset(sdl_pal,0,sizeof(SDL_Color));
	if (SDL_SetPaletteColors(sdl_game_palette,sdl_pal,0,1) != 0)
		IFEFatalError("SDL2 game palette set colors");

	/* apply palette to surface */
	if (SDL_SetSurfacePalette(sdl_game_surface,sdl_game_palette) != 0)
		IFEFatalError("SDL2 game palette set");

	/* make sure screen is cleared black */
	SDL_FillRect(sdl_game_surface,NULL,0);
	ifeapi->UpdateFullScreen();
	ifeapi->CheckEvents();

	IFECompleteVideoInit();
}

void p_FlushKeyboardInput(void) {
	IFEKeyQueueEmptyAll();
}

IFEKeyEvent *p_GetRawKeyboardInput(void) {
	return IFEKeyQueue.get();
}

IFECookedKeyEvent *p_GetCookedKeyboardInput(void) {
	return IFECookedKeyQueue.get();
}

IFEMouseStatus *p_GetMouseStatus(void) {
	return &ifemousestat;
}

void p_FlushMouseInput(void) {
	IFEMouseQueueEmptyAll();
	ifemousestat.status = 0;
	priv_CaptureMouse(false);
}

IFEMouseEvent *p_GetMouseInput(void) {
	return IFEMouseQueue.get();
}

void p_UpdateScreen(void) {
#define MAX_RUPD 32
	SDL_Rect rupd[MAX_RUPD];
	size_t rupdi=0;
	SDL_Rect r;
	size_t i;

	/* draw spans as needed, clear them as well */
	for (i=0;i < upd_yspan.size();) {
		if (upd_yspan[i].h != 0) { /* update span exists */
			r = upd_yspan[i];
			upd_yspan[i].x = upd_yspan[i].w = upd_yspan[i].h = 0;
			i++;

			/* combine with rects that have the same horizontal span */
			while (i < upd_yspan.size() && upd_yspan[i].x == r.x && upd_yspan[i].w == r.w) {
				r.h += upd_yspan[i].h;
				upd_yspan[i].x = upd_yspan[i].w = upd_yspan[i].h = 0;
				i++;
			}

			if (SDL_BlitSurface(sdl_game_surface,&r,sdl_window_surface,&r) != 0)
				IFEFatalError("Game to window BlitSurface");

			if (rupdi >= MAX_RUPD) {
				if (SDL_UpdateWindowSurfaceRects(sdl_window,rupd,rupdi) != 0)
					IFEFatalError("Window surface update");

				rupdi = 0;
			}
			rupd[rupdi++] = r;
		}
		else {
			i++;
		}
	}

	if (rupdi > 0) {
		if (SDL_UpdateWindowSurfaceRects(sdl_window,rupd,rupdi) != 0)
			IFEFatalError("Window surface update");
	}
#undef MAX_RUPD
}

void p_AddScreenUpdate(int x1,int y1,int x2,int y2) {
	if (x1 < 0) x1 = 0;
	if (y1 < 0) y1 = 0;

	if ((unsigned int)x2 > ifevidinfo_sdl2.width)
		x2 = ifevidinfo_sdl2.width;
	if ((unsigned int)y2 > ifevidinfo_sdl2.height)
		y2 = ifevidinfo_sdl2.height;

	if (x1 >= x2 || y1 >= y2) return;

	y1 = y1 / (int)upd_ystep;
	y2 = (y2 + (int)upd_ystep - 1) / (int)upd_ystep;

	if ((size_t)y2 > upd_yspan.size())
		IFEFatalError("UpdateScreen region bug");

	while ((unsigned int)y1 < (unsigned int)y2) {
		if (upd_yspan[(size_t)y1].h == 0) {
			upd_yspan[(size_t)y1].w = x2-x1;
			upd_yspan[(size_t)y1].h = upd_ystep;
			upd_yspan[(size_t)y1].x = x1;
		}
		else {
			if (upd_yspan[(size_t)y1].x > x1) {
				const unsigned int orig_x = upd_yspan[(size_t)y1].x;
				upd_yspan[(size_t)y1].x = x1;
				upd_yspan[(size_t)y1].w += orig_x - x1;
			}
			if ((upd_yspan[(size_t)y1].x+upd_yspan[(size_t)y1].w) < x2) {
				upd_yspan[(size_t)y1].w = x2-upd_yspan[(size_t)y1].x;
			}
		}
		y1++;
	}
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
	p_ShutdownVideo,
	p_InitVideo,
	p_FlushKeyboardInput,
	p_GetRawKeyboardInput,
	p_GetCookedKeyboardInput,
	p_GetMouseStatus,
	p_FlushMouseInput,
	p_GetMouseInput,
	p_UpdateScreen,
	p_AddScreenUpdate
};

bool priv_IFEMainInit(int argc,char **argv) {
	struct stat st;

	//not used yet
	(void)argc;
	(void)argv;

	/* if STDERR is a valid handle to something, enable debug */
	if (fstat(2/*STDERR*/,&st) == 0) {
		fprintf(stderr,"Will emit debug info to stderr\n");
		ifedbg_en = true;
	}

	return true;
}
#endif

