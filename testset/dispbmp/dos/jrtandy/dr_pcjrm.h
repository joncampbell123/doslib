
#if TARGET_MSDOS == 32
static unsigned char *get_pcjr_mem(void) {
#else
static unsigned char far *get_pcjr_mem(void) {
#endif
	unsigned short s;
	unsigned char b;

	if (is_pcjr) {
		/* PCjr: Must locate the system memory area because the video mem alias is limited to 16KB at 0xB8000 */
		/* [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/Video/PCjr/IBM%20Personal%20Computer%20PCjr%20Hardware%20Reference%20Library%20Technical%20Reference%20%281983%2d11%29%20First%20Edition%20Revised%2epdf]
		 * See:
		 *
		 * CPUREG EQU 0x38 ; MASK FOR CPU REG BITS
		 * CRTREG EQU 0x07 ; MASK FOR CRT REG BITS
		 *
		 * At segment 0x40 offset 0x8A PAGDAT DB ? ; IMAGE OF DATA WRITTEN TO PAGREG
		 *
		 * So to determine where in system memory the full 32KB exists, read 0x40:0x8A bits [5:3] and that is the location in 16KB units */
#if TARGET_MSDOS == 32
		b = *((unsigned char*)(0x48A));
#else
		b = *((unsigned char far*)MK_FP(0x40,0x8A));
#endif
		s = ((b >> 3) & 7) << 10;

		/* SAFETY: If s == 0, then return 0xB800 */
		if (s == 0) s = 0xB800;
	}
	else {
		s = 0xB800;
	}

#if TARGET_MSDOS == 32
	return (unsigned char*)(s << 4);
#else
	return (unsigned char far*)MK_FP(s,0);
#endif
}

