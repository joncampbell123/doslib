
#ifdef DSCOLOR
#if TARGET_MSDOS == 32
static inline void vga_rmw(unsigned char *d,const unsigned char b) {
	(void)d;
	(void)b;
	__asm {
		push	esi
		mov	esi,d
		mov	al,[esi]
		mov	al,b
		mov	[esi],al
		pop	esi
	}
}
#else
static inline void vga_rmw(unsigned char far *d,const unsigned char b) {
	(void)d;
	(void)b;
	__asm {
		push	ds
		push	si
		lds	si,d
		mov	al,[si]
		mov	al,b
		mov	[si],al
		pop	si
		pop	ds
	}
}
#endif
#endif

#ifdef DSCOLOR
#if TARGET_MSDOS == 32
static void vga4pcpy(unsigned char *d,unsigned char *src,unsigned int pixels) {
	register unsigned char *w;
#else
static void vga4pcpy(unsigned char far *d,unsigned char *src,unsigned int pixels) {
	register unsigned char far *w;
#endif
	register unsigned char *s;
	register uint16_t m;
	unsigned int b;

	outpw(0x3C4,0x0F02); /* write all bitplanes (map mask) */
	outpw(0x3CE,0x0205); /* write mode 2 (read mode 0) */

	m = 0x8000;
	for (b=0;b < 8 && b < pixels;b += 2) {
		const unsigned int count = (pixels + 7u - b) >> 3u;
		register unsigned int x;

		if (count == 0u) break;

		outpw(0x3CE,0x0008 + m); m >>= 1u; w = d; s = src;   x = count; do { vga_rmw(w++,*s >>  4u); s += 4; } while (--x != 0u);
		outpw(0x3CE,0x0008 + m); m >>= 1u; w = d; s = src++; x = count; do { vga_rmw(w++,*s & 0xFu); s += 4; } while (--x != 0u);
	}

	outpw(0x3CE,0x0005); /* write mode 0 (read mode 0) */
	outpw(0x3CE,0xFF08); /* bit mask */
}
#endif

typedef void (*draw_scanline_func_t)(unsigned int y,unsigned char *src,unsigned int pixels);

#ifdef PVGA1A
/* ET4K */
# define BANKSWITCH(bank) outpw(0x3CE,0x09 + ((bank) << 8u))
# define DSFUNCM draw_scanline_pvga1am
# define DSFUNC draw_scanline_pvga1a
# include "dr_bnk4t.h"
# undef BANKSWITCH
# undef DSFUNCM
# undef DSFUNC
#endif

