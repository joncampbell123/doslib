
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>

void vga_setup_wm1_block_copy() {
	vga_write_GC(VGA_GC_MODE,0x40/*256-color mode*/ | 1/*write mode 1*/); // and read mode 0
	vga_write_sequencer(0x02/*map mask*/,0xF); // all planes enabled
}

void vga_wm1_mem_block_copy(uint16_t dst,uint16_t src,uint16_t b) {
	VGA_RAM_PTR vdp,vsp;

	if (b == 0) return;
	vdp = vga_state.vga_graphics_ram + dst;
	vsp = vga_state.vga_graphics_ram + src;

#if TARGET_MSDOS == 32
	__asm {
		push	esi
		push	edi
		push	ecx
		push	eax
		cld
		movzx	ecx,b
		mov	esi,vsp
		mov	edi,vdp
		rep	movsb
		pop	eax
		pop	ecx
		pop	edi
		pop	esi
	}
#else
	__asm {
		push	si
		push	di
		push	cx
		push	ax
		push	ds
		push	es
		cld
		mov	cx,b
		lds	si,vsp
		les	di,vdp
		rep	movsb
		pop	es
		pop	ds
		pop	ax
		pop	cx
		pop	di
		pop	si
	}
#endif
}

void vga_restore_rm0wm0() {
	vga_write_GC(VGA_GC_MODE,0x40/*256-color mode*/); // read mode 0, write mode 0
}

