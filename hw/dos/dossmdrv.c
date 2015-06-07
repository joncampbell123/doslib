/* dos.c
 *
 * Code to detect the surrounding DOS/Windows environment and support routines to work with it
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

#ifdef TARGET_WINDOWS
# include <windows.h>
#endif

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/dos/dosntvdm.h>

#if TARGET_MSDOS == 16 || (TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS) && !defined(TARGET_OS2))
unsigned short smartdrv_version = 0xFFFF;
int smartdrv_fd = -1;
#endif

#if (TARGET_MSDOS == 16 || TARGET_MSDOS == 32) && !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
int smartdrv_close() {
	if (smartdrv_fd >= 0) {
		close(smartdrv_fd);
		smartdrv_fd = -1;
	}

	return 0;
}

int smartdrv_flush() {
	if (smartdrv_version == 0xFFFF || smartdrv_version == 0)
		return 0;

	if (smartdrv_version >= 0x400) { /* SMARTDRV 4.xx and later */
#if TARGET_MSDOS == 32
		__asm {
			push	eax
			push	ebx
			mov	eax,0x4A10		; SMARTDRV
			mov	ebx,0x0001		; FLUSH BUFFERS (COMMIT CACHE)
			int	0x2F
			pop	ebx
			pop	eax
		}
#else
		__asm {
			push	ax
			push	bx
			mov	ax,0x4A10		; SMARTDRV
			mov	bx,0x0001		; FLUSH BUFFERS (COMMIT CACHE)
			int	0x2F
			pop	bx
			pop	ax
		}
#endif
	}
	else if (smartdrv_version >= 0x300 && smartdrv_version <= 0x3FF) { /* SMARTDRV 3.xx */
		char buffer[0x1];
#if TARGET_MSDOS == 32
#else
		char far *bptr = (char far*)buffer;
#endif
		int rd=0;

		if (smartdrv_fd < 0)
			return 0;

		buffer[0] = 0; /* flush cache */
#if TARGET_MSDOS == 32
		/* FIXME: We do not yet have a 32-bit protected mode version.
		 *        DOS extenders do not appear to translate AX=0x4403 properly */
#else
		__asm {
			push	ax
			push	bx
			push	cx
			push	dx
			push	ds
			mov	ax,0x4403		; IOCTL SMARTAAR CACHE CONTROL
			mov	bx,smartdrv_fd
			mov	cx,0x1			; 0x01 bytes
			lds	dx,word ptr [bptr]
			int	0x21
			jc	err1
			mov	rd,ax			; CF=0, AX=bytes written
err1:			pop	ds
			pop	dx
			pop	cx
			pop	bx
			pop	ax
		}
#endif

		if (rd == 0)
			return 0;
	}

	return 1;
}

int smartdrv_detect() {
	unsigned int rvax=0,rvbp=0;

	if (smartdrv_version == 0xFFFF) {
		/* Is Microsoft SMARTDRV 4.x or equivalent disk cache present? */

#if TARGET_MSDOS == 32
		__asm {
			push	eax
			push	ebx
			push	ecx
			push	edx
			push	esi
			push	edi
			push	ebp
			push	ds
			mov	eax,0x4A10		; SMARTDRV 4.xx INSTALLATION CHECK AND HIT RATIOS
			mov	ebx,0
			mov	ecx,0xEBAB		; "BABE" backwards
			xor	ebp,ebp
			int	0x2F			; multiplex (hope your DOS extender supports it properly!)
			pop	ds
			mov	ebx,ebp			; copy EBP to EBX. Watcom C uses EBP to refer to the stack!
			pop	ebp
			mov	rvax,eax		; we only care about EAX and EBP(now EBX)
			mov	rvbp,ebx
			pop	edi
			pop	esi
			pop	edx
			pop	ecx
			pop	ebx
			pop	eax
		}
#else
		__asm {
			push	ax
			push	bx
			push	cx
			push	dx
			push	si
			push	di
			push	bp
			push	ds
			mov	ax,0x4A10		; SMARTDRV 4.xx INSTALLATION CHECK AND HIT RATIOS
			mov	bx,0
			mov	cx,0xEBAB		; "BABE" backwards
			xor	bp,bp
			int	0x2F			; multiplex
			pop	ds
			mov	bx,bp			; copy BP to BX. Watcom C uses BP to refer to the stack!
			pop	bp
			mov	rvax,ax			; we only care about EAX and EBP(now EBX)
			mov	rvbp,bx
			pop	di
			pop	si
			pop	dx
			pop	cx
			pop	bx
			pop	ax
		}
#endif

		if ((rvax&0xFFFF) == 0xBABE && (rvbp&0xFFFF) >= 0x400 && (rvbp&0xFFFF) <= 0x5FF) {
			/* yup. SMARTDRV 4.xx! */
			smartdrv_version = (rvbp&0xFF00) + (((rvbp>>4)&0xF) * 10) + (rvbp&0xF); /* convert BCD to decimal */
		}

		if (smartdrv_version == 0xFFFF) {
			char buffer[0x28];
#if TARGET_MSDOS == 32
#else
			char far *bptr = (char far*)buffer;
#endif
			int rd=0;

			memset(buffer,0,sizeof(buffer));

			/* check for SMARTDRV 3.xx */
			smartdrv_fd = open("SMARTAAR",O_RDONLY);
			if (smartdrv_fd >= 0) {
				/* FIXME: The DOS library should provide a common function to do IOCTL read/write character "control channel" functions */
#if TARGET_MSDOS == 32
				/* FIXME: We do not yet have a 32-bit protected mode version.
				 *        DOS extenders do not appear to translate AX=0x4402 properly */
#else
				__asm {
					push	ax
					push	bx
					push	cx
					push	dx
					push	ds
					mov	ax,0x4402		; IOCTL SMARTAAR GET CACHE STATUS
					mov	bx,smartdrv_fd
					mov	cx,0x28			; 0x28 bytes
					lds	dx,word ptr [bptr]
					int	0x21
					jc	err1
					mov	rd,ax			; CF=0, AX=bytes read
err1:					pop	ds
					pop	dx
					pop	cx
					pop	bx
					pop	ax
				}
#endif

				/* NTS: Despite reading back 0x28 bytes of data, this IOCTL
				 *      returns AX=1 for some reason. */
				if (rd > 0 && rd <= 0x28) {
					if (buffer[0] == 1/*write through flag*/ && buffer[15] == 3/*major version number*/) { /* SMARTDRV 3.xx */
						smartdrv_version = ((unsigned short)buffer[15] << 8) + ((unsigned short)buffer[14]);
					}
					else {
						close(smartdrv_fd);
					}
				}
			}
		}

		/* didn't find anything. then no SMARTDRV */
		if (smartdrv_version == 0xFFFF)
			smartdrv_version = 0;
	}

	return (smartdrv_version != 0);
}
#endif

