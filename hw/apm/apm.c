/* apm.c
 *
 * Advanced Power Management BIOS library.
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box]
 *
 * This library is intended for DOS programs that want to communicate with the APM
 * BIOS interface on PC systems made since about 1994. Note that on systems built
 * since 1999 you may want to use the ACPI BIOS instead, however that interface is
 * quite a bit more complex. */
 
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/apm/apm.h>
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC */

struct apm_bios_ctx *apm_bios = NULL;

#if TARGET_MSDOS == 32
static void apm_bios_rm_call(struct dpmi_realmode_call *rc) {
	__asm {
		mov	ax,0x0300
		mov	bx,0x0015
		xor	cx,cx
		mov	edi,rc		; we trust Watcom has left ES == DS
		int	0x31		; call DPMI
	}
}
#endif

void apm_bios_free() {
	if (apm_bios) free(apm_bios);
	apm_bios = NULL;
}

int apm_bios_probe() { /* 8.8 version */
	apm_bios_free();

	apm_bios = (struct apm_bios_ctx*)malloc(sizeof(*apm_bios));
	if (apm_bios == NULL) return 0;
	memset(apm_bios,0,sizeof(*apm_bios));

	apm_bios->version_want = 0x102;
	apm_bios->major_neg = 1;
	apm_bios->minor_neg = 0;

#if TARGET_MSDOS == 32
	{
		struct dpmi_realmode_call rc={0};
		rc.eax = 0x5300;	/* AH=0x53 A=0x00 */
		apm_bios_rm_call(&rc);
		if (!(rc.flags & 1)) { /* CF=0 */
			apm_bios->major = (rc.eax >> 8) & 0xFF;
			apm_bios->minor = (rc.eax & 0xFF);
			apm_bios->signature = (rc.ebx & 0xFFFF);
			apm_bios->flags = (rc.ecx & 0xFFFF);
		}
	}
#else
	__asm {
		mov	ah,0x53
		xor	al,al
		xor	bx,bx
		int	0x15
		jc	err1
#if defined(__LARGE__) || defined(__COMPACT__)
		push	ds
		mov	si,seg apm_bios		; we need DS = segment of apm_bios variable
		mov	ds,si
		lds	si,apm_bios		; DS:SI = apm_bios
#else
		mov	si,apm_bios
#endif
		mov	byte ptr [si+0],ah
		mov	byte ptr [si+1],al
		mov	word ptr [si+2],bx
		mov	word ptr [si+4],cx
#if defined(__LARGE__) || defined(__COMPACT__)
		pop	ds
#endif
err1:
	}
#endif

	return (apm_bios->signature == APM_PM_SIG);
}

void apm_bios_update_capabilities() {
	apm_bios->capabilities = 0;
	apm_bios->batteries = 0;

#if TARGET_MSDOS == 32
	{
		struct dpmi_realmode_call rc={0};
		rc.eax = 0x5310;
		apm_bios_rm_call(&rc);
		if (!(rc.flags & 1)) { /* CF=0 */
			apm_bios->capabilities = rc.ecx & 0xFFFF;
			apm_bios->batteries = rc.ebx & 0xFF;
		}
	}
#else
	{
		unsigned char bat=0,err=0;
		unsigned short cap=0;

		__asm {
			mov	ax,0x5310
			xor	bx,bx
			int	0x15
			jnc	err1
			mov	err,ah
err1:			mov	bat,bl
			mov	cap,cx
		}

		if (err == 0) {
			apm_bios->capabilities = cap;
			apm_bios->batteries = bat;
		}
	}
#endif
}

/* FIXME: For the protected mode calls, this code needs to save AX, BX, CX, (E)SI, and (E)DI
 *        which contain all the info needed for protected mode selectors */
int apm_bios_connect(int n) {
	unsigned char err=0;

	if (apm_bios == NULL)
		return 0;
	if (n < APM_CONNECT_NONE || n > APM_CONNECT_32_PM)
		return 0;
	if (n == APM_CONNECT_NONE)
		n = 0x04; /* AH=0x04 disconnect function */

	/* NTS: for now, only the real-mode interface */
	if (n == APM_CONNECT_16_PM || n == APM_CONNECT_32_PM)
		return 0; /* anything but real mode not supported */

#if TARGET_MSDOS == 32
	{
		struct dpmi_realmode_call rc={0};
		rc.eax = 0x5300+n;	/* AH=0x53 A=0x01-0x04 */
		apm_bios_rm_call(&rc);
		if (rc.flags & 1) { /* CF=1 */
			err = (rc.eax >> 8) & 0xFF;
		}
	}
#else
	__asm {
		mov	ah,0x53
		mov	al,byte ptr n
		xor	bx,bx
		int	0x15
		jnc	err1
		mov	err,ah
err1:
	}
#endif

	/* translate "n" back to enum */
	if (n == 0x04) n = APM_CONNECT_NONE;

	/* success: BIOS returns success, or disconnect (AH=4) command says "not connected" */
	if (err == 0x00 || (err == 0x03 && n == 0x04)) {
		apm_bios->connection = n;
	}
	else if (err == 0x02) { /* err code 2 real mode interface connected */
		apm_bios->connection = APM_CONNECT_REALMODE;
	}
	else if (err == 0x05) { /* err code 5 16-bit prot mode interface connected */
		apm_bios->connection = APM_CONNECT_16_PM;
	}
	else if (err == 0x07) { /* err code 2 32-bit prot mode interface connected */
		apm_bios->connection = APM_CONNECT_32_PM;
	}

	/* if transitioning to an interface, negotiate a newer API */
	if (err == 0x00 && n > APM_CONNECT_NONE) {
		unsigned int neg_ver = apm_bios->version_want;
		unsigned short ww = 0;

		apm_bios->major_neg = 1;
		apm_bios->minor_neg = 0;
		if (apm_bios->major == 1 && apm_bios->minor != 0 && neg_ver >= 0x101 && neg_ver <= 0x1FF) {
#if TARGET_MSDOS == 32
			{
				struct dpmi_realmode_call rc={0};
				rc.eax = 0x530E;
				rc.ecx = neg_ver;
				apm_bios_rm_call(&rc);
				if (!(rc.flags & 1)) { /* CF=0 */
					ww = rc.eax & 0xFFFF;
				}
			}
#else
			__asm {
				mov	ax,0x530E
				xor	bx,bx
				mov	cx,neg_ver
				int	0x15
				jc	err2
				mov	ww,ax
err2:
			}
#endif

			if (ww >= 0x101 && ww <= 0x1FF) {
				apm_bios->major_neg = ww >> 8;
				apm_bios->minor_neg = ww & 0xFF;
			}
		}
	}

	if (n > APM_CONNECT_NONE && apm_bios->connection == APM_CONNECT_NONE)
		apm_bios_update_capabilities();

	apm_bios->last_error = err;
	return (apm_bios->connection == n);
}

int apm_bios_cpu_busy() {
	unsigned char err=0;

#if TARGET_MSDOS == 32
{
	struct dpmi_realmode_call rc={0};
	rc.eax = 0x5306;
	apm_bios_rm_call(&rc);
	if (rc.flags & 1) { /* CF=1 */
		err = (rc.eax >> 8) & 0xFF;
	}
}
#else
__asm {
	mov	ax,0x5306
	int	0x15
	jnc	err1
	mov	err,ah
err1:
}
#endif

	return (err == 0);
}

int apm_bios_cpu_idle() {
	unsigned char err=0;

#if TARGET_MSDOS == 32
	{
		struct dpmi_realmode_call rc={0};
		rc.eax = 0x5305;
		apm_bios_rm_call(&rc);
		if (rc.flags & 1) { /* CF=1 */
			err = (rc.eax >> 8) & 0xFF;
		}
	}
#else
	__asm {
		mov	ax,0x5305
		int	0x15
		jnc	err1
		mov	err,ah
err1:
	}
#endif

	return (err == 0);
}

signed long apm_bios_pm_evnet() {
	unsigned short ev=0,info=0;

#if TARGET_MSDOS == 32
	{
		struct dpmi_realmode_call rc={0};
		rc.eax = 0x530B;
		apm_bios_rm_call(&rc);
		if (!(rc.flags & 1)) { /* CF=0 */
			ev = rc.ebx & 0xFFFF;
			info = rc.ecx & 0xFFFF;
		}
	}
#else
	__asm {
		mov	ax,0x530B
		int	0x15
		jc	err1
		mov	ev,bx
		mov	info,cx
err1:
	}
#endif

	return (ev == 0 ? -1LL : (unsigned long)ev);
}

void apm_bios_update_status() {
#if TARGET_MSDOS == 32
	{
		struct dpmi_realmode_call rc={0};
		rc.eax = 0x530A;
		apm_bios_rm_call(&rc);
		if (!(rc.flags & 1)) { /* CF=0 */
			apm_bios->status_ac = (rc.ebx >> 8) & 0xFF;
			apm_bios->status_battery = (rc.ebx & 0xFF);
			apm_bios->status_battery_flag = (rc.ecx >> 8) & 0xFF;
			apm_bios->status_battery_life = (rc.ecx & 0xFF);
			apm_bios->status_battery_time = rc.edx & 0xFFFF;
		}
	}
#else
	{
		unsigned short b=0,c=0,d=0;

		__asm {
			mov	ax,0x530A
			int	0x15
			jc	err1
			mov	b,bx
			mov	c,cx
			mov	d,dx
err1:
		}

		apm_bios->status_ac = (b >> 8) & 0xFF;
		apm_bios->status_battery = (b & 0xFF);
		apm_bios->status_battery_flag = (c >> 8) & 0xFF;
		apm_bios->status_battery_life = (c & 0xFF);
		apm_bios->status_battery_time = d & 0xFFFF;
	}
#endif
}

int apm_bios_power_state(unsigned short state) {
	unsigned char err=0;

#if TARGET_MSDOS == 32
	{
		struct dpmi_realmode_call rc={0};
		rc.eax = 0x5307;
		rc.ebx = 0x0001;
		rc.ecx = state;
		apm_bios_rm_call(&rc);
		if (rc.flags & 1) { /* CF=0 */
			err = (rc.eax >> 8) & 0xFF;
		}
	}
#else
	{
		__asm {
			mov	ax,0x5307
			mov	bx,0x0001
			mov	cx,state
			int	0x15
			jnc	err1
			mov	err,ah
err1:
		}
	}
#endif

	return (err == 0);
}

