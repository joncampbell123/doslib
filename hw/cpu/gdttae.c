/* gdt_tae.c
 *
 * Dumps the contents of the GDT onto the screen using "trial and error",
 * meaning that it loads each segment and attempts to read from it. It uses
 * exception handling to safely recover.
 *
 * (C) 2011-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/cpu/cpuidext.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

unsigned char tmp[16];
int lines=0;

#if TARGET_MSDOS == 32 && defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
int TestSegmentWin32(unsigned int segm,unsigned char *ptrv,unsigned int cntv) {
	int worked = -1;

	__try {
		__asm {
			mov	eax,segm
			mov	es,ax		; <- might fault here
			; OK try to read
			; Be *CAREFUL*, it might be a 16-bit data segment which depending on
			; i386 intricate details and CPU might make this code act wonky.
			; more specifically, it might affect how the cpu decodes 16-bit vs 32-bit instructions with data.
			; so we want to access only in a way the CPU will basically do the same thing either way.
			mov	worked,0

			; if decoded as 16-bit: becomes MOV AL,[BX]
			; if decoded as 32-bit: becomes MOV AL,[EDI]
			xor	ebx,ebx
			mov	edi,ebx
			mov	esi,ptrv
			mov	ecx,cntv	; <- copy 32 bytes

			; read data
lev1:			mov	al,BYTE PTR [es:edi]
			mov	BYTE PTR [esi],al
			inc	edi
			inc	esi
			mov	ebx,edi
			mov	worked,edi
			loop	lev1
		}
	}
	__except(1) {
	}

	__asm {
		push	ds
		pop	es
	}

	return worked;
}
#endif

#if TARGET_MSDOS == 32 && defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
static void pause_if_tty() {
	unsigned char c;
	if (isatty(1)) { do { read(0,&c,1); } while (c != 13); }
}
#endif

int main() {
	cpu_probe();
	printf("Your CPU is basically a %s or higher\n",cpu_basic_level_to_string(cpu_basic_level));

	if (cpu_v86_active)
		printf(" - Your CPU is currently running me in virtual 8086 mode\n");
	if (cpu_flags & CPU_FLAG_PROTECTED_MODE)
		printf(" - Your CPU is currently running in protected mode\n");
	if (cpu_flags & CPU_FLAG_PROTECTED_MODE_32)
		printf(" - Your CPU is currently running in 32-bit protected mode\n");

#if TARGET_MSDOS == 32 && defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
	{
		unsigned int selv=0,i;
		int worked;

		__asm {
			xor	eax,eax
			mov	ax,cs
			mov	selv,eax
		}
		printf("CS=%04x\n",selv);

		__asm {
			xor	eax,eax
			mov	ax,ds
			mov	selv,eax
		}
		printf("DS=%04x\n",selv);

		/* start from segment 0x08 and move on from there, alternating LDT and GDT, always assuming DPL=3 */
		for (selv=8|3;selv <= 0xFFFF;selv += 4) {
			worked = TestSegmentWin32(selv,tmp,sizeof(tmp));
			if (worked >= 0) {
				printf("Found segment %04x (%u bytes)\n",selv,worked);
				fflush(stdout);
				lines++;

				if (worked != 0) {
					for (i=0;i < worked;i++) printf("%02x ",tmp[i]);
					printf("\n");
					lines++;
				}

				if (lines >= 20) {
					pause_if_tty();
					lines -= 20;
				}
			}
		}
	}
#else
	printf("Target environment is not applicable\n");
#endif

	cpu_extended_cpuid_info_free();
	return 0;
}

