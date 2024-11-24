
static unsigned int ega_memory_size(void) {
	uint16_t r = 0;

	__asm {
		mov	ax,0x1200
		mov	bx,0xFF10
		int	10h
		cmp	bh,0xFF
		jz	noega
		xor	bh,bh
		inc	bx
		mov	r,bx
noega:
	}

	if (r > 4) r = 4;
	return r * 64u;
}

