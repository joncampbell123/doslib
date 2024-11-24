
static unsigned char rgb2ega16(unsigned char r,unsigned char g,unsigned char b) {
	register unsigned char ret = 0;

	unsigned char a = r;
	if (a < g) a = g;
	if (a < b) a = b;

	if (a >= 0x33) {
		if (a >= 0x99) ret += 16;
		ret += (b >= a) ? 1 : 0;
		ret += (g >= a) ? 2 : 0;
		ret += (r >= a) ? 4 : 0;
	}

	return ret;
}

