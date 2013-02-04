/* biosext.c
 *
 * DOS library for carrying out extended memory copy using BIOS INT 15h
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/biosext.h>

#if TARGET_MSDOS == 16
int bios_extcopy_sub(uint32_t dst,uint32_t src,unsigned long copy/* WARNING: must be multiple of 2, and <= 64KB */) {
	/* WARNING: There are better ways to access extended memory.
	 *          But we have this function for those cases where the better ways are
	 *          unavailable. Such as:
	 *
	 *             a) we're running on a 286, where 32-bit 386 instructions
	 *                are unavailable to carry out flat real mode tricks
	 *
	 *             b) we're running under Windows, whos DOS VM subsystem
	 *                is intolerant of flat real mode and generally demands that
	 *                we switch into DPMI protected mode to even access it
	 *
	 *             c) we're running under some other virtual 8086 mode system
	 *                that forbids flat realmode like hacks (such as EMM386.EXE)
	 *
	 *          There is also another possible danger: Some BIOS implementations emulate 286 behavior (even on
	 *          386, 486, Pentium, and beyond) by masking off or ignoring the upper 8 bits of the base (24-31).
	 *          On these systems, accessing memory at or above the 16MB mark with the BIOS is impossible. But on
	 *          these same systems, you have 32-bit protected mode and the option to use flat real mode anyway,
	 *          so it doesn't really matter. On a 286 class system where you must use this function, the CPU is
	 *          physically incapable of signalling more than 16MB (24 bit addressing) so again, that doesn't
	 *          matter.
	 *
	 *          TODO: I need to test this theory: If a BIOS ignores address bits 24-31, but we're running under
	 *                Windows (and therefore the Windows kernel's emulation of the BIOS call), does the address
	 *                masking limitation still apply? */
	/* The following comment is a list of all system & BIOS combinations this code has been tested under.
	 *
	 *   BIOS/System/Environment         Works?       At or above 16MB boundary?
	 *   ---------------------------------------------------------------------
	 *   DOSBox 0.74                     YES w/ BUGS  YES
	 *   Microsoft Virtual PC 2007       YES          YES
	 *   Oracle VirtualBox 4.0.4         YES          YES
	 *   QEMU                            YES          YES
	 *   DOSBox 0.74 +
	 *      Microsoft Windows 3.0
	 *         Real mode                 YES          YES
	 *         Standard mode             YES          YES
	 *         386 Enhanced mode         YES          NO
	 *      Microsoft Windows 3.1
	 *         Standard mode             YES          YES
	 *         386 Enhanced mode         YES          YES
	 *      Microsoft Windows 3.11
	 *         Standard mode             YES          YES
	 *         386 Enhanced mode         YES          YES
	 *   QEMU +
	 *      Microsoft Windows 95 (4.00.950)
	 *         Normal                    YES          YES
	 *         Safe mode                 YES          YES
	 *      Microsoft Windows 98 (4.10.1998)
	 *         Normal                    YES          YES
	 *         Safe mode                 YES          YES
	 *      Microsoft Windows ME (4.90.3000)
	 *         Normal                    YES          YES
	 *         Safe mode                 YES          YES
	 *      Microsoft Windows 2000 Professional (5.00.2195)
	 *         Normal                    YES          NO
	 *      Microsoft Windows XP Professional (5.1.2600)
	 *         Normal                    YES          NO
	 *      Microsoft Windows XP Professional SP2 (5.1.2600)
	 *         Normal                    YES          NO
	 *      Microsoft Windows Vista Ultimate
	 *         Normal                    YES          NO
	 *
	 *   Bugs:
	 *     * DOSBox 0.74: DOSBox directly emulates the INT BIOS function. Unfortunately it does it wrong,
	 *                    apparently getting bytes 7 and 8 of the descriptor backwards. If this code always
	 *                    fills in the flags and limit(16:19) bits, then it causes DOSBox to access addresses
	 *                    in the 0x8F000000....0x8FFFFFFF range (because the flags+limit byte is usually 0x8F
	 *                    on 386 systems and DOSBox is mistreating it as bits 24-31 of the address).
	 *
	 *                    Unfortunately this bug means that any program relying on this function exclusively
	 *                    will be unable to properly target memory above 16MB when running under DOSBox. However,
	 *                    DOSBox also fails to enforce segment limits in real mode (emulating a CPU that is
	 *                    perpetually in "flat real mode") which means that you are free to abuse 386+ style
	 *                    32-bit addressing, therefore, you should be using flat real mode instead of this function.
	 *
	 *                    It's possible the DOSBox developers missed the bug because they only tested it against
	 *                    ancient DOS games written in the 286 era that habitually left bytes 7-8 zero anyway.
	 *                    Who knows?
	 */
	union REGS regs;
	struct SREGS sregs;
	uint8_t tmp[0x40];	/* the global descriptor table */

	memset(tmp,0,sizeof(tmp));

	*((uint16_t*)(tmp+0x10)) = 0xFFFF;				/* limit (source) */
	*((uint32_t*)(tmp+0x12)) = 0x93000000UL + (src & 0xFFFFFFUL);	/* base and access byte (source) */
	if (src >= 0x1000000UL/*>= 16MB*/) /* see DOSBox bug report listed above to understand why I am filling in bytes 7-8 this way */
		*((uint16_t*)(tmp+0x16)) = ((src >> 16) & 0xFF00UL) + 0x8FUL;	/* (386) base bits 24-31, flags, limit bits 16-19 */

	*((uint16_t*)(tmp+0x18)) = 0xFFFF;				/* limit (dest) */
	*((uint32_t*)(tmp+0x1A)) = 0x93000000UL + (dst & 0xFFFFFFUL);	/* base and access byte (dest) */
	if (dst >= 0x1000000UL/*>= 16MB*/) /* see DOSBox bug report listed above to understand why I am filling in bytes 7-8 this way */
		*((uint16_t*)(tmp+0x1E)) = ((dst >> 16) & 0xFF00UL) + 0x8FUL;	/* (386) base bits 24-31, flags, limit bits 16-19 */

	regs.h.ah = 0x87;
	regs.w.cx = (unsigned int)(copy >> 1UL);	/* number of WORDS, not BYTES */
	regs.w.si = FP_OFF((unsigned char*)tmp);
	sregs.es = FP_SEG((unsigned char*)tmp);
	int86x(0x15,&regs,&regs,&sregs); /* now call the BIOS */
	return (regs.h.ah == 0) ? 0 : -1;
}

int bios_extcopy(uint32_t dst,uint32_t src,unsigned long copy) {
	if (copy == 0UL) return 0;
	/* if we're on anything less than a 286, then this function is meaningless--there is no extended memory */
	if (cpu_basic_level == (signed char)0xFF) cpu_probe();
	if (cpu_basic_level < 2) return -1;
	/* carry out the copy operation */
	while (copy >= 0x10000UL) {
		if (bios_extcopy_sub(dst,src,0x10000UL))
			return -1;

		copy -= 0x10000UL;
		dst += 0x10000UL;
		src += 0x10000UL;
	}
	if (copy >= 2UL) {
		if (bios_extcopy_sub(dst,src,copy & 0xFFFFFFFEUL))
			return -1;

		dst += copy & 0xFFFFFFFEUL;
		src += copy & 0xFFFFFFFEUL;
		copy &= 1UL;
	}
	if (copy != 0UL) {
		unsigned char tmp[2];
		if (bios_extcopy_sub(ptr2phys(tmp),src,2)) return -1;
		*((unsigned char far*)MK_FP(dst>>4,dst&0xF)) = tmp[0];
	}

	return 0;
}
#endif

