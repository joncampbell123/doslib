
static unsigned char rgb2ega16(unsigned char r,unsigned char g,unsigned char b) {
	const unsigned char a = (r + (g * 2) + b + 2) >> 2;
	register unsigned char ret = 0;

	if (a >= 0x40) {
		if (a >= 0x70) ret += 8;
		ret += (b >= a) ? 1 : 0;
		ret += (g >= a) ? 2 : 0;
		ret += (r >= a) ? 4 : 0;
	}

	return ret;
}

