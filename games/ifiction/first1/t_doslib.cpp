
#if defined(USE_DOSLIB)
# include <hw/cpu/cpu.h>
# include <hw/dos/dos.h>
# include <hw/vga/vga.h>
# include <hw/vesa/vesa.h>
# include <hw/8254/8254.h>
# include <hw/8259/8259.h>
# include <hw/8042/8042.h>
# include <hw/dos/doswin.h>
# include <hw/dosboxid/iglib.h> /* for debugging */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include "ifict.h"
#include "utils.h"
#include "debug.h"
#include "fatal.h"
#include "palette.h"

unsigned char*			vesa_lfb = NULL; /* video memory, linear framebuffer */
unsigned char*			vesa_lfb_offscreen = NULL; /* system memory framebuffer, to copy to video memory */
uint32_t			vesa_lfb_physaddr = 0;
uint32_t			vesa_lfb_map_size = 0;
uint32_t			vesa_lfb_stride = 0;
uint16_t			vesa_lfb_height = 0;
uint16_t			vesa_lfb_width = 0;
bool				vesa_setmode = false;
bool				vesa_8bitpal = false; /* 8-bit DAC */
uint16_t			vesa_mode = 0;

unsigned char			vesa_pal[256*4];

uint32_t			pit_count = 0;
uint16_t			pit_prev = 0;

ifevidinfo_t			ifevidinfo_doslib;

/* keyboard controller and IRQ 1, IBM PC/AT 8042 */
bool				keybirq_init = false;
void				(__interrupt *keybirq_old)() = NULL;
unsigned char			keybirq_buf[64];
unsigned char			keybirq_head=0,keybirq_tail=0;

unsigned char			p_keybin[3];
unsigned char			p_keybinpos=0;
unsigned char			p_keybinlen=0;

/* read keyboard buffer */
int keybirq_read_buf(void) {
	int r = -1;

	/* protect from interruption by PUSH+CLI+....+POPF */
	SAVE_CPUFLAGS( _cli() ) {
		if (keybirq_head != keybirq_tail) {
			r = (int)((unsigned char)keybirq_buf[keybirq_head]);
			if (++keybirq_head >= sizeof(keybirq_buf))
				keybirq_head = 0;
		}
	} RESTORE_CPUFLAGS();

	return r;
}

/* IRQ 1 keyboard interrupt */
void __interrupt keybirq() {
	while (inp(K8042_STATUS) & 0x01) {
		unsigned char np = keybirq_tail + 1;

		if (np >= sizeof(keybirq_buf))
			np = 0;

		if (np == keybirq_head)
			break;

		keybirq_buf[np] = inp(K8042_DATA);
		keybirq_tail = np;
	}

	p8259_OCW2(1,P8259_OCW2_SPECIFIC_EOI | 1); /* specific EOI ack IRQ 1 */
}

void keybirq_hook(void) {
	if (!keybirq_init) {
		p8259_mask(1); /* mask the IRQ so we can safely work on it */
		inp(K8042_DATA); /* flush 8042 data */
		inp(K8042_STATUS);
		p8259_OCW2(1,P8259_OCW2_SPECIFIC_EOI | 1); /* make sure the PIC is ready to work */
		keybirq_head = keybirq_tail = 0;
		p_keybinpos=0;
		p_keybinlen=0;

		/* okay, hook the IRQ */
		keybirq_old = _dos_getvect(irq2int(1));
		_dos_setvect(irq2int(1),keybirq);

		/* ready to work */
		p8259_unmask(1); /* unmask the IRQ */

		/* done */
		keybirq_init = true;
	}
}

void keybirq_unhook(void) {
	if (keybirq_init) {
		p8259_mask(1); /* mask the IRQ so we can safely work on it */
		inp(K8042_DATA); /* flush 8042 data */
		inp(K8042_STATUS);
		p8259_OCW2(1,P8259_OCW2_SPECIFIC_EOI | 1); /* make sure the PIC is ready to work */

		/* okay, hook the IRQ */
		_dos_setvect(irq2int(1),keybirq_old);
		keybirq_old = NULL;

		/* ready to work */
		p8259_unmask(1); /* unmask the IRQ */

		/* done */
		keybirq_init = false;
	}
}

static void p_SetPaletteColors(const unsigned int first,const unsigned int count,IFEPaletteEntry *pal) {
	unsigned int i;

	/* IBM PC/AT */
	if (vesa_8bitpal) {
		for (i=0;i < count;i++) {
			/* VGA 8-bit RGB */
			vesa_pal[i*4u + 0u] = (unsigned char)(pal[i].r);
			vesa_pal[i*4u + 1u] = (unsigned char)(pal[i].g);
			vesa_pal[i*4u + 2u] = (unsigned char)(pal[i].b);
		}
	}
	else {
		for (i=0;i < count;i++) {
			/* VGA 6-bit RGB */
			vesa_pal[i*4u + 0u] = (unsigned char)(pal[i].r >> 2u);
			vesa_pal[i*4u + 1u] = (unsigned char)(pal[i].g >> 2u);
			vesa_pal[i*4u + 2u] = (unsigned char)(pal[i].b >> 2u);
		}
	}

	vesa_set_palette_data(first,count,(char*)vesa_pal);
}

static uint32_t p_GetTicks(void) {
	uint32_t w,p;
	{
		uint16_t pit_cur = read_8254(T8254_TIMER_INTERRUPT_TICK);
		pit_count += (uint32_t)((uint16_t)(pit_prev - pit_cur)); /* 8254 counts DOWN, not UP, typecast to ensure 16-bit rollover */
		pit_prev = pit_cur;

		/* convert ticks to milliseconds */
		w = pit_count / (uint32_t)T8254_REF_CLOCK_HZ;
		p = ((pit_count % (uint32_t)T8254_REF_CLOCK_HZ) * (uint32_t)1000ul) / (uint32_t)T8254_REF_CLOCK_HZ;
	}

	return (w * (uint32_t)1000ul) + p;
}

static void p_ResetTicks(const uint32_t base) {
	/* convert milliseconds back to timer ticks and adjust */
	pit_count -= (base / (uint32_t)1000ul) * (uint32_t)T8254_REF_CLOCK_HZ;
	pit_count -= ((base % (uint32_t)1000ul) * (uint32_t)T8254_REF_CLOCK_HZ) / (uint32_t)1000ul;
}

static void p_UpdateFullScreen(void) {
	memcpy(vesa_lfb,vesa_lfb_offscreen,vesa_lfb_map_size);
}

static ifevidinfo_t* p_GetVidInfo(void) {
	return &ifevidinfo_doslib;
}

static bool p_UserWantsToQuit(void) {
	return false;
}

static void p_ProcessScanCode(void) {
	if (p_keybinlen == 3) {
		IFEDBG("Key 0x%02x 0x%02x 0x%02x",p_keybin[0],p_keybin[1],p_keybin[2]);
	}
	else if (p_keybinlen == 2) {
		IFEDBG("Key 0x%02x 0x%02x",p_keybin[0],p_keybin[1]);
	}
	else if (p_keybinlen == 1) {
		IFEDBG("Key 0x%02x",p_keybin[0]);
	}
}

static void p_CheckKeyboard(void) {
	int c;

	if (p_keybinpos == 0) {
		c = keybirq_read_buf();
		if (c < 0) return;

		p_keybin[p_keybinpos++] = (unsigned char)c;

		if (c == 0xE1) /* 0xE1 <xx> <xx> */
			p_keybinlen=3;
		else if (c == 0xE0) /* 0xE0 <xx> */
			p_keybinlen=2;
		else {
			p_keybinlen=1;
			p_ProcessScanCode();
			p_keybinpos=0;
		}
	}
	else if (p_keybinpos < p_keybinlen) {
		c = keybirq_read_buf();
		if (c < 0) return;

		p_keybin[p_keybinpos++] = (unsigned char)c;
		if (p_keybinpos >= p_keybinlen) {
			p_ProcessScanCode();
			p_keybinpos=0;
		}
	}
}

static void p_CheckEvents(void) {
	/* check keyboard input */
	p_CheckKeyboard();
}

static void p_WaitEvent(const int wait_ms) {
	(void)wait_ms;
	p_CheckEvents();
}

static bool p_BeginScreenDraw(void) {
	return true; // nothing to do
}

static void p_EndScreenDraw(void) {
	// nothing to do
}

static void p_ShutdownVideo(void) {
	/* IBM PC/AT */
	keybirq_unhook();
	_sti();
	if (vesa_lfb_offscreen != NULL) {
		ifevidinfo_doslib.buf_base = ifevidinfo_doslib.buf_first_row = NULL;
		free((void*)vesa_lfb_offscreen);
		vesa_lfb_offscreen = NULL;
	}
	if (vesa_lfb != NULL) {
		ifevidinfo_doslib.vram_base = NULL;
		dpmi_phys_addr_free((void*)vesa_lfb);
		vesa_lfb = NULL;
	}
	if (vesa_setmode) {
		vesa_setmode = false;
		int10_setmode(3); /* back to 80x25 text */
	}
}

static void p_InitVideo(void) {
	/* IBM PC/AT compatible */
	/* Find 640x480 256-color mode.
	 * Linear framebuffer required (we'll support older bank switched stuff later) */
	{
		const uint32_t wantf1 = VESA_MODE_ATTR_HW_SUPPORTED | VESA_MODE_ATTR_GRAPHICS_MODE | VESA_MODE_ATTR_LINEAR_FRAMEBUFFER_AVAILABLE;
		struct vbe_mode_info mi = {0};
		uint16_t found_mode = 0;
		unsigned int entry;
		uint16_t mode;

		memset(&ifevidinfo_doslib,0,sizeof(ifevidinfo_doslib));

		for (entry=0;entry < 4096;entry++) {
			mode = vbe_read_mode_entry(vbe_info->video_mode_ptr,entry);
			if (mode == 0xFFFF) break;

			if (vbe_read_mode_info(mode,&mi)) {
				vbe_fill_in_mode_info(mode,&mi);
				if ((mi.mode_attributes & wantf1) == wantf1 && mi.x_resolution == 640 && mi.y_resolution == 480 &&
					mi.bits_per_pixel == 8 && mi.memory_model == 0x04/*packed pixel*/ &&
					mi.phys_base_ptr != 0x00000000ul && mi.phys_base_ptr != 0xFFFFFFFFul &&
					mi.bytes_per_scan_line >= 640) {
					vesa_lfb_physaddr = mi.phys_base_ptr;
					vesa_lfb_map_size = mi.bytes_per_scan_line * mi.y_resolution;
					vesa_lfb_stride = mi.bytes_per_scan_line;
					vesa_lfb_width = mi.x_resolution;
					vesa_lfb_height = mi.y_resolution;
					found_mode = mode;
					break;
				}
			}
		}

		if (found_mode == 0)
			IFEFatalError("VESA BIOS video mode (640 x 480 256-color) not found");

		vesa_mode = found_mode;
	}

	if (!vesa_setmode) {
		/* Set the video mode */
		if (!vbe_set_mode((uint16_t)(vesa_mode | VBE_MODE_LINEAR),NULL))
			IFEFatalError("Unable to set VESA video mode");

		/* we set the mode, set the flag so FatalError can unset it properly */
		vesa_setmode = true;

		ifevidinfo_doslib.width = vesa_lfb_width;
		ifevidinfo_doslib.height = vesa_lfb_height;
		ifevidinfo_doslib.buf_pitch = ifevidinfo_doslib.vram_pitch = vesa_lfb_stride;
		ifevidinfo_doslib.buf_alloc = ifevidinfo_doslib.buf_size = ifevidinfo_doslib.vram_size = vesa_lfb_map_size;

		/* use 8-bit DAC if available */
		if (vbe_info->capabilities & VBE_CAP_8BIT_DAC) {
			if (vbe_set_dac_width(8) == 8)
				vesa_8bitpal = true;
		}

		/* As a 32-bit DOS program atop DPMI we cannot assume a 1:1 mapping between linear and physical,
		 * though plenty of DOS extenders will do just that if EMM386.EXE is not loaded */
		vesa_lfb = (unsigned char*)dpmi_phys_addr_map(vesa_lfb_physaddr,vesa_lfb_map_size);
		if (vesa_lfb == NULL)
			IFEFatalError("DPMI VESA LFB map fail");

		/* video memory is slow, and unlike Windows and SDL2, vesa_lfb points directly at video memory.
		 * Provide an offscreen copy of video memory to work from for compositing. */
		vesa_lfb_offscreen = (unsigned char*)malloc(vesa_lfb_map_size);
		if (vesa_lfb_offscreen == NULL)
			IFEFatalError("DPMI VESA LFB shadow malloc fail");

		ifevidinfo_doslib.vram_base = vesa_lfb;
		ifevidinfo_doslib.buf_base = ifevidinfo_doslib.buf_first_row = vesa_lfb_offscreen;
	}

	/* hook keyboard */
	keybirq_hook();
}

bool priv_IFEMainInit(int argc,char **argv) {
	//not used yet
	(void)argc;
	(void)argv;

	cpu_probe();
	probe_dos();
	detect_windows();
	probe_dpmi();
	probe_vcpi();
	dos_ltp_probe();

	if (!probe_8254())
		IFEFatalError("8254 timer not detected");
	if (!probe_8259())
		IFEFatalError("8259 interrupt controller not detected");
	if (!k8042_probe())
		IFEFatalError("8042 keyboard controller not found");
	if (!probe_vga())
		IFEFatalError("Unable to detect video card");
	if (!(vga_state.vga_flags & VGA_IS_VGA))
		IFEFatalError("Standard VGA not detected");
	if (!vbe_probe() || vbe_info == NULL || vbe_info->video_mode_ptr == 0)
			IFEFatalError("VESA BIOS extensions not detected");

	if (probe_dosbox_id()) {
		printf("DOSBox Integration Device detected\n");
		dosbox_ig = ifedbg_en = true;

		IFEDBG("Using DOSBox Integration Device for debug info. This should appear in your DOSBox/DOSBox-X log file");
	}

	/* make sure the timer is ticking at 18.2Hz. */
	write_8254_system_timer(0);

	/* establish the base line timer tick */
	pit_prev = read_8254(T8254_TIMER_INTERRUPT_TICK);

	/* Windows 95 bug: After reading the 8254, the TF flag (Trap Flag) is stuck on, and this
	 * program runs extremely slowly. Clear the TF flag. It may have something to do with
	 * read_8254 or any other code that does PUSHF + CLI + POPF to protect against interrupts.
	 * POPF is known to not cause a fault on 386-level IOPL3 protected mode, and therefore
	 * a VM monitor has difficulty knowing whether interrupts are enabled, so perhaps setting
	 * TF when the VM executes the CLI instruction is Microsoft's hack to try to work with
	 * DOS games regardless. */
	IFE_win95_tf_hang_check();

	return true;
}

void p_FlushKeyboardInput(void) {
	IFEKeyQueueEmptyAll();
}

IFEKeyEvent *p_GetRawKeyboardInput(void) {
	return IFEKeyQueue.get();
}

IFECookedKeyEvent *p_GetCookedKeyboardInput(void) {
	return IFECookedKeyQueue.get();
}

ifeapi_t ifeapi_doslib = {
	"DOSLIB (IBM PC/AT)",
	p_SetPaletteColors,
	p_GetTicks,
	p_ResetTicks,
	p_UpdateFullScreen,
	p_GetVidInfo,
	p_UserWantsToQuit,
	p_CheckEvents,
	p_WaitEvent,
	p_BeginScreenDraw,
	p_EndScreenDraw,
	p_ShutdownVideo,
	p_InitVideo,
	p_FlushKeyboardInput,
	p_GetRawKeyboardInput,
	p_GetCookedKeyboardInput
};
#endif

