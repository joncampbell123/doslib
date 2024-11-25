
#if TARGET_MSDOS == 32
static void hgcincolor4pcpy(unsigned char *d,unsigned char *src,unsigned int pixels) {
	register unsigned char *w;
#else
static void hgcincolor4pcpy(unsigned char far *d,unsigned char *src,unsigned int pixels) {
	register unsigned char far *w;
#endif
	register unsigned int *s;
	unsigned char plane;
	unsigned int b;

	outpw(0x3B4,0x0019); /* write mode 0 (and don't care, and mask polarity) */
	outpw(0x3B4,0x0F1A); /* foreground=15/background=0 */
	for (plane=0;plane < 4;plane++) {
		w = d; s = (unsigned int*)src; outpw(0x3B4,0x0F18 + (((0x1<<plane)^0xF)<<12));
		for (b=0;b < pixels;b += 8) {
			register unsigned char b = 0,m = 0x80;
			register unsigned int t;

#if TARGET_MSDOS == 32
			t = (*s++) >> plane;
			if (t & 0x00000010u) b |= m; m >>= 1u;
			if (t & 0x00000001u) b |= m; m >>= 1u; t >>= 8u;
			if (t & 0x00000010u) b |= m; m >>= 1u;
			if (t & 0x00000001u) b |= m; m >>= 1u; t >>= 8u;
			if (t & 0x00000010u) b |= m; m >>= 1u;
			if (t & 0x00000001u) b |= m; m >>= 1u; t >>= 8u;
			if (t & 0x00000010u) b |= m; m >>= 1u;
			if (t & 0x00000001u) b |= m;
#else // 16-bit
			t = (*s++) >> plane;
			if (t & 0x0010u) b |= m; m >>= 1u;
			if (t & 0x0001u) b |= m; m >>= 1u; t >>= 8u;
			if (t & 0x0010u) b |= m; m >>= 1u;
			if (t & 0x0001u) b |= m; m >>= 1u;
			t = (*s++) >> plane;
			if (t & 0x0010u) b |= m; m >>= 1u;
			if (t & 0x0001u) b |= m; m >>= 1u; t >>= 8u;
			if (t & 0x0010u) b |= m; m >>= 1u;
			if (t & 0x0001u) b |= m;
#endif
			*w++ = b;
		}
	}
	outpw(0x3B4,0x0F18);
}

/* draw scanline in 4bpp planar 16-color mode */
static void draw_scanline(unsigned int y,unsigned char *src,unsigned int pixels) {
	if (y < img_height) {
#if TARGET_MSDOS == 32
		unsigned char *d = (unsigned char*)0xB0000 + ((y >> 2) * img_stride) + ((y & 3) * 0x2000u);
#else
		unsigned char far *d = MK_FP(0xB000,((y >> 2) * img_stride) + ((y & 3) * 0x2000));
#endif
		hgcincolor4pcpy(d,src,pixels);
	}
}

