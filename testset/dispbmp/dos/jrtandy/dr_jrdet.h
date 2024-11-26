
static unsigned int detect_pcjr(void) {
	unsigned char mb; /* model byte */

#if TARGET_MSDOS == 32
	mb = *((unsigned char*)0xFFFFE);
#else
	mb = *((unsigned char far*)MK_FP(0xF000,0xFFFE));
#endif

	return (mb == 0xFD);
}

static unsigned int detect_tandy(void) {
	unsigned char mb; /* model byte */

#if TARGET_MSDOS == 32
	mb = *((unsigned char*)0xFFFFE);
#else
	mb = *((unsigned char far*)MK_FP(0xF000,0xFFFE));
#endif

	return (mb == 0xFF);
}

