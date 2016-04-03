
/* FIXME: Need to put this elsewhere in DOSLIB */

/* .PCX image file format header */
#pragma pack(push,1)
struct pcx_header {
	uint8_t			manufacturer;		// +0x00  always 0x0A
	uint8_t			version;		// +0x01  0, 2, 3, or 5
	uint8_t			encoding;		// +0x02  always 0x01 for RLE
	uint8_t			bitsPerPlane;		// +0x03  bits per pixel in each color plane (1, 2, 4, 8, 24)
	uint16_t		Xmin,Ymin,Xmax,Ymax;	// +0x04  window (image dimensions). Pixel count in each dimension is Xmin <= x <= Xmax, Ymin <= y <= Ymax i.e. INCLUSIVE
	uint16_t		VertDPI,HorzDPI;	// +0x0C  vertical/horizontal resolution in DPI
	uint8_t			palette[48];		// +0x10  16-color or less color palette
	uint8_t			reserved;		// +0x40  reserved, set to zero
	uint8_t			colorPlanes;		// +0x41  number of color planes
	uint16_t		bytesPerPlaneLine;	// +0x42  number of bytes to read for a single plane's scanline (uncompressed, apparently)
	uint16_t		palType;		// +0x44  palette type (1 = color)
	uint16_t		hScrSize,vScrSize;	// +0x46  scrolling?
	uint8_t			pad[54];		// +0x4A  padding
};							// =0x80
#pragma pack(pop)

