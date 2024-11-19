#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <malloc.h>
#include <fcntl.h>
#include <ctype.h>
#include <i86.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#include <hw/dos/doswin.h>

struct vgastate_t	vga_state;

#if defined(TARGET_PC98)

/* emulated subset for code migration */

int probe_vga() {
    update_state_from_vga();
    return 1;
}

void update_state_from_vga() {
	vga_state.vga_pos_x = 0;
	vga_state.vga_pos_y = 0;
	vga_state.vga_stride = 80;
	vga_state.vga_height = 25;
	vga_state.vga_width = 80;
	vga_state.vga_9wide = 0;
	vga_state.vga_ram_base = 0xA0000;
	vga_state.vga_ram_size = 0x03F00;

	vga_state.vga_graphics_ram = NULL;
	vga_state.vga_graphics_ram_fence = NULL;

# if TARGET_MSDOS == 32
	/* NTS: According to many sources, 32-bit DOS extenders tend to map the lower 1MB 1:1, so this is safe */
	/* NTS: If I remember correctly Windows 95 also did this for Win32 applications, for whatever reason! */
	vga_state.vga_alpha_ram = (uint16_t*)vga_state.vga_ram_base;
	vga_state.vga_alpha_ram_fence = (uint16_t*)(vga_state.vga_alpha_ram + vga_state.vga_ram_size);
# else
	vga_state.vga_alpha_ram = (uint16_t far*)MK_FP(vga_state.vga_ram_base>>4,vga_state.vga_ram_base&0xF);	/* A0000 -> A000:0000 */
	vga_state.vga_alpha_ram_fence = (uint16_t far*)(vga_state.vga_alpha_ram + vga_state.vga_ram_size);
# endif
}

#else

#ifdef TARGET_WINDOWS
# include <hw/dos/winfcon.h>
# include <windows/apihelp.h>
# include <windows/dispdib/dispdib.h>
# include <windows/win16eb/win16eb.h>
#endif

void vga_sync_hw_cursor() {
	unsigned int i;
	i = vga_read_CRTC(0xF);			/* cursor low */
	i |= vga_read_CRTC(0xE) << 8;		/* cursor high */
	vga_state.vga_pos_x = i % vga_state.vga_stride;
	vga_state.vga_pos_y = i / vga_state.vga_stride;
}

void update_state_vga_memory_map_select(unsigned char c) {
	switch (c) {
		case 0: vga_state.vga_ram_base = 0xA0000; vga_state.vga_ram_size = 0x20000; break;
		case 1: vga_state.vga_ram_base = 0xA0000; vga_state.vga_ram_size = 0x10000; break;
		case 2: vga_state.vga_ram_base = 0xB0000; vga_state.vga_ram_size = 0x08000; break;
		case 3: vga_state.vga_ram_base = 0xB8000; vga_state.vga_ram_size = 0x08000; break;
	}

#if defined(TARGET_WINDOWS)
	if (vga_state.vga_ram_base == 0xA0000)
		vga_state.vga_graphics_ram = (unsigned char FAR*)DisplayDibGetScreenPointerA000();
	else if (vga_state.vga_ram_base == 0xB0000)
		vga_state.vga_graphics_ram = (unsigned char FAR*)DisplayDibGetScreenPointerB000();
	else if (vga_state.vga_ram_base == 0xB8000)
		vga_state.vga_graphics_ram = (unsigned char FAR*)DisplayDibGetScreenPointerB800();
	else
		vga_state.vga_graphics_ram = (unsigned char FAR*)DisplayDibGetScreenPointerA000();

	vga_state.vga_graphics_ram_fence = vga_state.vga_graphics_ram + vga_state.vga_ram_size;
	vga_state.vga_alpha_ram = (uint16_t FAR*)vga_state.vga_graphics_ram;
	vga_state.vga_alpha_ram_fence = (uint16_t FAR*)vga_state.vga_graphics_ram_fence;
#else
# if TARGET_MSDOS == 32
	/* NTS: According to many sources, 32-bit DOS extenders tend to map the lower 1MB 1:1, so this is safe */
	/* NTS: If I remember correctly Windows 95 also did this for Win32 applications, for whatever reason! */
	vga_state.vga_graphics_ram = (unsigned char*)vga_state.vga_ram_base;
	vga_state.vga_graphics_ram_fence = vga_state.vga_graphics_ram + vga_state.vga_ram_size;
	vga_state.vga_alpha_ram = (uint16_t*)vga_state.vga_graphics_ram;
	vga_state.vga_alpha_ram_fence = (uint16_t*)vga_state.vga_graphics_ram_fence;
# else
	vga_state.vga_graphics_ram = MK_FP(vga_state.vga_ram_base>>4,vga_state.vga_ram_base&0xF);	/* A0000 -> A000:0000 */
	vga_state.vga_graphics_ram_fence = MK_FP((vga_state.vga_ram_base+vga_state.vga_ram_size)>>4,(vga_state.vga_ram_base+vga_state.vga_ram_size)&0xF);
	vga_state.vga_alpha_ram = (uint16_t far*)vga_state.vga_graphics_ram;
	vga_state.vga_alpha_ram_fence = (uint16_t far*)vga_state.vga_graphics_ram_fence;
# endif
#endif
}

void update_state_from_vga() {
	unsigned char c;

	vga_state.vga_pos_x = 0;
	vga_state.vga_pos_y = 0;
	vga_state.vga_stride = 80;
	vga_state.vga_height = 25;
	vga_state.vga_width = 80;
	vga_state.vga_9wide = 0;

	if (vga_state.vga_flags & VGA_IS_VGA) { /* VGA only. EGA cards DO have the misc. output reg but it's write-only */
		/* update state from H/W which I/O port */
		c = inp(0x3CC);
		if (c & 1) {
			vga_state.vga_base_3x0 = 0x3D0;
		}
		else {
			vga_state.vga_base_3x0 = 0x3B0;
		}

		/* now ask the graphics controller where/how VGA memory is mapped */
		c = vga_read_GC(6);
		/* bit 0 = alpha disable (if set, graphics) */
		vga_state.vga_alpha_mode = ((c & 1) == 0);
		/* bits 2-3 memory map select */
		update_state_vga_memory_map_select((c>>2)&3);

		/* read the sequencer: are we in 8 or 9 dot mode? */
		c = vga_read_sequencer(0x1);
		vga_state.vga_9wide = (c & 1) == 0;

		/* read from the CRTC controller the stride, width, and height */
		vga_state.vga_stride = vga_read_CRTC(0x13) * 2;	/* "offset" register */
		if (vga_state.vga_alpha_mode) {
			vga_state.vga_width = vga_state.vga_stride;
			vga_sync_hw_cursor();
			/* TODO: read vertical blank values and calculate active area, then divide by scan line height, to get alpha height */
			/* TODO: read horizontal blank values to calculate active area, then visible width */
		}
		else {
			/* TODO: similar semantics for graphics mode */
		}
	}
	else if (vga_state.vga_flags & VGA_IS_EGA) {
		/* Well the EGA has similar registers BUT they aren't readable. So we have to
		 * guess based on other information handed to us */

		/* reading the misc. output register doesn't work, use BIOS data area */
#if defined(TARGET_WINDOWS)
		c = 0xD0;/*TODO*/
#else
# if TARGET_MSDOS == 32
		c = *((unsigned char*)0x463);
# else
		c = *((unsigned char far*)MK_FP(0x40,0x63));
# endif
#endif
		if ((c&0xF0) == 0xD0)
			vga_state.vga_base_3x0 = 0x3D0;
		else if ((c&0xF0) == 0xB0)
			vga_state.vga_base_3x0 = 0x3B0;
		else {
			vga_state.vga_base_3x0 = 0x3D0;
		}

		/* reading from the graphics controller (0x3CE) doesn't work, deduce from BIOS mode */
		c = int10_getmode();
		switch (c) {
			case 0: case 1: case 2: case 3: case 7:
				vga_state.vga_alpha_mode = 1;

 /* the best we can do is assume B0000 if CRTC is at 3Bx or B8000 if at 3Dx even though it's possible to map at B8000 and 3Bx */
				if (vga_state.vga_base_3x0 == 0x3B0)
					update_state_vga_memory_map_select(2);
				else
					update_state_vga_memory_map_select(3);
				break;
			case 4: case 5: case 6:
				vga_state.vga_alpha_mode = 0;
				update_state_vga_memory_map_select(3);
				break;
			case 13: case 14: case 15: case 16: case 17: case 18: default:
				vga_state.vga_alpha_mode = 0;
				update_state_vga_memory_map_select(1);
				break;
		}

		/* read from the CRTC controller the stride, width, and height */
		vga_state.vga_stride = vga_read_CRTC(0x13) * 2;	/* "offset" register */
		if (vga_state.vga_alpha_mode) {
			vga_state.vga_width = vga_state.vga_stride;
			vga_sync_hw_cursor();
			/* TODO: read vertical blank values and calculate active area, then divide by scan line height, to get alpha height */
			/* TODO: read horizontal blank values to calculate active area, then visible width */
		}
		else {
			/* TODO: similar semantics for graphics mode */
		}
	}
	else if (vga_state.vga_flags & (VGA_IS_CGA|VGA_IS_MCGA)) {
		vga_state.vga_base_3x0 = 0x3D0; /* always at 0x3Dx */

		/* TODO: If Tandy, detect state */

		/* read the status register to determine the state of the CGA... oh wait... we can't.
		 * fine. deduce it from the BIOS video mode. */
		c = int10_getmode();
		switch (c) {
			case 0: case 1: case 2: case 3: case 7:
				vga_state.vga_alpha_mode = 1;
				break;
			default:
				vga_state.vga_alpha_mode = 0;
				break;
		}

		if (c <= 1) {
			vga_state.vga_stride = 40;
			vga_state.vga_width = 40;
		}

		if (c >= 0x08)
			update_state_vga_memory_map_select(1); /* 0xA0000 */
		else
			update_state_vga_memory_map_select(3); /* 0xB8000 */
	}
	else if (vga_state.vga_flags & VGA_IS_MDA) {
		vga_state.vga_base_3x0 = 0x3B0; /* always at 0x3Bx */
		vga_state.vga_alpha_mode = 1; /* stock MDA doesn't have graphics */

		/* Hercules MDA: It would be nice to be able to read bit 2 of the display control,
		 *               except that the port is write-only. Thanks >:( */
		update_state_vga_memory_map_select(2); /* 0xB0000 */
	}

	vga_state.vga_draw_stride_limit = vga_state.vga_draw_stride = vga_state.vga_stride;
}

static const uint16_t vga_dcc_table[0x0D] = {
	/*0x00*/VGA_NO_MONITOR,
	/*0x01*/VGA_IS_MDA | VGA_IS_DIGITAL | VGA_IS_MONOCHROME,
	/*0x02*/VGA_IS_CGA | VGA_IS_DIGITAL,
	/*0x03*/0,
	/*0x04*/VGA_IS_CGA | VGA_IS_EGA | VGA_IS_DIGITAL,
	/*0x05*/VGA_IS_CGA | VGA_IS_EGA | VGA_IS_DIGITAL | VGA_IS_MONOCHROME,
	/*0x06*/VGA_IS_CGA | VGA_IS_PGA | VGA_IS_ANALOG,
	/*0x07*/VGA_IS_CGA | VGA_IS_EGA | VGA_IS_VGA | VGA_IS_ANALOG | VGA_IS_MONOCHROME,
	/*0x08*/VGA_IS_CGA | VGA_IS_EGA | VGA_IS_VGA | VGA_IS_ANALOG,
	/*0x09*/0,
	/*0x0A*/VGA_IS_CGA | VGA_IS_MCGA | VGA_IS_DIGITAL,
	/*0x0B*/VGA_IS_CGA | VGA_IS_MCGA | VGA_IS_ANALOG | VGA_IS_MONOCHROME,
	/*0x0C*/VGA_IS_CGA | VGA_IS_MCGA | VGA_IS_ANALOG
};

static const uint16_t vga_ega_switches_table[0x0C] = {
	/*0x00*/VGA_IS_MDA | VGA_IS_DIGITAL | VGA_IS_MONOCHROME,
	/*0x01*/VGA_IS_MDA | VGA_IS_DIGITAL | VGA_IS_MONOCHROME,
	/*0x02*/VGA_IS_MDA | VGA_IS_DIGITAL | VGA_IS_MONOCHROME,
	/*0x03*/VGA_IS_MDA | VGA_IS_DIGITAL | VGA_IS_MONOCHROME,
	/*0x04*/VGA_IS_CGA | VGA_IS_DIGITAL,
	/*0x05*/VGA_IS_CGA | VGA_IS_DIGITAL,
	/*0x06*/VGA_IS_CGA | VGA_IS_EGA | VGA_IS_DIGITAL,
	/*0x07*/VGA_IS_CGA | VGA_IS_EGA | VGA_IS_DIGITAL,
	/*0x08*/VGA_IS_CGA | VGA_IS_EGA | VGA_IS_DIGITAL,
	/*0x09*/VGA_IS_CGA | VGA_IS_EGA | VGA_IS_DIGITAL,
	/*0x0A*/VGA_IS_CGA | VGA_IS_EGA | VGA_IS_DIGITAL | VGA_IS_MONOCHROME,
	/*0x0B*/VGA_IS_CGA | VGA_IS_EGA | VGA_IS_DIGITAL | VGA_IS_MONOCHROME

};

int probe_vga() {
#if defined(TARGET_WINDOWS)
	/* TODO: More comprehensive tests! */
	vga_state.vga_flags |= VGA_IS_VGA | VGA_IS_EGA | VGA_IS_CGA | VGA_IS_ANALOG;
	update_state_from_vga();
	return 1;
#else
	union REGS regs = {0};

	vga_state.vga_flags = 0;
	vga_state.vga_base_3x0 = 0;

	/* apparently the best way is to ask the VGA BIOS */
	/* according to sources I have this function only works on VGA BIOSes and nothing prior to that */
	{
		regs.w.ax = 0x1A00;
#if TARGET_MSDOS == 32
		int386(0x10,&regs,&regs);
#else
		int86(0x10,&regs,&regs);
#endif
		if (regs.h.al == 0x1A) {
			if (regs.h.bl < (sizeof(vga_dcc_table)/sizeof(vga_dcc_table[0])))
				vga_state.vga_flags = vga_dcc_table[regs.h.bl];
		}
	}

	/* if that didn't work, then assume it's not VGA, and probe around a bit */
	/* are you an EGA? Determine by BIOS */
	if (vga_state.vga_flags == 0) {
		regs.w.ax = 0x1200;
		regs.w.bx = 0xFF10;
#if TARGET_MSDOS == 32
		int386(0x10,&regs,&regs);
#else
		int86(0x10,&regs,&regs);
#endif
		if (regs.h.bh != 0xFF) { /* so, if BH changes the EGA BIOS or higher is present? */
			if (regs.h.cl < (sizeof(vga_ega_switches_table)/sizeof(vga_ega_switches_table[0])))
				vga_state.vga_flags = vga_ega_switches_table[regs.h.cl];
		}
	}

	/* If nothing found yet, then it's either CGA or MDA. At this point, ask the BIOS.
	 * Probing I/O ports on old hardware is not advised because the undefined ports
	 * may return values other than 0xFF especially with aging components. */
	if (vga_state.vga_flags == 0) {
		unsigned short equip = 0;

		__asm {
			push    ax
			int     11h
			mov     equip,ax
			pop     ax
		}

		/* bits [5:4]
		 *   11 = TTL monochrome (MDA)
		 *   10 = 80-column CGA
		 *   01 = 40-column CGA
		 *   00 = ?? */
		if ((equip & (3 << 4)) == (3 << 4))
			vga_state.vga_flags = VGA_IS_MDA | VGA_IS_DIGITAL | VGA_IS_MONOCHROME;
		else if ((equip & (3 << 4)) != (0 << 4))
			vga_state.vga_flags = VGA_IS_CGA | VGA_IS_DIGITAL;
	}

	/* if it looks like CGA, it might be PCjr */
	if ((vga_state.vga_flags & VGA_IS_CGA) && !(vga_state.vga_flags & (VGA_IS_PCJR|VGA_IS_TANDY))) {
		unsigned char mb; /* Look at the BIOS model byte */

#if TARGET_MSDOS == 32
		mb = *((unsigned char*)0xFFFFE);
#else
		mb = *((unsigned char far*)MK_FP(0xF000,0xFFFE));
#endif

		if (mb == 0xFD)
			vga_state.vga_flags |= VGA_IS_PCJR;
	}

	/* if it looks like CGA, it might be Tandy */
	if ((vga_state.vga_flags & VGA_IS_CGA) && !(vga_state.vga_flags & (VGA_IS_PCJR|VGA_IS_TANDY))) {
		/* Detect by the 3-voice chip.
		 * FIXME: Is there a better way? */
		{
			union REGS regs = {0};
			regs.w.ax = 0x8100;
#if TARGET_MSDOS == 32
			int386(0x1A,&regs,&regs);
#else
			int86(0x1A,&regs,&regs);
#endif
			if (regs.h.al & 0x80) {
				if ((regs.w.cflag & 1) == 0) { /* and sound chip is free? CF=0 */
					vga_state.vga_flags |= VGA_IS_TANDY | VGA_IS_DIGITAL;
				}
			}
		}

		/* what about Amstrad? */
		{
			union REGS regs = {0};
			regs.w.ax = 0x0600;
			regs.w.bx = 0;
			regs.w.cflag = 0;
#if TARGET_MSDOS == 32
			int386(0x15,&regs,&regs);
#else
			int86(0x15,&regs,&regs);
#endif
			if (regs.w.bx != 0 && !(regs.w.cflag & 1)) {
				vga_state.vga_flags |= VGA_IS_AMSTRAD | VGA_IS_DIGITAL;
				/* TODO: If I read the Amstrad tech manual correctly, their video hardware also emulates Hercules modes, right? */
				/* TODO: I get the impression Amstrad graphics do not include Tandy modes, is that correct? */
				//vga_state.vga_flags &= ~VGA_IS_TANDY; /* <- if so, uncomment this line */
			}
		}
	}

	/* If it looks like an MDA, then it might be helpful to tell whether it's a
	 * Hercules graphics card */
	if (vga_state.vga_flags & VGA_IS_MDA) {
		unsigned char cm,c;
		unsigned int hsyncs = 0; /* in case we're on a slow machine use hsync count to know when to stop */
		unsigned int patience = 0xFFFF; /* NTS: 0xFFFF on actual hardware of that era might turn into a considerable and unnecessary pause */
		cm = inp(0x3BA);
		while (--patience != 0 && ((c=inp(0x3BA)) & 0x80) == (cm & 0x80)) {
			if ((c^cm) & 1) { /* HSYNC change? */
				cm ^= 1;
				if (c & 1) { /* did HSYNC start? */
					if (++hsyncs >= 600) break; /* if we've gone 600 hsyncs without seeing VSYNC change then give up */
				}
			}
			inp(0x80);
		}
		if (patience > 0 && (c^cm) & 0x80) { /* if it changed, then HGC */
			vga_state.vga_flags |= VGA_IS_HGC;
			vga_state.vga_hgc_type = (c >> 4) & 7;
			switch ((c>>4)&7) {
				case 5: case 1: vga_state.vga_hgc_type = (c>>4)&7; break;
				default: vga_state.vga_hgc_type = 0; break;
			}
		}
	}

done:
	update_state_from_vga();
	return 1;
#endif
}

#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 32
unsigned char int10_getmode() {
	unsigned char *base,*p,ret;
	DWORD code;

	if ((base=p=LockWin16EBbuffer()) == NULL)
		return 3;

	/* force creation of the code alias */
	code = GetCodeAliasWin16EBbuffer();
	if (code == 0) {
		UnlockWin16EBbuffer();
		return 3;
	}

	/* MOV AX,0F00 */
	*p++ = 0xB8; *((WORD FAR*)p) = 0x0F00; p+=2;
	/* INT 10h */
	*p++ = 0xCD; *p++ = 0x10;

	/* RETF */
	*p++ = 0xCB;

	/* run it! */
	ret = (unsigned char)ExecuteWin16EBbuffer(code);
	UnlockWin16EBbuffer();
	return ret;
}

void int10_setmode(unsigned char mode) {
	unsigned char *base,*p;
	DWORD code;

	if ((base=p=LockWin16EBbuffer()) == NULL)
		return;

	/* force creation of the code alias */
	code = GetCodeAliasWin16EBbuffer();
	if (code == 0) {
		UnlockWin16EBbuffer();
		return;
	}

	/* MOV AX,<x> */
	*p++ = 0xB8; *((WORD FAR*)p) = mode; p+=2;
	/* INT 10h */
	*p++ = 0xCD; *p++ = 0x10;
	/* RETF */
	*p++ = 0xCB;

	/* run it! */
	ExecuteWin16EBbuffer(code);
	UnlockWin16EBbuffer();
}

#endif

#endif

