
#if TARGET_MSDOS == 32
static unsigned char *find_vpt(void) {
#else
static unsigned char far *find_vpt(void) {
#endif
	/* NTS: EGA and VGA BIOSes also provide a VPT with this mode listed. At least the IBM PS/2 VGA BIOS does. */
	/* Video Parameter Control Block at 40:A8 */
	/* The first DWORD in that is the 0x40 byte blocks per video mode */
	/* layout:
	 *
	 * struct {
	 *    uint8_t         number of displayed character columns          +0x00
	 *    uint8_t         number of displayed screen rows minus 1        +0x01
	 *    uint8_t         character matrix height in points              +0x02
	 *    uint16_t        video buffer size in bytes                     +0x03
	 *    uint8_t[4]      contents of sequencer registers 1-4            +0x05
	 *    uint8_t         miscellaneous output register value            +0x09
	 *    uint8_t[25]     contents of CRTC registers 0-18h               +0x0A
	 *    uint8_t[20]     contents of attribute controller regs 0-13h    +0x23
	 *    uint8_t[9]      contents of graphics controller regs 0-8       +0x37
	 *                                                                   =0x40
	 * } = 64 bytes;
	 */
	/* The 640x350x4 mode is the 0x10th entry, at offset 0x40*0x10 = 0x400 */
	/* Do check that there are nonzero bytes of course in case SVGA cards from the late 1990s on are like
	 * "yeah, no, nobody's gonna use this" which is understandable.
	 * The way this code is written it renders properly in 640x350x16 mode anyway, so if we can't find it,
	 * no big deal. */
#if TARGET_MSDOS == 32
	uint32_t *vpct = (uint32_t*)(0x400 + 0xA8);
#else
	uint32_t far *vpct = (uint32_t far*)MK_FP(0x40,0xA8);
#endif
	if (*vpct != 0) {
#if TARGET_MSDOS == 32
		uint32_t *vpct2 = (uint32_t*)(((*vpct >> 16ul) << 4ul) + (*vpct & 0xFFFFul));
#else
		uint32_t far *vpct2 = (uint32_t far*)MK_FP(((*vpct >> 16ul) & 0xFFFFu),(*vpct & 0xFFFFu));
#endif
		if (*vpct2 != 0) {
#if TARGET_MSDOS == 32
			unsigned char *vp = (unsigned char*)(((*vpct2 >> 16ul) << 4ul) + (*vpct2 & 0xFFFFul));
#else
			unsigned char far *vp = (unsigned char far*)MK_FP(((*vpct2 >> 16ul) & 0xFFFFu),(*vpct2 & 0xFFFFu));
#endif
			return vp;
		}
	}

	return NULL;
}

#if TARGET_MSDOS == 32
static void apply_vpt_mode(unsigned char *vp) {
#else
static void apply_vpt_mode(unsigned char far *vp) {
#endif
	unsigned int i;

	/* sequencer (1-4) */
	for (i=1;i <= 0x04;i++)
		outpw(0x3C4,i + ((unsigned int)vp[0x05 + (i-1)] << 8u));

	/* miscellaneous */
	outp(0x3C2,vp[0x09]);

	/* CRTC registers (0x00-0x18) */
	outpw(0x3D4,0x0011); /* clear write protect */
	for (i=0;i <= 0x18;i++)
		outpw(0x3D4,i + ((unsigned int)vp[0x0A + i] << 8u));

	/* Attribute controller (0x00-0x13) */
	for (i=0;i <= 0x13;i++) {
		inp(0x3DA);
		outp(0x3C0,i);
		outp(0x3C0,vp[0x23 + i]);
	}
	inp(0x3DA);
	outp(0x3C0,0x20);

	/* graphics controller (0x00-0x08) */
	for (i=0;i <= 8;i++)
		outpw(0x3CE,i + ((unsigned int)vp[0x37 + i] << 8u));
}

