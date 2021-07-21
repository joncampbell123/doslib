
/* Test VBE scenarios:
 *
 * Setup                                     Real B L    Prot B L
 * --------------------------------------------------------------------
 * DOSBox 0.74 (machine=svga_s3)
 *    Detection                              YES  Y Y    YES  Y Y
 *    OEM strings                            YES  Y Y    YES  Y Y
 *    Enumeration of modes                   YES  Y Y    YES  Y Y
 *    16-color modes                         YES  Y Y    YES  Y Y
 *    256-color modes                        YES  Y Y    YES  Y Y
 *    16-color 5:5:5 modes                   YES  Y Y    YES  Y Y
 *    16-color 5:6:5 modes                   YES  Y Y    YES  Y Y
 *    24bpp 8:8:8 modes                      N/A         N/A             DOSBox does not emulate 24bpp VBE modes
 *    32bpp 8:8:8:8 modes                    YES  Y Y    YES  Y Y
 *    8-bit DAC modes                        N/A         N/A             DOSBox does not emulate 8-bit DAC support
 * DOSBox 0.74 (machine=vesa_oldvbe)
 *    Detection                              YES  Y Y    YES  Y Y
 *    OEM strings                            YES  Y Y    YES  Y Y
 *    Enumeration of modes                   YES  Y Y    YES  Y Y
 *    16-color modes                         YES  Y Y    YES  Y Y
 *    256-color modes                        YES  Y Y    YES  Y Y
 *    16-color 5:5:5 modes                   YES  Y Y    YES  Y Y
 *    16-color 5:6:5 modes                   YES  Y Y    YES  Y Y
 *    24bpp 8:8:8 modes                      N/A         N/A             DOSBox does not emulate 24bpp VBE modes
 *    32bpp 8:8:8:8 modes                    YES  Y Y    YES  Y Y
 *    8-bit DAC modes                        N/A         N/A             DOSBox does not emulate 8-bit DAC support
 * DOSBox 0.74 (machine=vesa_nolfb)
 *    Detection                              YES  Y Y    YES  Y Y
 *    OEM strings                            YES  Y Y    YES  Y Y
 *    Enumeration of modes                   YES  Y Y    YES  Y Y
 *    16-color modes                         YES  Y Y    YES  Y Y
 *    256-color modes                        YES  Y Y    YES  Y Y
 *    16-color 5:5:5 modes                   YES  Y Y    YES  Y Y
 *    16-color 5:6:5 modes                   YES  Y Y    YES  Y Y
 *    24bpp 8:8:8 modes                      N/A         N/A             DOSBox does not emulate 24bpp VBE modes
 *    32bpp 8:8:8:8 modes                    YES  Y Y    YES  Y Y
 *    8-bit DAC modes                        N/A         N/A             DOSBox does not emulate 8-bit DAC support
 * DOSBox 0.74 (machine=svga_s3) + Microsoft Windows 3.0 Real Mode
 *    Detection                              YES  Y Y    YES  Y Y
 *    OEM strings                            YES  Y Y    YES  Y Y
 *    Enumeration of modes                   YES  Y Y    YES  Y Y
 *    256-color modes                        YES  Y N    YES  N Y        Real mode: fails to setup mode. Protected mode: bank switching doesn't work?
 * DOSBox 0.74 (machine=svga_s3) + Microsoft Windows 3.0 Standard Mode.
 *    - Protected mode version of this program does not run, DOS extender complains about not having any memory to run it
 *    Detection                              YES  Y Y    YES  N N
 *    OEM strings                            YES  Y Y    YES  N N
 *    Enumeration of modes                   YES  Y Y    YES  N N
 *    256-color modes                        YES  Y N    YES  N N        Real mode: fails to setup mode.
 * DOSBox 0.74 (machine=svga_s3) + Microsoft Windows 3.0 386 Enhanced Mode
 *    Detection                              YES  Y Y    YES  Y Y
 *    OEM strings                            YES  Y Y    YES  Y Y
 *    Enumeration of modes                   YES  Y Y    YES  Y Y
 *    256-color modes                        YES  Y Y    YES  Y N        DOSBox exits with error "TSS not busy". Hm..
 * DOSBox 0.74 (machine=svga_s3) + Microsoft Windows 3.1 Standard Mode.
 *    - Protected mode version of this program does not run, DOS extender complains about not having any memory to run it
 *    Detection                              YES  Y Y    YES  N N
 *    OEM strings                            YES  Y Y    YES  N N
 *    Enumeration of modes                   YES  Y Y    YES  N N
 *    256-color modes                        YES  Y N    YES  N N        Real mode: fails to setup mode.
 * DOSBox 0.74 (machine=svga_s3) + Microsoft Windows 3.1 386 Enhanced Mode
 *    Detection                              YES  Y Y    YES  Y Y
 *    OEM strings                            YES  Y Y    YES  Y Y
 *    Enumeration of modes                   YES  Y Y    YES  Y Y
 *    256-color modes                        YES  Y Y    YES  Y N        DOSBox exits with error "TSS not busy". Hm..
 * QEMU + Microsoft Windows 95 (4.00.950) Native DOS
 *    Detection                              YES  Y Y    YES  Y Y
 *    OEM strings                            YES  Y Y    YES  Y Y
 *    Enumeration of modes                   YES  Y Y    YES  Y Y
 *    256-color modes                        YES  Y Y    YES  Y Y
 *    8-bit DAC                              YES  Y Y    YES  Y Y
 * QEMU + Microsoft Windows 95 (4.00.950) Normal
 *    Detection                              YES  Y Y    YES  Y Y
 *    OEM strings                            YES  Y Y    YES  Y Y
 *    Enumeration of modes                   YES  Y Y    YES  Y Y
 *    256-color modes                        YES  Y Y    YES  Y Y
 *    8-bit DAC                              YES  Y Y    YES  Y Y
 * QEMU + Microsoft Windows ME (4.90.3000) Normal
 *    Detection                              YES  Y Y    YES  Y Y
 *    OEM strings                            YES  Y Y    YES  Y Y
 *    Enumeration of modes                   YES  Y Y    YES  Y Y
 *    256-color modes                        YES  Y Y    YES  Y Y
 *    8-bit DAC                              YES  Y Y    YES  Y Y
 * VirtualBox + Microsoft Windows 2000 Professional
 *    Detection                              YES  Y Y    YES  Y Y        NTVDM.EXE apparently blocks VESA BIOS calls unless the DOS window is fullscreen.
 *    OEM strings                            YES  Y Y    YES  Y Y
 *    Enumeration of modes                   YES  Y?Y?   YES  Y?Y?       I got it to enumerate modes... once. Then it crashed, and next boot NTVDM.EXE
 *                                                                       is apparently blocking the modelist.
 *    256-color modes                        YES  N N    YES  N N        NTVDM.EXE's meddling with the VESA modelist screws up the test. It also seems
 *                                                                       to confuse the VESA bios. Bank switching doesn't work. Linear framebuffer
 *                                                                       modes don't work properly. The test scribbles gibberish on the screen, then
 *                                                                       Windows 2000 hard crashes and reboots. Epic fail.
 *    8-bit DAC                              YES  N N    YES  N N        NTVDM.EXE appears to block 8-bit DAC calls, but otherwise pass through
 *                                                                       flags stating that it's supported
 * VirtualBox + Microsoft Windows XP Professional
 *    Detection                              YES  Y Y    YES  Y Y        NTVDM.EXE automatically switches the DOS box to fullscree in response.
 *    OEM strings                            YES  Y Y    YES  Y Y
 *    Enumeration of modes                   YES  Y?Y?   YES  Y?Y?       I got it to enumerate modes... once. Then it crashed, and next boot NTVDM.EXE
 *                                                                       is apparently blocking the modelist.
 *    256-color modes                        YES  N N    YES  N N        NTVDM.EXE's meddling with the VESA modelist screws up the test. It also seems
 *                                                                       to confuse the VESA bios. Bank switching doesn't work. Linear framebuffer
 *                                                                       modes don't work properly. The test scribbles gibberish on the screen, then
 *                                                                       Windows XP hard crashes and reboots. Epic fail.
 *    8-bit DAC                              YES  N N    YES  N N        NTVDM.EXE appears to block 8-bit DAC calls, but otherwise pass through
 *                                                                       flags stating that it's supported
 * Pentium Pro system + Trident TD8900D ISA SVGA hardware (ISA, 1993)
 *    Detection                              YES  Y Y    YES  Y Y        VBE 1.2 compliant
 *    OEM strings                            YES  Y Y    YES  Y Y
 *    Enumeration of modes                   YES  Y Y    YES  Y Y        For some reason, VESA BIOS reports 16bpp/24bpp modes when the card only goes up to 256-color modes.
 *    256-color modes                        YES  Y Y    YES  Y Y
 *    8-bit DAC                              YES  N N    YES  N N        The card does not support 8-bit DAC output
 *      - It is the older ISA variety that does not offer a linear framebuffer (bank-switching only)
 *      - It also offers some SVGA alphanumeric modes
 *      - It seems to map 800 x 600 x 16-color planar to mode 0x6A, while other VESA modes are >= 0x100. This is reported by the BIOS during mode enumeration.
 *      - If asked to set 16bpp/24bpp modes the BIOS will program parameters that the chipset doesn't support, leaving you with a garbled 256-color display.
 *      - Programming 24bpp mode and then writing to bank-switched RAM causes the computer to hang
 * Toshiba Libretto 50CT laptop + Chips & Tech 65550 (800x600 laptop LCD display)
 *    Detection                              YES  Y Y    YES  Y Y        
 *    OEM strings                            YES  Y Y    YES  Y Y
 *    Enumeration of modes                   YES  Y Y    YES  Y Y        Like most laptop BIOSes, lists modes > 800x600 but does not permit setting them unless external VGA is available.
 *    256-color modes                        YES  Y Y    YES  Y Y
 *    8-bit DAC                              YES  N N    YES  N N        Does not support 8-bit DAC output (understandable, LCDs then were often dithered 5-6 bit RGB)
 *      - Supports banked and linear framebuffers
 *      - Supports 16-color 4-planar SVGA modes
 *      - Supports 256-color, 16bpp and 24bpp modes
 *      - For whatever interesting reason, the chipset also supports a packed 16-color mode (planes=1 bits_per_pixel=4).
 *        Perhaps as a way of allowing 16-color graphics without needing legacy VGA planar I/O. As you'd expect, 4-bit
 *        quantities are packed two to a byte, and index a 16-color CLUT. Unlike the planar modes, reprogramming the
 *        attribute controller to 1:1 mapping is not necessary. The order appears to be high nibble first, then low nibble.
 * Toshiba Sattelite Pro 465CDX laptop + Chips & Tech 65554 (800x600 laptop LCD display)
 *    Detection                              YES  Y Y    YES  Y Y        
 *    OEM strings                            YES  Y Y    YES  Y Y
 *    Enumeration of modes                   YES  Y Y    YES  Y Y        Like most laptop BIOSes, lists modes > 800x600 but does not permit setting them unless external VGA is available.
 *    256-color modes                        YES  Y Y    YES  Y Y
 *    8-bit DAC                              YES  N N    YES  N N        Does not support 8-bit DAC output (understandable, LCDs then were often dithered 5-6 bit RGB)
 *      - Lacks the 4-bit packed mode the Libretto offered :(
 *      - Unrelated ISA PnP test code note: Holy crap I actually found a BIOS that actually supporst the "power off" message. And it works :)

*/

/* TODO: Some of your laptops SVGA cards support a packed 16-color mode (planes=1 bits/pixel=4). Write a routine to target that mode.
 *       Test it on the Libretto laptop to be sure. */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/vesa/vesa.h>
#include <hw/flatreal/flatreal.h>
#include <hw/dos/doswin.h>

#if TARGET_MSDOS == 32
void vbe_realbnk(struct dpmi_realmode_call *rc) {
	if (dpmi_no_0301h > 0) {
		/* Fuck you DOS4/GW! */
		dpmi_alternate_rm_call(rc);
	}
	else {
		__asm {
			mov	ax,0x0301
			xor	bx,bx
			xor	cx,cx
			mov	edi,rc		; we trust Watcom has left ES == DS
			int	0x31		; call DPMI
		}
	}
}

void vbe_realint(struct dpmi_realmode_call *rc) {
	__asm {
		mov	ax,0x0300
		mov	bx,0x0010
		xor	cx,cx
		mov	edi,rc		; we trust Watcom has left ES == DS
		int	0x31		; call DPMI
	}
}

/* the VBE interface is realmode so we need a DOS segment to bounce data through */
static unsigned char*		vbe_dos_buffer = NULL;
static uint16_t			vbe_dos_buffer_selector = 0;
#endif

uint32_t			vesa_lfb_base = 0;
uint32_t			vesa_bnk_rproc = 0;
uint8_t				vesa_bnk_window = 0;		/* which window to use */
uint16_t			vesa_bnk_winseg = 0;
uint16_t			vesa_bnk_winshf = 0;		/* window granularity shift value NOT window size */
uint16_t			vesa_bnk_winszshf = 0;		/* window size shift value NOT window size */
uint16_t			vesa_bnk_wincur = 0;

#if TARGET_MSDOS == 32
void*				vesa_lfb_dpmi_map = NULL;
size_t				vesa_lfb_dpmi_map_size = 0;
uint8_t				vesa_lfb_dpmi_pam = 0;		/* set to 1: used DPMI function 0x0800 to obtain the pointer */
/* TODO: what if we have to use the "alloc/map device linear pages" 1.0 API */
#endif

void (*vesa_writeb)(uint32_t ofs,uint8_t b) = NULL;
void (*vesa_writew)(uint32_t ofs,uint16_t b) = NULL;
void (*vesa_writed)(uint32_t ofs,uint32_t b) = NULL;

struct vbe_info_block*		vbe_info = NULL;

void vbe_free() {
	if (vbe_info) free(vbe_info);
	vbe_info = NULL;

#if TARGET_MSDOS == 32
	if (vbe_dos_buffer != NULL) dpmi_free_dos(vbe_dos_buffer_selector);
	vbe_dos_buffer_selector = 0;
	vbe_dos_buffer = NULL;
#endif
}

int vbe_probe() {
	unsigned int retv = 0;

#if TARGET_MSDOS == 32
	/* WAIT! We might be running under DOS4/GW which does not implement
	   function 0301H: Call real-mode subroutine. If that is true, then
	   we cannot use the window bank switching routine provided by the
	   BIOS */
	if (dpmi_no_0301h < 0) probe_dpmi();
#endif

	vbe_free();

	vbe_info = malloc(sizeof(struct vbe_info_block));
	if (vbe_info == NULL) return 0;
	memset(vbe_info,0,sizeof(*vbe_info));

	/* you can get VBE2 extensions by setting the signature now */
	vbe_info->signature = *((const uint32_t*)("VBE2"));

#if TARGET_MSDOS == 32
	vbe_dos_buffer = dpmi_alloc_dos(VBE_DOS_BUFFER_SIZE,&vbe_dos_buffer_selector);
	if (vbe_dos_buffer == NULL) {
		vbe_free();
		return 0;
	}

	{
		struct dpmi_realmode_call rc={0};
		rc.eax = 0x4F00;
		memcpy(vbe_dos_buffer,vbe_info,sizeof(struct vbe_info_block));
		rc.edi = ((size_t)vbe_dos_buffer) & 0xF;
		rc.es = ((size_t)vbe_dos_buffer) >> 4;
		vbe_realint(&rc);
		retv = rc.eax & 0xFFFF;
		if ((retv >> 8) == 0) {
			uint32_t ofs;

			memcpy(vbe_info,vbe_dos_buffer,sizeof(struct vbe_info_block));
			/* more often than not the VESA BIOS copies it's strings into the "reserved" area
			 * of our struct then writes the memory address of it there. unforunately this
			 * realmode buffer is not the final destination, so we need to translate them.
			 * the string reading code understands that if the segment portion is zero, then
			 * the offset is relative to the vbe info struct. 
			 *
			 * this kind of hackery is why we have accessor functions for the data... :( */
			ofs = (((uint32_t)vbe_info->oem_string_ptr >> 16UL) << 4UL) + ((uint32_t)vbe_info->oem_string_ptr & 0x1FF);
			if (ofs >= (uint32_t)vbe_dos_buffer && ofs < ((uint32_t)vbe_dos_buffer + 512))
				vbe_info->oem_string_ptr = ofs - (uint32_t)vbe_dos_buffer;

			ofs = (((uint32_t)vbe_info->oem_vendor_name_ptr >> 16UL) << 4UL) + ((uint32_t)vbe_info->oem_vendor_name_ptr & 0x1FF);
			if (ofs >= (uint32_t)vbe_dos_buffer && ofs < ((uint32_t)vbe_dos_buffer + 512))
				vbe_info->oem_vendor_name_ptr = ofs - (uint32_t)vbe_dos_buffer;

			ofs = (((uint32_t)vbe_info->oem_product_name_ptr >> 16UL) << 4UL) + ((uint32_t)vbe_info->oem_product_name_ptr & 0x1FF);
			if (ofs >= (uint32_t)vbe_dos_buffer && ofs < ((uint32_t)vbe_dos_buffer + 512))
				vbe_info->oem_product_name_ptr = ofs - (uint32_t)vbe_dos_buffer;

			ofs = (((uint32_t)vbe_info->oem_product_rev_ptr >> 16UL) << 4UL) + ((uint32_t)vbe_info->oem_product_rev_ptr & 0x1FF);
			if (ofs >= (uint32_t)vbe_dos_buffer && ofs < ((uint32_t)vbe_dos_buffer + 512))
				vbe_info->oem_product_rev_ptr = ofs - (uint32_t)vbe_dos_buffer;

			/* the mode list might be stuck in the reserved section too. the VBE standard says that's perfectly fine. so we gotta deal with it */
			ofs = (((uint32_t)vbe_info->video_mode_ptr >> 16UL) << 4UL) + ((uint32_t)vbe_info->video_mode_ptr & 0x1FF);
			if (ofs >= (uint32_t)vbe_dos_buffer && ofs < ((uint32_t)vbe_dos_buffer + 512))
				vbe_info->video_mode_ptr = ofs - (uint32_t)vbe_dos_buffer;
		}
	}
#else
	{
		const unsigned int s = FP_SEG(vbe_info);
		const unsigned int o = FP_OFF(vbe_info);
		__asm {
			push	es
			mov	ax,0x4F00
			mov	di,s
			mov	es,di
			mov	di,o
			int	0x10
			pop	es
			mov	retv,ax
		}

		/* WARNING: Our buffer is filled with the information. But some of the information involves pointers
		 *          to strings stuck in the reserved section. If we move the structure, then those pointers
		 *          become invalid. We normally don't move the structure, so it should be fine unless for
		 *          whatever reason the caller does it for us. */
	}
#endif
	if ((retv >> 8) != 0 || (retv & 0xFF) != 0x4F) { /* AH = 0x00 && AL = 0x4F */
		vbe_free();
		return 0;
	}

	return 1;
}

void vbe_copystring(char *s,size_t max,uint32_t rp,struct vbe_info_block *b) {
	s[0] = 0; if (rp == 0 || max < 2) return; s[max-1] = 0;
#if TARGET_MSDOS == 32
	if ((rp>>16) == 0) /* HACK: Any strings returned to us in the reserved section are translated to 0000:offset format */
		memcpy(s,(char*)vbe_info + (rp&0x1FF),max-1);
	else
		memcpy(s,(char*)((((unsigned long)(rp >> 16UL)) << 4UL) + ((unsigned long)rp & 0xFFFFUL)),max-1);
#else
	_fmemcpy((char far*)s,MK_FP(rp>>16,rp&0xFFFF),max-1);
#endif
}

uint16_t vbe_read_mode_entry(uint32_t mode_ptr,unsigned int entry) {
#if TARGET_MSDOS == 32
	uint16_t *ptr;
	if ((mode_ptr>>16) == 0) /* HACK: The VESA BIOS probably stuck it in the "reserved" section, so fetch it from our copy */
		ptr = (uint16_t*)((char*)vbe_info + (mode_ptr&0x1FF));
	else
		ptr = (uint16_t*)(((mode_ptr >> 16UL) << 4UL) + (mode_ptr & 0xFFFF));
	return ptr[entry];
#else
	uint16_t far *ptr = MK_FP(mode_ptr>>16,mode_ptr&0xFFFF);
	return ptr[entry];
#endif
}

const char *vbe_memory_model_to_str(uint8_t m) {
	switch (m) {
		case 0x00:	return "Text";
		case 0x01:	return "CGA graphics";
		case 0x02:	return "Hercules graphics";
		case 0x03:	return "Planar";
		case 0x04:	return "Packed";
		case 0x05:	return "Unchained 4/256-color";
		case 0x06:	return "Direct color";
		case 0x07:	return "YUV";
	};
	return "?";
}

int vbe_read_mode_info(uint16_t mode,struct vbe_mode_info *mi) {
#if TARGET_MSDOS == 32
	struct dpmi_realmode_call rc={0};
	rc.eax = 0x4F01;
	rc.ecx = mode;
	rc.edi = (uint32_t)vbe_dos_buffer & 0xF;
	rc.es = (uint32_t)vbe_dos_buffer >> 4;
	memset(vbe_dos_buffer,0,256);
	vbe_realint(&rc);
	if (((rc.eax >> 8) & 0xFF) == 0) {
		memcpy((char*)mi,(char*)vbe_dos_buffer,sizeof(*mi));
		return 1;
	}
	return 0;
#else
	int retv = 0;
	const unsigned int s = FP_SEG(mi);
	const unsigned int o = FP_OFF(mi);

	memset(mi,0,sizeof(*mi));
	__asm {
		push	es
		mov	ax,0x4F01
		mov	cx,mode
		mov	di,s
		mov	es,di
		mov	di,o
		int	0x10
		pop	es
		mov	retv,ax
	}

	return (retv >> 8) == 0;
#endif
}

int vbe_fill_in_mode_info(uint16_t mode,struct vbe_mode_info *mi) {
	if (mi->bank_size == 0 && mi->win_granularity != 0)
		mi->bank_size = mi->win_granularity;

	return 1;
}

int vbe_set_mode(uint16_t mode,struct vbe_mode_custom_crtc_info *ci) {
#if TARGET_MSDOS == 32
	struct dpmi_realmode_call rc={0};
	rc.eax = 0x4F02;
	rc.ebx = mode;
	if (ci != NULL) {
		rc.edi = (uint32_t)vbe_dos_buffer & 0xF;
		rc.es = (uint32_t)vbe_dos_buffer >> 4;
		memcpy(vbe_dos_buffer,ci,sizeof(*ci));
	}
	vbe_realint(&rc);
	return (((rc.eax >> 8) & 0xFF) == 0);
#else
	int retv = 0;
	const unsigned int s = FP_SEG(ci);
	const unsigned int o = FP_OFF(ci);

	__asm {
		push	es
		mov	ax,0x4F02
		mov	bx,mode
		mov	di,s
		mov	es,di
		mov	di,o
		int	0x10
		pop	es
		mov	retv,ax
	}

	return (retv >> 8) == 0;
#endif
}

void vbe_bank_switch(uint32_t rproc,uint8_t window,uint16_t bank) {
#if TARGET_MSDOS == 32
	if (rproc == 0) {
		__asm {
			mov	ax,0x4F05
			mov	bl,window
			xor	bh,bh
			mov	dx,bank
			int	0x10
		}
	}
	else {
		struct dpmi_realmode_call rc={0};
		rc.eax = 0x4F05;
		rc.ebx = ((uint32_t)window & 0xFFUL);
		rc.edx = ((uint32_t)bank & 0xFFFFUL);
		rc.cs = (uint16_t)(rproc >> 16UL);
		rc.ip = (uint16_t)(rproc & 0xFFFF);
		vbe_realbnk(&rc);
	}
#else
	if (rproc == 0) {
		__asm {
			mov	ax,0x4F05
			mov	bl,window
			xor	bh,bh
			mov	dx,bank
			int	0x10
		}
	}
	else {
		__asm {
			mov	ax,0x4F05
			mov	bl,window
			xor	bh,bh
			mov	dx,bank
			call	dword ptr [rproc]
		}
	}
#endif
}

int vbe_set_dac_width(int w) {
	signed short int retv = 6;

	if (w < 1) w = 6;
	if (w > 8) w = 8;

	__asm {
		mov	ax,0x4F08
		xor	bl,bl
		mov	bh,byte ptr w
		int	0x10
		cmp	ah,0
		jnz	cnt
		mov	byte ptr retv,bh
cnt:
	}

	return retv;
}

void vesa_set_palette_data(unsigned int first,unsigned int count,char *pal) {
	if (count > 256) count = 256;
	if (count == 0) return;

#if TARGET_MSDOS == 32
	{
		struct dpmi_realmode_call rc={0};
		rc.eax = 0x4F09;
		rc.ebx = 0;
		rc.ecx = count;
		rc.edx = first;
		rc.edi = (uint32_t)vbe_dos_buffer & 0xF;
		rc.es = (uint32_t)vbe_dos_buffer >> 4;
		memcpy(vbe_dos_buffer,pal,count*4);
		vbe_realint(&rc);
	}
#else
	{
		const unsigned int s = FP_SEG(pal);
		const unsigned int o = FP_OFF(pal);
		__asm {
			push	es
			mov	ax,0x4F09
			xor	bx,bx
			mov	cx,count
			mov	dx,first
			mov	di,s
			mov	es,di
			mov	di,o
			int	0x10
			pop	es
		}
	}
#endif
}

void vesa_lfb_writeb(uint32_t ofs,uint8_t b) {
#if TARGET_MSDOS == 32
	*((uint8_t*)((char*)vesa_lfb_dpmi_map+ofs)) = b;
#else
	flatrealmode_writeb(vesa_lfb_base+ofs,b);
#endif
}

void vesa_lfb_writew(uint32_t ofs,uint16_t b) {
#if TARGET_MSDOS == 32
	*((uint16_t*)((char*)vesa_lfb_dpmi_map+ofs)) = b;
#else
	flatrealmode_writew(vesa_lfb_base+ofs,b);
#endif
}

void vesa_lfb_writed(uint32_t ofs,uint32_t b) {
#if TARGET_MSDOS == 32
	*((uint32_t*)((char*)vesa_lfb_dpmi_map+ofs)) = b;
#else
	flatrealmode_writed(vesa_lfb_base+ofs,b);
#endif
}

void vesa_bnk_writeb(uint32_t ofs,uint8_t b) {
	uint16_t bnk = (uint16_t)(ofs >> vesa_bnk_winshf);
	if (bnk != vesa_bnk_wincur) vbe_bank_switch(vesa_bnk_rproc,vesa_bnk_window,vesa_bnk_wincur=bnk);
#if TARGET_MSDOS == 32
	*((uint8_t*)((vesa_bnk_winseg << 4) + (ofs & ((1 << (unsigned long)vesa_bnk_winshf) - 1)))) = b;
#else
	*((uint8_t far*)MK_FP(vesa_bnk_winseg,(uint16_t)(ofs & ((1UL << (unsigned long)vesa_bnk_winshf) - 1UL)))) = b;
#endif
}

void vesa_bnk_writew(uint32_t ofs,uint16_t b) {
	uint16_t bnk = (uint16_t)(ofs >> vesa_bnk_winshf);
	if (bnk != vesa_bnk_wincur) vbe_bank_switch(vesa_bnk_rproc,vesa_bnk_window,vesa_bnk_wincur=bnk);
#if TARGET_MSDOS == 32
	*((uint16_t*)((vesa_bnk_winseg << 4) + (ofs & ((1 << (unsigned long)vesa_bnk_winshf) - 1)))) = b;
#else
	*((uint16_t far*)MK_FP(vesa_bnk_winseg,(uint16_t)(ofs & ((1UL << (unsigned long)vesa_bnk_winshf) - 1UL)))) = b;
#endif
}

void vesa_bnk_writed(uint32_t ofs,uint32_t b) {
	uint16_t bnk = (uint16_t)(ofs >> vesa_bnk_winshf);
	if (bnk != vesa_bnk_wincur) vbe_bank_switch(vesa_bnk_rproc,vesa_bnk_window,vesa_bnk_wincur=bnk);
#if TARGET_MSDOS == 32
	*((uint32_t*)((vesa_bnk_winseg << 4) + (ofs & ((1 << (unsigned long)vesa_bnk_winshf) - 1)))) = b;
#else
	*((uint32_t far*)MK_FP(vesa_bnk_winseg,(uint16_t)(ofs & ((1UL << (unsigned long)vesa_bnk_winshf) - 1UL)))) = b;
#endif
}

void vbe_reset_video_to_text() {
	__asm {
		mov	ax,3
		int	0x10
	}
}

void vbe_mode_decision_init(struct vbe_mode_decision *m) {
	m->force_wf = 0;
	m->no_wf = 0;
	m->mode = -1;
	m->dac8 = -1;
	m->lfb = -1;
}

int vbe_mode_decision_validate(struct vbe_mode_decision *md,struct vbe_mode_info *mi) {
	/* general rule of thumb: if the mode involves is a planar VGA 16-color type, then you generally DONT want to
	 * use the linear framebuffer. Most SVGA cards limit VGA read/write emulation to the legacy 0xA0000-0xAFFFF
	 * area only. Most BIOSes (correctly) do not allow you to use linear framebuffer modes with planar SVGA
	 * but there are some implementations that will. On these incorrect implementations, attempts to write the
	 * linear framebuffer VGA style will generate only gibberish.
	 *   - DOSBox 0.74                   [correct] 16-color SVGA modes require banked SVGA modes
	 *   - Microsoft Virtual PC 2007     [*WRONG*] 16-color SVGA modes allow linear framebuffer,
	 *                                             but doesn't emulate VGA read/write latch system when accessed through linear framebuffer.
	 *                                             also their 1024x768 and 1280x1024 16-color modes are broken */
	if (mi->memory_model == 3 && (mi->bits_per_pixel == 4 || mi->number_of_planes == 4) &&
		!(mi->mode_attributes & VESA_MODE_ATTR_NOT_VGA_COMPATIBLE_WINDOWING)) {
		if (md->lfb < 0) md->lfb = 0;
		else if (md->lfb == 1) return 0;
	}
	if (!(mi->mode_attributes & VESA_MODE_ATTR_LINEAR_FRAMEBUFFER_AVAILABLE) || mi->phys_base_ptr == 0) {
		if (md->lfb < 0) md->lfb = 0;
		else if (md->lfb == 1) return 0;
	}
	if ((mi->mode_attributes & VESA_MODE_ATTR_NOT_VGA_COMPATIBLE_WINDOWING) || mi->win_a_segment == 0 || mi->win_a_segment == 0xFFFF) {
		if (md->lfb < 0) md->lfb = 1;
		else if (md->lfb == 1) return 0;
	}

#if TARGET_MSDOS == 16
	/* the linear framebuffer is often above the 1MB mark, so if we're on a 286 or lower we can't reach it */
	/* even on a 386 or higher there are plenty of scenarios where flat real mode might not be available.
	 * in those scenarios we cannot offer LFB modes */
	if (!flatrealmode_allowed() || cpu_basic_level < 3) {
		if (md->lfb < 0) md->lfb = 0;
		else if (md->lfb == 1) return 0;
	}
#endif

	/* 16-color planar modes: if the bios announces 8-bit DAC support it's quite
	 * possible it still won't enable it even though the call doesn't return an
	 * error code. VirtualBox does this for example. */
	if (mi->memory_model == 3 && (mi->bits_per_pixel == 4 || mi->number_of_planes == 4)) {
		if (md->dac8 < 0) md->dac8 = 0;
		else if (md->dac8 == 1) return 0;
	}

	if (md->dac8 < 0)
		md->dac8 = (vbe_info->capabilities & VBE_CAP_8BIT_DAC) ? 1 : 0;

	return 1;
}

int vbe_mode_decision_setmode(struct vbe_mode_decision *md,struct vbe_mode_info *mi) {
	int ok = 0;
	if (!ok && (md->lfb == -1 || md->lfb == 1)) { ok = vbe_set_mode(md->mode | VBE_MODE_LINEAR,NULL); md->lfb = 1;  }
	if (!ok && (md->lfb == -1 || md->lfb == 0)) { ok = vbe_set_mode(md->mode,NULL); md->lfb = 0; }
	return ok;
}

int vbe_mode_decision_acceptmode(struct vbe_mode_decision *md,struct vbe_mode_info *mi) {
#if TARGET_MSDOS == 32
	if (vesa_lfb_dpmi_map != NULL) {
		if (vesa_lfb_dpmi_pam) dpmi_phys_addr_free(vesa_lfb_dpmi_map);
		vesa_lfb_dpmi_map = NULL;
	}
	vesa_lfb_dpmi_map_size = 0;
	vesa_lfb_dpmi_pam = 0;
#endif

	if (mi == NULL)
		return 1;

#if TARGET_MSDOS == 16
	if (md->lfb) { /* for real-mode to access the linear framebuffer we'll need to init flat real mode */
		if (flatrealmode_allowed()) {
			if (!flatrealmode_ok() && !flatrealmode_setup(FLATREALMODE_4GB))
				return 0;
		}
		else {
			return 0;
		}
	}
#else
	if (md->lfb) {
		/* in protected mode, we need to ask the DPMI server what linear memory address to use
		 * to access the VESA framebuffer */
		/* TODO: Does this work under Windows 95/98/ME?
		 *       Does this also work under Windows 2000/XP? */
		vesa_lfb_dpmi_map_size = (unsigned long)mi->bytes_per_scan_line * (unsigned long)mi->y_resolution;
		if (vesa_lfb_dpmi_map_size < 4096 || vesa_lfb_dpmi_map_size > (64UL * 1024UL * 1024UL)) return 0;
		if (mi->phys_base_ptr == 0 || mi->phys_base_ptr == 0xFFFFFFFFUL) return 0;
		vesa_lfb_dpmi_map = (void*)dpmi_phys_addr_map(mi->phys_base_ptr,vesa_lfb_dpmi_map_size);
		if (vesa_lfb_dpmi_map != NULL)
			vesa_lfb_dpmi_pam = 1;
		else
			return 0;
	}
#endif

	if (md->lfb) {
		vesa_lfb_base = mi->phys_base_ptr;
		vesa_writeb = vesa_lfb_writeb;
		vesa_writew = vesa_lfb_writew;
		vesa_writed = vesa_lfb_writed;
	}
	else {
		vesa_bnk_window = 0;	/* TODO */
		vesa_bnk_winseg = mi->win_a_segment;
		switch (mi->win_granularity) {
			case 1:		vesa_bnk_winshf = 10; break;
			case 2:		vesa_bnk_winshf = 11; break;
			case 4:		vesa_bnk_winshf = 12; break;
			case 8:		vesa_bnk_winshf = 13; break;
			case 16:	vesa_bnk_winshf = 14; break;
			case 32:	vesa_bnk_winshf = 15; break;
			case 64:	vesa_bnk_winshf = 16; break;
			case 128:	vesa_bnk_winshf = 17; break;
			default:	vesa_bnk_winshf = 14; break;
		}
		switch (mi->win_size) {
			case 1:		vesa_bnk_winszshf = 10; break;
			case 2:		vesa_bnk_winszshf = 11; break;
			case 4:		vesa_bnk_winszshf = 12; break;
			case 8:		vesa_bnk_winszshf = 13; break;
			case 16:	vesa_bnk_winszshf = 14; break;
			case 32:	vesa_bnk_winszshf = 15; break;
			case 64:	vesa_bnk_winszshf = 16; break;
			case 128:	vesa_bnk_winszshf = 17; break;
			default:	vesa_bnk_winszshf = 14; break;
		}

		/* sanity check: window SIZE cannot be smaller than window GRANULARITY */
		if (vesa_bnk_winszshf < vesa_bnk_winshf)
			vesa_bnk_winszshf = vesa_bnk_winshf;

		vesa_bnk_wincur = 0xFF;
		vesa_bnk_rproc = md->no_wf ? 0UL : mi->window_function;
		vesa_writeb = vesa_bnk_writeb;
		vesa_writew = vesa_bnk_writew;
		vesa_writed = vesa_bnk_writed;
	}

	return 1;
}

