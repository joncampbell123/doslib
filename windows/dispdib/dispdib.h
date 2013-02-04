
#include <hw/dos/dos.h>
#include <windows/apihelp.h>
#include <windows.h>

/* Microsoft headers */
#include <windows/dispdib/dispdms.h>

extern BITMAPINFOHEADER FAR*	dispDibFormat;
extern BYTE FAR*		dispDibBuffer;
extern HGLOBAL			dispDibBufferHandle;
extern DWORD			dispDibBufferFormat;
extern DWORD			dispDibBufferSize;

extern unsigned char		dispDibUseEntryPoints; /* <- Under Windows 3.1, hwnd == NULL and this value nonzero */
extern HWND			dispDibHwnd; /* <-- NOTE: Under Windows 3.1, this is NULL, because we use the direct function calls */
extern HWND			dispDibHwndParent;
#if TARGET_MSDOS == 32
extern DWORD			dispDibDLLTask;
#else
extern HINSTANCE		dispDibDLL;
#endif
extern const char*		dispDibLastError;
extern unsigned char		dispDibUserActive; /* User flag for caller to track whether it was active before switching away */
extern unsigned char		dispDibFullscreen; /* whether we are fullscreen */
extern unsigned int		dispDibEnterCount;
extern unsigned char		dispDibRedirectAllKeyboard; /* If the calling app is passing all messages through us, redirect keyboard I/O to parent hwnd */
#if TARGET_MSDOS == 32
extern unsigned char		dispDibDontUseWin9xLoadLibrary16;
#endif
extern const char*		dispDibLoadMethod;
#if TARGET_MSDOS == 16
extern UINT (FAR PASCAL *__DisplayDib)(LPBITMAPINFOHEADER lpbi, LPSTR lpBits, WORD wFlags);
extern UINT (FAR PASCAL *__DisplayDibEx)(LPBITMAPINFOHEADER lpbi, int x, int y, LPSTR lpBits, WORD wFlags);
#endif

int DisplayDibIsLoaded();
int DisplayDibCreateWindow(HWND hwndParent);
int DisplayDibAllocBuffer();
BYTE FAR *DisplayDibLockBuffer();
void DisplayDibUnlockBuffer();
void DisplayDibFreeBuffer();
RGBQUAD FAR *DisplayDibPalette();
int DisplayDibSetFormat(unsigned int width,unsigned int height);
int DisplayDibGenerateTestImage();
int DisplayDibLoadDLL();
void DisplayDibDoUpdate();
void DisplayDibDoUpdatePalette();
int DisplayDibDoBegin();
int DisplayDibDoEnd();
int DisplayDibUnloadDLL();
int DisplayDibCheckMessageAndRedirect(LPMSG msg);
BYTE DisplayDibScreenPointerIsModeX();
unsigned int DisplayDibScreenStride();
BYTE FAR *DisplayDibGetScreenPointerA000();
BYTE FAR *DisplayDibGetScreenPointerB000();
BYTE FAR *DisplayDibGetScreenPointerB800();
int DisplayDibSetBIOSVideoMode(unsigned char x);

