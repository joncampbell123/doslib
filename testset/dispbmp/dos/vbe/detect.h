
static int tseng_et3000_detect() {
	unsigned char old,val;
	int base;

	old = inp(0x3CD);
	outp(0x3CD,old ^ 0x3F);
	val = inp(0x3CD);
	outp(0x3CD,old);
	if (val != (old ^ 0x3F)) return 0;

	if (inp(0x3CC) & 1)	base = 0x3D4;
	else			base = 0x3B4;
	outp(base,0x1B);
	old = inp(base+1);
	outp(base+1,old ^ 0xFF);
	val = inp(base+1);
	outp(base+1,old);
	if (val != (old ^ 0xFF)) return 0;

	/* ET3000 detected */
	return 1;
}

static void tseng_et4000_extended_set_lock(int x) {
	if (x) {
		if (inp(0x3CC) & 1)	outp(0x3D8,0x29);
		else			outp(0x3B8,0x29);
		outp(0x3BF,0x01);
	}
	else {
		outp(0x3BF,0x03);
		if (inp(0x3CC) & 1)	outp(0x3D8,0xA0);
		else			outp(0x3B8,0xA0);
	}
}

static int tseng_et4000_detect() {
	unsigned char new,old,val;
	int base;

	/* extended register enable */
	tseng_et4000_extended_set_lock(/*unlock*/0);

	old = inp(0x3CD);
	outp(0x3CD,0x55);
	val = inp(0x3CD);
	outp(0x3CD,old);
	if (val != 0x55) return 0;

	if (inp(0x3CC) & 1)	base = 0x3D4;
	else			base = 0x3B4;
	outp(base,0x33);
	old = inp(base+1);
	new = old ^ 0x0F;
	outp(base+1,new);
	val = inp(base+1);
	outp(base+1,old);
	if (val != new) return 0;

	/* extended register lock */
	tseng_et4000_extended_set_lock(/*lock*/1);

	/* ET4000 detected */
	return 1;
}

