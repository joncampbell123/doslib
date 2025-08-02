
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>

#if TARGET_MSDOS == 16 && defined(TARGET_WINDOWS)
DWORD Win16_KERNELSYM(const unsigned int ord) {
	HMODULE krnl = GetModuleHandle("KERNEL");
	if (krnl) return (DWORD)GetProcAddress(krnl,MAKEINTRESOURCE(ord));
	return 0;
}

unsigned int Win16_AHINCR(void) {
# if WINVER >= 0x200
	return (unsigned)(Win16_KERNELSYM(114) & 0xFFFFu);
# else
	return 0x1000u; /* Windows 1.x is real mode only, assume real mode __AHINCR */
# endif
}

unsigned int Win16_AHSHIFT(void) {
# if WINVER >= 0x200
	return (unsigned)(Win16_KERNELSYM(113) & 0xFFFFu);
# else
	return 0x12u; /* Windows 1.x is real mode only, assume real mode __AHSHIFT */
# endif
}
#endif // 16-bit Windows

