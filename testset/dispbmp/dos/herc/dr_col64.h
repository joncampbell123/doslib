
static unsigned char rgb2ega64(const unsigned char r,const unsigned char g,const unsigned char b) {
	register unsigned char ret;
	register unsigned char c;

	/* RrGgBb encoded as RGBrgb */
	c = (b + 0u) >> 6u; ret  = ((c & 2) ? 0x01 : 0) + ((c & 1) ? 0x08 : 0);
	c = (g + 0u) >> 6u; ret += ((c & 2) ? 0x02 : 0) + ((c & 1) ? 0x10 : 0);
	c = (r + 0u) >> 6u; ret += ((c & 2) ? 0x04 : 0) + ((c & 1) ? 0x20 : 0);
	return ret;
}

