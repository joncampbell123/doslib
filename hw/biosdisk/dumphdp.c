/* Diagnostic program:

   1. Call INT 13h and request drive C (0x80) geometry.

   2. Locate the BIOS disk table describing drive C and dump it's contents

   3. Read drive C geometry from CMOS (or try to, anyway...)

   Comparison against IDE can then be made using the IDE test code.
   Hopefully the results of this program can reveal what Windows 95's "Standard ESDI/IDE controller" driver
   is so goddamn picky about and we can get DOSBox-X to emulate it --Jonathan C */

/* NOTES:

       - VirualBox seems to format CMOS data best viewed by "AWARD #2" parsing

       - QEMU INT 13h "Get Drive Parameters" returns cylinders - 2, not cylinders - 1
         like most implementations do. Emulating the "spare cylinder" thing perhaps?
         It stores data in the same manner as "AMI".

       - If BIOS geometry translation is in effect, the Fixed Disk Parameter Table and
         CMOS values will be translated as well. Getting the real geometry requires
         that we talk to the IDE drive directly.

       - ^ This means DOSBox is technically already correct in how it emulates INT 13h
         and said tables/CMOS. Windows 95's hangup with my IDE emulation can't be the
         result of CMOS then...

       - IDE drives return a "legacy" current sector total and a "total user addressible"
         sector total. In VirtualBox, the legacy count is always the product C * H * S
         while "total user addressible" is the actual number of sectors on the disk.
 */

#if TARGET_MSDOS == 32
# error This is not 32-bit capable code
#endif
#ifdef TARGET_WINDOWS
# error This code is NOT for windows
#endif
#ifdef TARGET_OS2
# error This code is NOT for OS2
#endif

#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>
#include <dos.h>

/* NTS: Steal some code from rtc.h */
static inline void rtc_io_finished() {
	outp(0x70,0x0D);
	inp(0x71);
}

static inline unsigned char rtc_io_read(unsigned char idx) {
	outp(0x70,idx | 0x80);	/* also mask off NMI */
	return inp(0x71);
}

int main() {
	unsigned int a=0,b=0,c=0,d=0,e=0;
	unsigned char far *fdpt = (unsigned char far*)_dos_getvect(0x41);

	/* read INT 13h geometry */
	__asm {
		push	es
		mov	ah,8
		mov	dl,0x80
		xor	di,di
		mov	bx,di
		mov	cx,di
		mov	dh,bh
		mov	es,di
		int	13h
		pop	es
		xor	al,al
		jnc	no1
		mov	al,0xFF
no1:		mov	a,ax
		mov	b,bx
		mov	c,cx
		mov	d,dx
		; AH=bios return error AL=0 if success 0xFF if not
	}
	printf("INT 13h results: AX=%04x BX=%04x CX=%04x DX=%04x\n",a,b,c,d);
	printf("   C/H/S: %u/%u/%u\n",((((c>>6)&3) << 8) | (c>>8))+1,(d>>8)+1,(c&0x3F));

	/* read the Fixed Disk Parameter Table */
	printf("Fixed Disk Parameter Table (at %04X:%04X)\n",FP_SEG(fdpt),FP_OFF(fdpt));
	printf("   C/H/S: %u/%u/%u land=%u ctrl=%02x\n",
		*((unsigned short far*)(fdpt+0)),
		fdpt[2],
		fdpt[14],
		*((unsigned short far*)(fdpt+12)),
		fdpt[8]);
	printf("   [RAW]: ");
	for (a=0;a < 16;a++) printf("%02x ",fdpt[a]);
	printf("\n");

	/* read CMOS memory where most BIOSes store hard disk geometry */
	__asm cli
	a = rtc_io_read(0x12);
	rtc_io_finished();
	__asm sti
	printf("CMOS hard disk geometry: Drive 0=%u Drive 1=%u\n",(a>>4),a&0xF);

	__asm cli
	a = rtc_io_read(0x19);
	b = rtc_io_read(0x1A);
	rtc_io_finished();
	__asm sti
	printf("  Drive 0 ext-type %u   Drive 1 ext-type %u\n",a,b);

	__asm cli
	a = rtc_io_read(0x1B) | (rtc_io_read(0x1C) << 8);
	b = rtc_io_read(0x1D);
	c = rtc_io_read(0x20);
	d = rtc_io_read(0x23);
	rtc_io_finished();
	__asm sti
	printf("  [AMI] Drive 0 C/H/S %u/%u/%u control %02x\n",a,b,d,c);

	__asm cli
	a = rtc_io_read(0x26) | (rtc_io_read(0x27) << 8);
	b = rtc_io_read(0x28);
	c = rtc_io_read(0x2D);
	rtc_io_finished();
	__asm sti
	printf("  [AWARD #1] Drive 0 C/H/S %u/%u/%u\n",a,b,c);

	/* apparently this is what VirtualBox follows */
	__asm cli
	a = rtc_io_read(0x1E) | (rtc_io_read(0x1F) << 8); /* 2nd hard disk my ass */
	b = rtc_io_read(0x20);
	c = rtc_io_read(0x25);
	d = rtc_io_read(0x21) | (rtc_io_read(0x22) << 8);
	e = rtc_io_read(0x23) | (rtc_io_read(0x24) << 8);
	rtc_io_finished();
	__asm sti
	printf("  [AWARD #2] Drive 0 C/H/S %u/%u/%u write_precomp=%u land=%u\n",a,b,c,d,e);

	return 0;
}

