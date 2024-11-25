
static unsigned char hgc_type = 0;
static int herc_detect() {
	unsigned char cm,c;
	unsigned int hsyncs = 0; /* in case we're on a slow machine use hsync count to know when to stop */
	unsigned int patience = 0xFFFF; /* NTS: 0xFFFF on actual hardware of that era might turn into a considerable and unnecessary pause */

	cm = inp(0x3BA);
	while (--patience != 0 && ((c=inp(0x3BA)) & 0x80) == (cm & 0x80)) {
		if ((c^cm) & 1) { /* HSYNC change? */
			cm ^= 1;
			if (c & 1) { /* did HSYNC start? */
				if (++hsyncs >= 600) break; /* if we've gone 600 hsyncs without seeing VSYNC change then give up */
			}
		}
		inp(0x80);
	}
	if (patience > 0 && (c^cm) & 0x80) { /* if it changed, then HGC */
		hgc_type = (c >> 4) & 7;
		switch ((c>>4)&7) {
			case 5: case 1: hgc_type = (c>>4)&7; break;
			default: hgc_type = 0; break;
		}

		return 1;
	}

	return 0;
}

