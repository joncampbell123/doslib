
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include <hw/cpu/cpu.h>

#if TARGET_MSDOS == 32
uint16_t __declspec(naked) cpu_read_my_cs() {
	/* NTS: Don't forget that in 32-bit mode, PUSH <sreg> pushes the 16-bit segment value zero extended to 32-bit!
	 *      However Open Watcom will encode "pop ax" as "pop 16 bits off stack to AX" so the "pop eax" is needed
	 *      to keep the stack proper. It didn't used to do that, but some change made it happen that way, so,
	 *      now we have to explicitly say "pop eax" (2022/01/28) */
	/* NTS: Open Watcom has developed a nasty bug where it references stack variables by EBP without making an ESP/EBP
	 *      stack frame, causing stack corruption with some functions and breaking even this function. To work around
	 *      it, this function is now naked. */
	__asm {
		push	cs
		pop	eax
		ret
	}
}
#endif

