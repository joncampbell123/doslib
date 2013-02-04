/* emm.c
 *
 * Expanded Memory Manager library.
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

/* api library for DOS programs that want to use Expanded Memory (usually, EMM386.EXE)
 *
 * NOTES:
 *    This code is intended for use with 16-bit real-mode programs. 32-bit programs have whatever the DOS extender
 *    offers and have no need for expanded memory, in fact, the DOS extender will often take up all extended
 *    & expanded memory for it's use and leave us nothing, which is why 32-bit builds of this library do not
 *    function.
 *
 * Testing:
 *
 *    YES* = Yes, if DOS underneath provides it (or if DOS, when configured to load EMM386.EXE). Otherwise, No
 *
 * System/configuration                 Works?   Limit?
 * DOSBox 0.74                          YES      NO
 * DOSBox 0.74 +
 *   Microsoft Windows 3.0
 *     Real mode                        YES*     NO
 *     Standard mode                    NO       --                      EMM functions present, but will always report 0KB free. If more than 16MB of RAM is present, Windows causes a serious fault and DOSBox aborts
 *     386 Enhanced mode                YES*     ?
 *   Microsoft Windows 3.1
 *     Standard mode                    NO       --                      EMM functions present, but will always report 0KB free
 *     386 Enhanced mode                YES*     NO
 *   Microsoft Windows 3.11
 *     Standard mode                    NO       --                      EMM functions present, but will always report 0KB free
 *     386 Enhanced mode                YES*     NO
 * QEMU/VirtualBox
 *   Microsoft Windows 95 (4.00.950)[1]
 *     Normal mode                      YES*     64MB                    API usually reports 16MB free. The test VM had 96MB of RAM
 *     Safe mode                        YES*     64MB
 *     MS-DOS mode (official)           YES*     32MB
 *     MS-DOS mode (gui=0)              YES*     32MB
 *   Microsoft Windows 98 (4.10.1998)[1]
 *     Normal mode                      YES*     64MB                    API usually reports 16MB free. The test VM had 96MB of RAM
 *     MS-DOS mode (gui=0)              YES*     32MB
 *   Microsoft Windows ME (4.90.3000)[2]
 *     Normal mode                      YES*     64MB                    The API will never report more than 16MB free, but you can hack
 *                                                                       the PIF editor for the DOS program to allow up to 65534KB of
 *                                                                       EMM memory. The test program seems to have no problem allocating
 *                                                                       48MB of expanded memory when said hack is applied. I suppose the
 *                                                                       API could handle more, but remember limits are imposed by the
 *                                                                       DOS Box VM and those are apparently represented by unsigned
 *                                                                       16-bit integers, thus the 64MB (65534KB) limit.
 *     MS-DOS mode (bootdisk)           ?        ?                       I am unable to get Windows ME to make a bootdisk at this time.
 *                                                                       So I attemped to use a Windows ME bootdisk from bootdisk.com,
 *                                                                       and added DEVICE=EMM386.EXE only to find that at boot time
 *                                                                       it locks up the computer! So, I have no way of knowing what
 *                                                                       a pure MS-DOS mode EMM386.EXE from Windows ME does. It probably
 *                                                                       acts just like the Windows 95/98 versions...
 *   Microsoft Windows 2000 Professional
 *     Normal mode                      YES      32MB                    For whatever reason NTVDM defaults to NOT providing EMM memory.
 *                                                                       Limits to 32MB even if you type in larger values in the PIF editor.
 *
 * [1] EMM386.EXE for these systems will not be able to automatically find a page frame in QEMU or VirtualBox, probably because for
 *     unmapped regions the emulator returns 0x00 not 0xFF. To work around that, open CONFIG.SYS in a text editor and edit the
 *     line referring to EMM386.EXE. Add I=E000-EFFF and save. It should look like:
 *
 *     DEVICE=C:\WINDOWS\EMM386.EXE I=E000-EFFF.
 *
 * [2] You're probably wondering... if Windows ME ignores AUTOEXEC.BAT and CONFIG.SYS then how the hell do you get EMM386.EXE
 *     loaded? Well, it's very obscure and undocumented, but you can get it loaded on boot up as follows:
 *
 *        1. Go to the start menu, select "run" and type "notepad c:\windows\system.ini"
 *        2. Locate the [386Enh] section, go to the bottom of the section, and add the following lines of text to the end of [386Enh]
 *
 *             EMMInclude=E000-EFFF
 *             ReservePageFrame=yes
 *
 *        3. Reboot, and enjoy
 */
#if !defined(TARGET_OS2) && !defined(TARGET_WINDOWS)

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/emm.h>

unsigned char emm_status = 0xFF; /* initialize to 0xFF as a way of indicating that nobody checked yet */
unsigned char emm_present = 0;
unsigned char emm_version = 0;
unsigned char emm_phys_pages = 0;
unsigned short emm_total_pages = 0;
unsigned int emm_page_frame_segment = 0;
unsigned short emm_unallocated_pages = 0;
struct emm_phys_page_map *emm_phys_map = NULL;	/* maps physical page number -> segment address */
static const char *devname = "EMMXXXX0";
static const char *devname2 = "EMMQXXX0"; /* Microsoft publishes EMM standard then breaks it subtly in non-backwards compatible way... news at 11 */
#if TARGET_MSDOS == 32 && !defined(TARGET_OS2)
static uint16_t emm_phys_map_sel = 0;

static void emm_realmode_67_call(struct dpmi_realmode_call *rc) {
	__asm {
		mov	ax,0x0300
		mov	bx,0x0067
		xor	cx,cx
		mov	edi,rc		; we trust Watcom has left ES == DS
		int	0x31		; call DPMI
	}
}
#endif

void emm_phys_pages_sort() {
	/* TODO */
}

#if TARGET_MSDOS == 16 && !defined(TARGET_OS2)
void emm_update_page_count() {
	emm_unallocated_pages = 0;
	emm_total_pages = 0;

	if (!emm_present) return;

	__asm {
		mov	ah,0x42
		push	es
		int	0x67
		pop	es
		mov	emm_unallocated_pages,bx
		mov	emm_total_pages,dx
	}
}

int probe_emm() {
	void far *emmptr;

	emm_present = 0;
	emmptr = (void far*)_dos_getvect(0x67);
	if (emmptr == (void far*)0)
		return 0;

	/* apparently 10 bytes into the segment there is the magic string */
	if (	_fmemcmp((char far*)MK_FP(FP_SEG(emmptr),0x000A),(char far*)devname,8) != 0 &&
		_fmemcmp((char far*)MK_FP(FP_SEG(emmptr),0x000A),(char far*)devname2,8) != 0)
		return 0;

	emm_present = 1;
	emm_phys_pages = 1;
	emm_page_frame_segment = 0;

	__asm {
		mov	ah,0x40
		push	es
		int	0x67
		pop	es
		mov	emm_status,ah

		mov	ah,0x41
		push	es
		int	0x67
		pop	es
		or	ah,ah
		jnz	pfn_end
		mov	emm_page_frame_segment,bx

		mov	ah,0x46
		push	es
		int	0x67
		pop	es
		mov	emm_version,al
pfn_end:
	}

	if (emm_phys_map != NULL) {
		free(emm_phys_map);
		emm_phys_map = NULL;
	}

	if (emm_phys_map == NULL) {
		/* see if the EMM provides a mapping table describing the real-mode segments
		 * corresponding to each physical page. if not, then assume only one page
		 * available. the table could be up to 256 entries. the API really doesn't
		 * have a way to tell us ahead of time, so assume the worst. */
		assert(sizeof(struct emm_phys_page_map) == (size_t)4);
		emm_phys_map = malloc(sizeof(struct emm_phys_page_map) * 256);
		if (emm_phys_map != NULL) {
			const unsigned int s = FP_SEG(emm_phys_map);
			const unsigned int o = FP_OFF(emm_phys_map);
			unsigned int c = 0;
			__asm {
				push	es
				mov	ax,0x5800
				mov	di,s
				mov	es,di
				mov	di,o
				int	0x67
				or	ah,ah
				jnz	fail
				mov	c,cx
fail:				pop	es
			}

			if (c == 0) {
				free(emm_phys_map);
				emm_phys_map = NULL;
			}
			else {
				emm_phys_pages = c;
				if (c < 256) {
					void *x = realloc(emm_phys_map,sizeof(struct emm_phys_page_map) * c);
					if (x != NULL) { /* NTS: if we cannot realloc, well, too bad */
						emm_phys_map = x;
					}
				}

				/* WARNING: we are assuming several things about the table.
				 *  - That the table is sorted by real-mode segment (as described in the standard)
				 *  - There are no duplicate page numbers
				 *  - The table has as many entries as physical pages */

				/* do ourself a favor and sort by page number the table */
				emm_phys_pages_sort();
			}
		}
	}

	return 1;
}

int emm_alloc_pages(unsigned int pages) {
	int handle = -1;

	if (emm_present) {
		__asm {
			mov	ah,0x43
			mov	bx,pages
			push	es
			int	0x67
			pop	es
			or	ah,ah
			jnz	fail
			mov	handle,dx
fail:
		}
	}

	return handle;
}

int emm_free_pages(unsigned int handle) {
	int retv = 0;

	if (emm_present) {
		__asm {
			mov	ah,0x45
			mov	dx,handle
			push	es
			int	0x67
			pop	es
			or	ah,ah
			jnz	fail
			mov	retv,1
fail:
		}
	}

	return retv;
}

int emm_map_page(unsigned int handle,unsigned int phys_page,unsigned int log_page) {
	int retv = 0;

	if (phys_page >= (unsigned int)emm_phys_pages)
		return 0;

	if (emm_present) {
		__asm {
			mov	ah,0x44
			mov	al,byte ptr phys_page
			mov	bx,log_page
			mov	dx,handle
			push	es
			int	0x67
			pop	es
			or	ah,ah
			jnz	fail
			mov	retv,1
fail:
		}
	}

	return retv;
}

/* given physical page number, return real-mode segment value */
unsigned short emm_last_phys_page_segment(unsigned int phys_page) {
	unsigned int i;

	if (phys_page >= (unsigned int)emm_phys_pages)
		return 0;

	/* if we don't have a copy of the EMM's mapping table, then assume that there is
	 * only physical page 0 at the page frame address */
	if (phys_page == 0 && emm_phys_map == NULL)
		return emm_page_frame_segment;

	for (i=0;i < emm_phys_pages && emm_phys_map != NULL;i++) {
		struct emm_phys_page_map *me = emm_phys_map + i;
		if (phys_page == me->number)
			return me->segment;
	}

	return 0;
}
#else
void emm_update_page_count() {
	emm_unallocated_pages = 0;
	emm_total_pages = 0;

	if (!emm_present) return;

	__asm {
		mov	ah,0x42
		push	es
		int	0x67
		pop	es
		mov	emm_unallocated_pages,bx
		mov	emm_total_pages,dx
	}
}

int probe_emm() {/*32-bit*/
	unsigned int emm_seg;

	sanity();
	emm_present = 0;
	/* Tricky. The DOS extender would likely translate the vector, when what we
	   really want is the segment value of int 67h */
	emm_seg = *((uint16_t*)((0x67 << 2) + 2));
	sanity();

	/* apparently 10 bytes into the segment there is the magic string */
	if (	memcmp((void*)(((unsigned long)emm_seg << 4UL) + 0x000A),devname,8) != 0 &&
		memcmp((void*)(((unsigned long)emm_seg << 4UL) + 0x000A),devname2,8) != 0)
		return 0;

	sanity();
	emm_present = 1;
	emm_phys_pages = 1;
	emm_page_frame_segment = 0;

	__asm {
		mov	ah,0x40
		push	es
		int	0x67
		pop	es
		mov	emm_status,ah

		mov	ah,0x41
		push	es
		int	0x67
		pop	es
		or	ah,ah
		jnz	pfn_end
		mov	word ptr emm_page_frame_segment,bx

		mov	ah,0x46
		push	es
		int	0x67
		pop	es
		mov	emm_version,al
pfn_end:
	}
	sanity();

	if (emm_phys_map != NULL) {
		dpmi_free_dos(emm_phys_map_sel);
		emm_phys_map_sel = 0;
		emm_phys_map = NULL;
	}

	if (emm_phys_map == NULL) {
		/* see if the EMM provides a mapping table describing the real-mode segments
		 * corresponding to each physical page. if not, then assume only one page
		 * available. the table could be up to 256 entries. the API really doesn't
		 * have a way to tell us ahead of time, so assume the worst. */
		assert(sizeof(struct emm_phys_page_map) == (size_t)4);
		emm_phys_map = dpmi_alloc_dos(sizeof(struct emm_phys_page_map) * 256,&emm_phys_map_sel);
		if (emm_phys_map != NULL) {
			const unsigned int s = ((uint32_t)emm_phys_map) >> 4;
			const unsigned int o = ((uint32_t)emm_phys_map) & 0xF;
			struct dpmi_realmode_call rc={0};
			unsigned int c = 0;

			rc.eax = 0x5800;
			rc.edi = o;
			rc.es = s;
			rc.ds = s;
			emm_realmode_67_call(&rc);
			if ((rc.eax&0xFF) == 0) c = rc.ecx & 0xFFFF;

			if (c == 0) {
				dpmi_free_dos(emm_phys_map_sel);
				emm_phys_map_sel = 0;
				emm_phys_map = NULL;
			}
			else {
				emm_phys_pages = c;

				/* WARNING: we are assuming several things about the table.
				 *  - That the table is sorted by real-mode segment (as described in the standard)
				 *  - There are no duplicate page numbers
				 *  - The table has as many entries as physical pages */

				/* do ourself a favor and sort by page number the table */
				emm_phys_pages_sort();
			}
		}
	}

	return 1;
}

int emm_alloc_pages(unsigned int pages) {
	int handle = -1;

	if (emm_present) {
		__asm {
			mov	ah,0x43
			mov	ebx,pages
			push	es
			int	0x67
			pop	es
			or	ah,ah
			jnz	fail
			and	edx,0xFFFF
			mov	handle,edx
fail:
		}
	}

	return handle;
}

int emm_free_pages(unsigned int handle) {
	int retv = 0;

	if (emm_present) {
		__asm {
			mov	ah,0x45
			mov	edx,handle
			push	es
			int	0x67
			pop	es
			or	ah,ah
			jnz	fail
			mov	retv,1
fail:
		}
	}

	return retv;
}

int emm_map_page(unsigned int handle,unsigned int phys_page,unsigned int log_page) {
	int retv = 0;

	if (phys_page >= (unsigned int)emm_phys_pages)
		return 0;

	if (emm_present) {
		__asm {
			mov	ah,0x44
			mov	al,byte ptr phys_page
			mov	ebx,log_page
			mov	edx,handle
			push	es
			int	0x67
			pop	es
			or	ah,ah
			jnz	fail
			mov	retv,1
fail:
		}
	}

	return retv;
}

unsigned short emm_last_phys_page_segment(unsigned int phys_page) {
	unsigned int i;

	if (phys_page >= (unsigned int)emm_phys_pages)
		return 0;

	/* if we don't have a copy of the EMM's mapping table, then assume that there is
	 * only physical page 0 at the page frame address */
	if (phys_page == 0 && emm_phys_map == NULL)
		return emm_page_frame_segment;

	for (i=0;i < emm_phys_pages && emm_phys_map != NULL;i++) {
		struct emm_phys_page_map *me = emm_phys_map + i;
		if (phys_page == me->number)
			return me->segment;
	}

	return 0;
}
#endif

#endif /* !defined(TARGET_OS2) && !defined(TARGET_WINDOWS) */

