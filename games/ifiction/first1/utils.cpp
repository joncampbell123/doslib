
#include "utils.h"

#if defined(USE_DOSLIB)
uint16_t cpu_clear_TF(void) { /* EFLAGS bit 8, TF */
	uint32_t f;

	__asm {
		push	eax
		pushfd
		pop	eax
		mov	f,eax
		and	ah,0xFE
		push	eax
		popfd
		pop	eax
	}

	return uint16_t(f & 0x100u); /* only the state of TF before clearing */
}
#endif

