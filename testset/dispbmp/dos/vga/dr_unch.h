
/* draw scanline in unchained 256-color mode */
static void draw_scanline(unsigned int y,unsigned char *src,unsigned int pixels) {
	if (y < img_height) {
#if TARGET_MSDOS == 32
		unsigned char *d = (unsigned char*)0xA0000 + (y * (img_width >> 2u));
#else
		unsigned char far *d = MK_FP(0xA000,y * (img_width >> 2u));
#endif
		unsigned char bitplane;
		unsigned int x,o;

		for (bitplane=0;bitplane < 4;bitplane++) {
			outpw(0x3C4,0x0002 + (0x100 << bitplane)); /* sequencer map mask */
			o = 0; for (x=bitplane;x < pixels;x += 4) d[o++] = src[x];
		}
	}
}

