
/* VGA 16-color: We need to program the Attribute Controller palette to an identity mapping for palette programming to work,
 * unless of course you want to take care to write only 0x00-0x07 and 0x38-0x3F all the time. Hm? No? Thought so. */
static void vga_ac_identity_map(void) {
	unsigned int x;

	for (x=0;x < 16;x++) {
		inp(0x3DA);
		outp(0x3C0,x); /* index */
		outp(0x3C0,x); /* data */
	}
	inp(0x3DA);
	outp(0x3C0,0x20); /* reenable the display */
}

