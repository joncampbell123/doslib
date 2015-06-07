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

#ifdef TARGET_WINDOWS
# include <hw/dos/winfcon.h>
# include <windows/apihelp.h>
# include <windows/dispdib/dispdib.h>
# include <windows/win16eb/win16eb.h>
#endif

VGA_RAM_PTR	vga_graphics_ram = NULL;
VGA_RAM_PTR	vga_graphics_ram_fence = NULL;
VGA_ALPHA_PTR	vga_alpha_ram = NULL;
VGA_ALPHA_PTR	vga_alpha_ram_fence = NULL;

unsigned char	vga_pos_x = 0,vga_pos_y = 0,vga_color = 0x7;
unsigned char	vga_width = 80,vga_height = 25;
unsigned char	vga_alpha_mode = 0;
unsigned char	vga_hgc_type = 0;
unsigned int	vga_base_3x0 = 0;
unsigned long	vga_ram_base = 0;
unsigned long	vga_ram_size = 0;
unsigned char	vga_stride = 80;
unsigned short	vga_flags = 0;
unsigned char	vga_9wide = 0;

/* HGC values for HGC graphics */
static unsigned char hgc_graphics_crtc[12] = {
	0x35,0x2D,0x2E,0x07,
	0x5B,0x02,0x57,0x57,
	0x02,0x03,0x00,0x00
};

static unsigned char hgc_text_crtc[12] = {
	0x61,0x50,0x52,0x0F,
	0x19,0x06,0x19,0x19,
	0x02,0x0D,0x0D,0x0C
};

uint32_t vga_clock_rates[4] = {
	25175000UL,
	(25175000UL*9UL)/8UL,	/* ~28MHz */
/* if you know your SVGA chipset's correct values you may overwrite entries 2 and 3 */
	0,
	0
};

int check_vga_3C0() {
	unsigned char mor;

	/* Misc. output register test. This register is supposed to be readable at 0x3CC, writeable at 0x3C2,
	 * and contain 7 documented bits that we can freely change */
	_cli();
	mor = inp(0x3CC);
	outp(0x3C2,0);	/* turn it all off */
	if ((inp(0x3CC)&0xEF) != 0) { /* NTS: do not assume bit 4 is changeable */
		/* well if I couldn't change it properly it isn't the Misc. output register now is it? */
		outp(0x3C2,mor); /* restore it */
		return 0;
	}
	outp(0x3C2,0xEF);	/* turn it all on (NTS: bit 4 is undocumented) */
	if ((inp(0x3CC)&0xEF) != 0xEF) { /* NTS: do not assume bit 4 is changeable */
		/* well if I couldn't change it properly it isn't the Misc. output register now is it? */
		outp(0x3C2,mor); /* restore it */
		return 0;
	}
	outp(0x3C2,mor);
	_sti();

	return 1;
}

void vga_sync_hw_cursor() {
	unsigned int i;
	i = vga_read_CRTC(0xF);			/* cursor low */
	i |= vga_read_CRTC(0xE) << 8;		/* cursor high */
	vga_pos_x = i % vga_stride;
	vga_pos_y = i / vga_stride;
}

void vga_sync_bios_cursor() {
#ifndef TARGET_WINDOWS
# if TARGET_MSDOS == 32
	*((unsigned char*)0x450) = vga_pos_x;
	*((unsigned char*)0x451) = vga_pos_y;
# else
	*((unsigned char far*)MK_FP(0x40,0x50)) = vga_pos_x;
	*((unsigned char far*)MK_FP(0x40,0x51)) = vga_pos_y;
# endif
#endif
}

void update_state_vga_memory_map_select(unsigned char c) {
	switch (c) {
		case 0: vga_ram_base = 0xA0000; vga_ram_size = 0x20000; break;
		case 1: vga_ram_base = 0xA0000; vga_ram_size = 0x10000; break;
		case 2: vga_ram_base = 0xB0000; vga_ram_size = 0x08000; break;
		case 3: vga_ram_base = 0xB8000; vga_ram_size = 0x08000; break;
	}

#if defined(TARGET_WINDOWS)
	if (vga_ram_base == 0xA0000)
		vga_graphics_ram = (unsigned char FAR*)DisplayDibGetScreenPointerA000();
	else if (vga_ram_base == 0xB0000)
		vga_graphics_ram = (unsigned char FAR*)DisplayDibGetScreenPointerB000();
	else if (vga_ram_base == 0xB8000)
		vga_graphics_ram = (unsigned char FAR*)DisplayDibGetScreenPointerB800();
	else
		vga_graphics_ram = (unsigned char FAR*)DisplayDibGetScreenPointerA000();

	vga_graphics_ram_fence = vga_graphics_ram + vga_ram_size;
	vga_alpha_ram = (uint16_t FAR*)vga_graphics_ram;
	vga_alpha_ram_fence = (uint16_t FAR*)vga_graphics_ram_fence;
#else
# if TARGET_MSDOS == 32
	/* NTS: According to many sources, 32-bit DOS extenders tend to map the lower 1MB 1:1, so this is safe */
	/* NTS: If I remember correctly Windows 95 also did this for Win32 applications, for whatever reason! */
	vga_graphics_ram = (unsigned char*)vga_ram_base;
	vga_graphics_ram_fence = vga_graphics_ram + vga_ram_size;
	vga_alpha_ram = (uint16_t*)vga_graphics_ram;
	vga_alpha_ram_fence = (uint16_t*)vga_graphics_ram_fence;
# else
	vga_graphics_ram = MK_FP(vga_ram_base>>4,vga_ram_base&0xF);	/* A0000 -> A000:0000 */
	vga_graphics_ram_fence = MK_FP((vga_ram_base+vga_ram_size)>>4,(vga_ram_base+vga_ram_size)&0xF);
	vga_alpha_ram = (uint16_t far*)vga_graphics_ram;
	vga_alpha_ram_fence = (uint16_t far*)vga_graphics_ram_fence;
# endif
#endif
}

/* EGA/VGA only */
void vga_set_memory_map(unsigned char c) {
	unsigned char b;

	if (vga_flags & VGA_IS_VGA) {
		b = vga_read_GC(6);
		vga_write_GC(6,(b & 0xF3) | (c << 2));
		update_state_vga_memory_map_select(c);
	}
	else if (vga_flags & VGA_IS_EGA) {
		/* guessing code because you can't readback regs on EGA */
		b = int10_getmode();
		/* INT 10H text modes: set odd/even,   else, set alpha disable */
		vga_write_GC(6,0x02 | (c << 2));
		update_state_vga_memory_map_select(c);
	}
}

void update_state_from_vga() {
	unsigned char c;

	vga_pos_x = vga_pos_y = 0;
	vga_stride = 80;
	vga_height = 25;
	vga_width = 80;
	vga_9wide = 0;

	if (vga_flags & VGA_IS_VGA) { /* VGA only. EGA cards DO have the misc. output reg but it's write-only */
		/* update state from H/W which I/O port */
		c = inp(0x3CC);
		if (c & 1) {
			vga_base_3x0 = 0x3D0;
		}
		else {
			vga_base_3x0 = 0x3B0;
		}

		/* now ask the graphics controller where/how VGA memory is mapped */
		c = vga_read_GC(6);
		/* bit 0 = alpha disable (if set, graphics) */
		vga_alpha_mode = ((c & 1) == 0);
		/* bits 2-3 memory map select */
		update_state_vga_memory_map_select((c>>2)&3);

		/* read the sequencer: are we in 8 or 9 dot mode? */
		c = vga_read_sequencer(0x1);
		vga_9wide = (c & 1) == 0;

		/* read from the CRTC controller the stride, width, and height */
		vga_stride = vga_read_CRTC(0x13) * 2;	/* "offset" register */
		if (vga_alpha_mode) {
			vga_width = vga_stride;
			vga_sync_hw_cursor();
			/* TODO: read vertical blank values and calculate active area, then divide by scan line height, to get alpha height */
			/* TODO: read horizontal blank values to calculate active area, then visible width */
		}
		else {
			/* TODO: similar semantics for graphics mode */
		}
	}
	else if (vga_flags & VGA_IS_EGA) {
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
			vga_base_3x0 = 0x3D0;
		else if ((c&0xF0) == 0xB0)
			vga_base_3x0 = 0x3B0;
		else {
			vga_base_3x0 = 0x3D0;
		}

		/* reading from the graphics controller (0x3CE) doesn't work, deduce from BIOS mode */
		c = int10_getmode();
		switch (c) {
			case 0: case 1: case 2: case 3: case 7:
				vga_alpha_mode = 1;

 /* the best we can do is assume B0000 if CRTC is at 3Bx or B8000 if at 3Dx even though it's possible to map at B8000 and 3Bx */
				if (vga_base_3x0 == 0x3B0)
					update_state_vga_memory_map_select(2);
				else
					update_state_vga_memory_map_select(3);
				break;
			case 4: case 5: case 6:
				vga_alpha_mode = 0;
				update_state_vga_memory_map_select(3);
				break;
			case 13: case 14: case 15: case 16: case 17: case 18: default:
				vga_alpha_mode = 0;
				update_state_vga_memory_map_select(1);
				break;
		}

		/* read from the CRTC controller the stride, width, and height */
		vga_stride = vga_read_CRTC(0x13) * 2;	/* "offset" register */
		if (vga_alpha_mode) {
			vga_width = vga_stride;
			vga_sync_hw_cursor();
			/* TODO: read vertical blank values and calculate active area, then divide by scan line height, to get alpha height */
			/* TODO: read horizontal blank values to calculate active area, then visible width */
		}
		else {
			/* TODO: similar semantics for graphics mode */
		}

#if defined(TARGET_WINDOWS)
		/* TODO */
#else
# if TARGET_MSDOS == 32
		/* NTS: According to many sources, 32-bit DOS extenders tend to map the lower 1MB 1:1, so this is safe */
		/* NTS: If I remember correctly Windows 95 also did this for Win32 applications, for whatever reason! */
		vga_graphics_ram = (unsigned char*)vga_ram_base;
		vga_graphics_ram_fence = vga_graphics_ram + vga_ram_size;
		vga_alpha_ram = (uint16_t*)vga_graphics_ram;
		vga_alpha_ram_fence = (uint16_t*)vga_graphics_ram_fence;
# else
		vga_graphics_ram = MK_FP(vga_ram_base>>4,vga_ram_base&0xF);	/* A0000 -> A000:0000 */
		vga_graphics_ram_fence = MK_FP((vga_ram_base+vga_ram_size)>>4,vga_ram_base+vga_ram_size);
		vga_alpha_ram = (uint16_t far*)vga_graphics_ram;
		vga_alpha_ram_fence = (uint16_t far*)vga_graphics_ram_fence;
# endif
#endif
	}
	else if (vga_flags & VGA_IS_CGA) {
		vga_base_3x0 = 0x3D0; /* always at 0x3Dx */

		/* TODO: If Tandy, detect state */

		/* read the status register to determine the state of the CGA... oh wait... we can't.
		 * fine. deduce it from the BIOS video mode. */
		c = int10_getmode();
		switch (c) {
			case 0: case 1: case 2: case 3: case 7:
				vga_alpha_mode = 1;
				break;
			default:
				vga_alpha_mode = 0;
				break;
		}

		if (c <= 1) {
			vga_stride = 40;
			vga_width = 40;
		}

		vga_ram_base = 0xB8000;
		vga_ram_size = 0x08000;

#if defined(TARGET_WINDOWS)
		/* TODO */
#else
# if TARGET_MSDOS == 32
		/* NTS: According to many sources, 32-bit DOS extenders tend to map the lower 1MB 1:1, so this is safe */
		/* NTS: If I remember correctly Windows 95 also did this for Win32 applications, for whatever reason! */
		vga_graphics_ram = (unsigned char*)vga_ram_base;
		vga_graphics_ram_fence = vga_graphics_ram + vga_ram_size;
		vga_alpha_ram = (uint16_t*)vga_graphics_ram;
		vga_alpha_ram_fence = (uint16_t*)vga_graphics_ram_fence;
# else
		vga_graphics_ram = MK_FP(vga_ram_base>>4,vga_ram_base&0xF);	/* A0000 -> A000:0000 */
		vga_graphics_ram_fence = MK_FP((vga_ram_base+vga_ram_size)>>4,vga_ram_base+vga_ram_size);
		vga_alpha_ram = (uint16_t far*)vga_graphics_ram;
		vga_alpha_ram_fence = (uint16_t far*)vga_graphics_ram_fence;
# endif
#endif
	}
	else if (vga_flags & VGA_IS_MDA) {
		vga_base_3x0 = 0x3B0; /* always at 0x3Bx */
		vga_alpha_mode = 1; /* stock MDA doesn't have graphics */
		vga_ram_base = 0xB0000;
		vga_ram_size = 0x08000;

		/* Hercules MDA: It would be nice to be able to read bit 2 of the display control,
		 *               except that the port is write-only. Thanks >:( */

#if defined(TARGET_WINDOWS)
		/* TODO */
#else
# if TARGET_MSDOS == 32
		/* NTS: According to many sources, 32-bit DOS extenders tend to map the lower 1MB 1:1, so this is safe */
		/* NTS: If I remember correctly Windows 95 also did this for Win32 applications, for whatever reason! */
		vga_graphics_ram = (unsigned char*)vga_ram_base;
		vga_graphics_ram_fence = vga_graphics_ram + vga_ram_size;
		vga_alpha_ram = (uint16_t*)vga_graphics_ram;
		vga_alpha_ram_fence = (uint16_t*)vga_graphics_ram_fence;
# else
		vga_graphics_ram = MK_FP(vga_ram_base>>4,vga_ram_base&0xF);	/* A0000 -> A000:0000 */
		vga_graphics_ram_fence = MK_FP((vga_ram_base+vga_ram_size)>>4,vga_ram_base+vga_ram_size);
		vga_alpha_ram = (uint16_t far*)vga_graphics_ram;
		vga_alpha_ram_fence = (uint16_t far*)vga_graphics_ram_fence;
# endif
#endif
	}
}

int probe_vga() {
#if defined(TARGET_WINDOWS)
	/* TODO: More comprehensive tests! */
	vga_flags |= VGA_IS_VGA | VGA_IS_EGA | VGA_IS_CGA;
	update_state_from_vga();
	return 1;
#else
	union REGS regs = {0};
	unsigned char c,c2;

	vga_flags = 0;
	vga_base_3x0 = 0;

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
			if (regs.h.bl == 1) {
				vga_flags |= VGA_IS_MDA;
			}
			else if (regs.h.bl == 2) {
				vga_flags |= VGA_IS_CGA;
			}
			else if (regs.h.bl == 4 || regs.h.bl == 5) {
				/* it's an EGA */
				vga_flags |= VGA_IS_EGA | VGA_IS_CGA;
			}
			else if (regs.h.bl == 7 || regs.h.bl == 8) {
				/* VGA, officially */
				vga_flags |= VGA_IS_VGA | VGA_IS_EGA | VGA_IS_CGA;
			}
			else if (regs.h.bl == 10 || regs.h.bl == 11 || regs.h.bl == 12) {
				vga_flags |= VGA_IS_MCGA | VGA_IS_CGA;
			}
		}
	}

	/* if that didn't work, then assume it's not VGA, and probe around a bit */
	/* are you an EGA? Determine by BIOS */
	if (vga_flags == 0) {
		regs.w.ax = 0x1200;
		regs.w.bx = 0xFF10;
#if TARGET_MSDOS == 32
		int386(0x10,&regs,&regs);
#else
		int86(0x10,&regs,&regs);
#endif
		if (regs.h.bh != 0xFF) { /* so, if BH changes the EGA BIOS or higher is present? */
			vga_flags |= VGA_IS_CGA | VGA_IS_EGA;
			/* and BH == 0 if color */
		}
	}

	/* hm, okay. how about testing for a CGA? */
	/* NTS: The CGA is always at port 0x3Dx */
	if (vga_flags == 0) {
		unsigned int patience = 1000;
		outp(0x3D4,0xF);
		c = inp(0x3D5);
		outp(0x3D5,0xAA);
		while (--patience != 0 && (c2=inp(0x3D5)) != 0xAA);
		outp(0x3D5,c);
		if (c2 == 0xAA) {
			vga_flags |= VGA_IS_CGA;

			/* If it looks like a CGA it might be a Tandy/PCjr */
			/* unfortunately I can't find ANYTHING on
			 * how to go about detecting Tandy graphics. The best I can
			 * find are obscure INT 1Ah calls that detect the Tandy's
			 * sound chip (and conflict the PCMCIA services---YIKES!) */
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
						vga_flags |= VGA_IS_TANDY;
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
					vga_flags |= VGA_IS_AMSTRAD;
					/* TODO: If I read the Amstrad tech manual correctly, their video hardware also emulates Hercules modes, right? */
					/* TODO: I get the impression Amstrad graphics do not include Tandy modes, is that correct? */
					//vga_flags &= ~VGA_IS_TANDY; /* <- if so, uncomment this line */
				}
			}
		}
	}

	/* hm, okay. how about testing for a MDA? */
	/* NTS: The MDA is always at port 0x3Bx */
	if (vga_flags == 0) {
		unsigned int patience = 1000;
		outp(0x3B4,0xF);
		c = inp(0x3B5);
		outp(0x3B5,0xAA);
		while (--patience != 0 && (c2=inp(0x3B5)) != 0xAA);
		outp(0x3B5,c);
		if (c2 == 0xAA) {
			vga_flags |= VGA_IS_MDA;
		}
	}

	/* If it looks like an MDA, then it might be helpful to tell whether it's a
	 * Hercules graphics card */
	if (vga_flags & VGA_IS_MDA) {
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
			vga_flags |= VGA_IS_HGC;
			vga_hgc_type = (c >> 4) & 7;
			switch ((c>>4)&7) {
				case 5: case 1: vga_hgc_type = (c>>4)&7; break;
				default: vga_hgc_type = 0; break;
			}
		}
	}

	update_state_from_vga();
	return 1;
#endif
}

/* WARNING: [At least in DOSBox 0.74] do not call this for any CGA or EGA graphics modes.
 *          It seems to trigger a false mode change and alphanumeric mode */
void vga_relocate_crtc(unsigned char color) {
	unsigned char moc = 0;

	/* this code assumes color == 0 or color == 1 */
	color = (color != 0)?1:0;

	/* VGA: Easy, read the register, write it back */
	if (vga_flags & VGA_IS_VGA) {
		moc = inp(0x3CC);
		moc &= 0xFE; /* clear bit 0, respond to 0x3Bx */
		outp(0x3C2,moc | color);
	}
	else if (vga_flags & VGA_IS_EGA) {
		/* EGA: We can't read it, but we can write it from our best guess */
		outp(0x3C2,0x02 | (color?1:0));
	}

	vga_base_3x0 = color ? 0x3D0 : 0x3B0;
}

void vga_switch_to_mono() {
	unsigned char moc = 0;

	/* VGA: Easy, read the register, write it back */
	if (vga_flags & VGA_IS_VGA) {
		moc = inp(0x3CC);
		moc &= 0xFE; /* clear bit 0, respond to 0x3Bx */
		outp(0x3C2,moc);

		/* and then hack the graphics controller to remap the VRAM */
		moc = vga_read_GC(6);
		moc &= 0xF3;	/* clear bits 2-3 */
		moc |= 0x08;	/* bits 2-3 = 10 = B0000 */
		vga_write_GC(6,moc);
	}
	else if (vga_flags & VGA_IS_EGA) {
		/* EGA: We can't read it, but we can write it from our best guess */
		moc = 0x02;	/* VSYNC/HSYNC pos, low page odd/even, 25MHz clock, RAM enable */
		outp(0x3C2,moc);
		vga_write_GC(6,0x08|0x02); /* B0000 with odd/even */
	}
	else {
		/* whuh? */
		return;
	}

	/* next, tell the BIOS of the change */
#ifndef TARGET_WINDOWS
# if TARGET_MSDOS == 32
	*((unsigned char*)0x449) = 0x07; /* mode 7 */
	*((unsigned char*)0x463) = 0xB4;
	*((unsigned char*)0x410) |= 0x30; /*  -> change to 80x25 mono */
	*((unsigned char*)0x465) &= ~0x04; /* monochrome operation */
# else
	*((unsigned char far*)MK_FP(0x40,0x49)) = 0x07; /* mode 7 */
	*((unsigned char far*)MK_FP(0x40,0x63)) = 0xB4;
	*((unsigned char far*)MK_FP(0x40,0x10)) |= 0x30; /*  -> change to 80x25 mono */
	*((unsigned char far*)MK_FP(0x40,0x65)) &= ~0x04; /* monochrome operation */
# endif
#endif
}

void vga_switch_to_color() {
	unsigned char moc = 0;

	/* VGA: Easy, read the register, write it back */
	if (vga_flags & VGA_IS_VGA) {
		moc = inp(0x3CC);
		moc |= 0x01; /* set bit 0, respond to 0x3Dx */
		outp(0x3C2,moc);

		/* and then hack the graphics controller to remap the VRAM */
		moc = vga_read_GC(6);
		moc &= 0xF3;	/* clear bits 2-3 */
		moc |= 0x0C;	/* bits 2-3 = 11 = B8000 */
		vga_write_GC(6,moc);
	}
	else if (vga_flags & VGA_IS_EGA) {
		/* EGA: We can't read it, but we can write it from our best guess */
		moc = 0x02|1;	/* VSYNC/HSYNC pos, low page odd/even, 25MHz clock, RAM enable */
		outp(0x3C2,moc);
		vga_write_GC(6,0x0C|0x02); /* B8000 with odd/even */
	}
	else {
		/* whuh? */
		return;
	}

	/* next, tell the BIOS of the change */
#ifndef TARGET_WINDOWS
# if TARGET_MSDOS == 32
	*((unsigned char*)0x449) = 0x03; /* mode 3 */
	*((unsigned char*)0x463) = 0xD4;
	*((unsigned char*)0x410) &= 0x30; /* INT 11 initial video mode */
	*((unsigned char*)0x410) |= 0x20; /*  -> change to 80x25 color */
	*((unsigned char*)0x465) |= 0x04; /* color operation */
# else
	*((unsigned char far*)MK_FP(0x40,0x49)) = 0x03; /* mode 3 */
	*((unsigned char far*)MK_FP(0x40,0x63)) = 0xD4;
	*((unsigned char far*)MK_FP(0x40,0x10)) &= 0x30; /* INT 11 initial video mode */
	*((unsigned char far*)MK_FP(0x40,0x10)) |= 0x20; /*  -> change to 80x25 color */
	*((unsigned char far*)MK_FP(0x40,0x65)) |= 0x04; /* color operation */
# endif
#endif
}

void vga_moveto(unsigned char x,unsigned char y) {
	vga_pos_x = x;
	vga_pos_y = y;
}

void vga_turn_on_hgc() {
	unsigned char c;

	outp(0x3B8,0x00); /* turn off video */
	outp(0x3BF,0x01); /* enable setting graphics mode */
	outp(0x3B8,0x02); /* turn on graphics */
	for (c=0;c < 12;c++) {
		outp(0x3B4,c);
		outp(0x3B5,hgc_graphics_crtc[c]);
	}
	outp(0x3B8,0x0A); /* turn on graphics+video */
}

void vga_turn_off_hgc() {
	unsigned char c;

	outp(0x3B8,0x00); /* turn off video */
	outp(0x3BF,0x00); /* disable setting graphics mode */
	for (c=0;c < 12;c++) {
		outp(0x3B4,c);
		outp(0x3B5,hgc_text_crtc[c]);
	}
	outp(0x3B8,0x28); /* turn on video and text */
}

void vga_set_cga_palette_and_background(unsigned char pal,unsigned char color) {
	outp(0x3D9,(pal << 5) | color);
}

void vga_set_cga_mode(unsigned char b) {
	outp(0x3D8,b);
}

void vga_tandy_setpalette(unsigned char i,unsigned char c) {
	inp(0x3DA);
	outp(0x3DA,0x10 + i);
	outp(0x3DE,c);	/* NTS: This works properly on Tandy [at least DOSBox] */
	outp(0x3DA,c);	/* NTS: Writing 0x00 like some sames do works on Tandy but PCjr takes THIS byte as palette data */
}

void vga_enable_256color_modex() {
	vga_write_sequencer(4,0x06);
	vga_write_sequencer(0,0x01);
	vga_write_CRTC(0x17,0xE3);
	vga_write_CRTC(0x14,0x00);
	vga_write_sequencer(0,0x03);
	vga_write_sequencer(VGA_SC_MAP_MASK,0xF);
}

void vga_set_stride(unsigned int stride) {
	/* TODO: Knowing the current "byte/word/dword" memory size, compute properly */
	stride >>= (2+1); /* divide by DWORD * 2 */
	vga_write_CRTC(0x13,stride);
}

void vga_set_start_location(unsigned int offset) {
	vga_write_CRTC(0x0C,offset>>8);
	vga_write_CRTC(0x0D,offset);
}

void vga_set_ypan_sub(unsigned char c) {
	vga_write_CRTC(0x08,c);
}

void vga_set_xpan(unsigned char c) {
	vga_write_AC(0x13|0x20,c);
}

void vga_splitscreen(unsigned int v) {
	unsigned char c;

	/* FIXME: Either DOSBox 0.74 got EGA emulation wrong, or we really do have to divide the line count by 2. */
	/*        Until then leave the value as-is */
	/*  TODO: Didn't Mike Abrash or other programming gurus mention a bug or off-by-1 error with the EGA linecount? */

	vga_write_CRTC(0x18,v);
	if (vga_flags & VGA_IS_VGA) {
		c = vga_read_CRTC(0x07);
		vga_write_CRTC(0x07,(c & (~0x10)) | (((v>>8)&1) << 4));
		c = vga_read_CRTC(0x09);
		vga_write_CRTC(0x09,(c & (~0x40)) | (((v>>9)&1) << 6));
	}
	else {
		c = 0x1F; /* since most modes have > 256 lines this is as good a guess as any */
		vga_write_CRTC(0x07,(c & (~0x10)) | (((v>>8)&1) << 4));
	}
}

void vga_alpha_switch_to_font_plane() {
	vga_write_GC(0x4,0x02); /* NTS: Read Map Select: This is very important if the caller wants to read from the font plane without reading back gibberish */
	vga_write_GC(0x5,0x00);
	vga_write_GC(0x6,0x0C);
	vga_write_sequencer(0x2,0x04);
	vga_write_sequencer(0x4,0x06);
}

void vga_alpha_switch_from_font_plane() {
	vga_write_GC(0x4,0x00);
	vga_write_GC(0x5,0x10);
	vga_write_GC(0x6,0x0E);
	vga_write_sequencer(0x2,0x03);
	vga_write_sequencer(0x4,0x02);
}

/* NTS: this also completes the change by setting the clock select bits in Misc. out register
 *      on the assumption that you are changing 8/9 bits in 80x25 alphanumeric mode. The
 *      clock change is necessary to retain the proper hsync/vsync rates on the VGA monitor. */
void vga_set_9wide(unsigned char en) {
	unsigned char c;

	if (en == vga_9wide)
		return;

	c = vga_read_sequencer(1);
	c &= 0xFE;
	c |= en ^ 1;
	vga_write_sequencer(1,c);
	vga_9wide = en;

	c = inp(0x3CC);
	c &= 0xF3;
	c |= (en ? 1 : 0) << 2;	/* 0=25MHz 1=28MHz */
	outp(0x3C2,c);
}

void vga_select_charset_a_b(unsigned short a,unsigned short b) {
	unsigned char c;

	c  =  a >> 14;
	c |= (b >> 14) << 2;
	c |= ((a >> 13) & 1) << 4;
	c |= ((b >> 13) & 1) << 5;

	vga_write_sequencer(3,c);
}

/* VGA hardware registers impose several restraints on valid values,
 * mostly related to how retrace and blanking start/end values are encoded */
void vga_correct_crtc_mode(struct vga_mode_params *p) {
	/* TODO */
}

/* WARNING: This code is deliberately designed to NOT fullfill your request if you have
 *          anything but VGA. EGA and CGA monitors are fixed-frequency and this kind of
 *          experimentation is not good for them. You can of course fry a VGA monitor
 *          that way too, but at least we can read back the "safe" values the BIOS
 *          programmed into the hardware */
void vga_write_crtc_mode(struct vga_mode_params *p,unsigned int flags) {
	unsigned char c,c2;

	if (!(vga_flags & VGA_IS_VGA))
		return;

	/* sync disable unless told not to */
	if (!(flags & VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC)) {
		c = vga_read_CRTC(0x17);
		vga_write_CRTC(0x17,c&0x7F);
	}

	c = inp(0x3CC); /* misc out reg */
	c &= ~((3 << 2) | (1 << 6) | (1 << 7));
	c |= (p->clock_select&3) << 2;
	c |= p->hsync_neg << 6;
	c |= p->vsync_neg << 7;
	outp(0x3C2,c);

	vga_write_sequencer(1,
		(p->shift_load_rate << 2) |
		(p->shift4_enable << 4) |
		((p->clock9 ^ 1) << 0) |
		(p->clock_div2 << 3));

	c = 0; /* use 'c' as overflow register */
	c2 = vga_read_CRTC(0x09); /* read max scan line */
	c2 &= ~(1 << 5); /* mask out start vertical blank bit 9 */
	vga_write_CRTC(0x11, /* NTS: we leave bit 7 (protect) == 0 so we can program regs 0-7 in this routine */
		(((p->refresh_cycles_per_scanline == 5) ? 1 : 0) << 6) |
		(p->vertical_end_retrace & 0xF));
	vga_write_CRTC(0x06,(p->vertical_total - 2));
		c |= (((p->vertical_total - 2) >> 8) & 1) << 0;
		c |= (((p->vertical_total - 2) >> 9) & 1) << 5;
	vga_write_CRTC(0x10,p->vertical_start_retrace);
		c |= ((p->vertical_start_retrace >> 8) & 1) << 2;
		c |= ((p->vertical_start_retrace >> 9) & 1) << 7;
	vga_write_CRTC(0x12,p->vertical_display_end - 1);
		c |= (((p->vertical_display_end - 1) >> 8) & 1) << 1;
		c |= (((p->vertical_display_end - 1) >> 9) & 1) << 6;
	vga_write_CRTC(0x15,p->vertical_blank_start - 1);
		c |= (((p->vertical_blank_start - 1) >> 8) & 1) << 3;
		c2|= (((p->vertical_blank_start - 1) >> 9) & 1) << 5;

	/* NTS: this field is 7 bits wide but "Some SVGA chipsets use all 8" as VGADOC says. */
	/*      writing it in this way resolves the partial/full screen blanking problems with Intel 855/915/945 chipsets */
	vga_write_CRTC(0x16,p->vertical_blank_end - 1);

	vga_write_CRTC(0x14, /* NTS we write "underline location == 0" */
		(p->dword_mode << 6) |
		(p->inc_mem_addr_only_every_4th << 5));
	vga_write_CRTC(0x07,c); /* overflow reg */

	c2 &= ~(0x9F);	/* mask out SD + Max scanline */
	c2 |= (p->scan_double << 7);
	c2 |= (p->max_scanline - 1) & 0x1F;
	vga_write_CRTC(0x09,c2);
	vga_write_CRTC(0x13,p->offset);
	vga_write_CRTC(0,(p->horizontal_total - 5));
	vga_write_CRTC(1,(p->horizontal_display_end - 1));
	vga_write_CRTC(2,p->horizontal_blank_start - 1);
	vga_write_CRTC(3,((p->horizontal_blank_end - 1) & 0x1F) | (p->horizontal_start_delay_after_total << 5) | 0x80);
	vga_write_CRTC(4,p->horizontal_start_retrace);
	vga_write_CRTC(5,((((p->horizontal_blank_end - 1) >> 5) & 1) << 7) | (p->horizontal_start_delay_after_retrace << 5) |
		(p->horizontal_end_retrace & 0x1F));

	/* finish by writing reg 0x17 which also enables sync */
	vga_write_CRTC(0x17,
		(p->sync_enable << 7) |
		(vga_read_CRTC(0x17) & 0x10) | /* NTS: one undocumented bit, perhaps best not to change it */
		((p->word_mode^1) << 6) |
		(p->address_wrap_select << 5) |
		(p->memaddr_div2 << 3) |
		(p->scanline_div2 << 2) |
		((p->map14 ^ 1) << 1) |
		((p->map13 ^ 1) << 0));

	/* reinforce write protect */
	c = vga_read_CRTC(0x11);
	vga_write_CRTC(0x11,c|0x80);
}

void vga_read_crtc_mode(struct vga_mode_params *p) {
	unsigned char c,c2;

	if (!(vga_flags & VGA_IS_VGA))
		return;

	c = inp(0x3CC);	/* misc out reg */
	p->clock_select = (c >> 2) & 3;
	p->hsync_neg = (c >> 6) & 1;
	p->vsync_neg = (c >> 7) & 1;

	c = vga_read_sequencer(1);
	p->clock9 = (c & 1) ^ 1;
	p->clock_div2 = (c >> 3) & 1;
	p->shift4_enable = (c >> 4) & 1;
	p->shift_load_rate = (c >> 2) & 1;

	p->sync_enable = (vga_read_CRTC(0x17) >> 7) & 1;
	p->word_mode = ((vga_read_CRTC(0x17) >> 6) & 1) ^ 1;
	p->address_wrap_select = (vga_read_CRTC(0x17) >> 5) & 1;
	p->memaddr_div2 = (vga_read_CRTC(0x17) >> 3) & 1;
	p->scanline_div2 = (vga_read_CRTC(0x17) >> 2) & 1;
	p->map14 = ((vga_read_CRTC(0x17) >> 1) & 1) ^ 1;
	p->map13 = ((vga_read_CRTC(0x17) >> 0) & 1) ^ 1;

	p->dword_mode = (vga_read_CRTC(0x14) >> 6) & 1;
	p->horizontal_total = vga_read_CRTC(0) + 5;
	p->horizontal_display_end = vga_read_CRTC(1) + 1;
	p->horizontal_blank_start = vga_read_CRTC(2) + 1;
	p->horizontal_blank_end = ((vga_read_CRTC(3) & 0x1F) | ((vga_read_CRTC(5) >> 7) << 5) |
		((p->horizontal_blank_start - 1) & (~0x3F))) + 1;
	if (p->horizontal_blank_start >= p->horizontal_blank_end)
		p->horizontal_blank_end += 0x40;
	p->horizontal_start_retrace = vga_read_CRTC(4);
	p->horizontal_end_retrace = (vga_read_CRTC(5) & 0x1F) |
		(p->horizontal_start_retrace & (~0x1F));
	if ((p->horizontal_start_retrace&0x1F) >= (p->horizontal_end_retrace&0x1F))
		p->horizontal_end_retrace += 0x20;
	p->horizontal_start_delay_after_total = (vga_read_CRTC(3) >> 5) & 3;
	p->horizontal_start_delay_after_retrace = (vga_read_CRTC(5) >> 5) & 3;

	c = vga_read_CRTC(7); /* c = overflow reg */
	c2 = vga_read_CRTC(9);

	p->scan_double = (c2 >> 7) & 1;
	p->max_scanline = (c2 & 0x1F) + 1;
	p->offset = vga_read_CRTC(0x13);
	p->vertical_total = (vga_read_CRTC(6) | ((c & 1) << 8) | (((c >> 5) & 1) << 9)) + 2;
	p->vertical_start_retrace = (vga_read_CRTC(0x10) | (((c >> 2) & 1) << 8) | (((c >> 7) & 1) << 9));
	p->vertical_end_retrace = (vga_read_CRTC(0x11) & 0xF) |
		(p->vertical_start_retrace & (~0xF));
	if ((p->vertical_start_retrace&0xF) >= (p->vertical_end_retrace&0xF))
		p->vertical_end_retrace += 0x10;
	p->refresh_cycles_per_scanline = ((vga_read_CRTC(0x11) >> 6) & 1) ? 5 : 3;
	p->inc_mem_addr_only_every_4th = (vga_read_CRTC(0x14) >> 5) & 1;
	p->vertical_display_end = ((vga_read_CRTC(0x12) | (((c >> 1) & 1) << 8) | (((c >> 6) & 1) << 9))) + 1;
	p->vertical_blank_start = ((vga_read_CRTC(0x15) | (((c >> 3) & 1) << 8) | (((c2 >> 5) & 1) << 9))) + 1;
	p->vertical_blank_end = ((vga_read_CRTC(0x16) & 0x7F) | ((p->vertical_blank_start - 1) & (~0x7F))) + 1;
	if (p->vertical_blank_start >= p->vertical_blank_end)
		p->vertical_blank_end += 0x80;
}

void vga_bios_set_80x50_text() { /* switch to VGA 80x50 8-line high text */
#if defined(TARGET_WINDOWS)
#else
	union REGS regs = {0};
	regs.w.ax = 0x1112;
	regs.w.bx = 0;
# if TARGET_MSDOS == 32
	int386(0x10,&regs,&regs);
# else
	int86(0x10,&regs,&regs);
# endif
	vga_height = 50;
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

