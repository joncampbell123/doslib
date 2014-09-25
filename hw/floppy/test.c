
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/vga/vga.h>
#include <hw/dos/dos.h>
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/vga/vgagui.h>
#include <hw/vga/vgatty.h>

#define MAX_FLOPPY_CONTROLLER 4

struct floppy_controller {
	uint16_t		base_io;
	int8_t			irq;
	int8_t			dma;

	/* known state */
	uint8_t			ps2_status_regs[2];	/* 0x3F0-0x3F1 */
	uint8_t			digital_out;		/* last value written to 0x3F2 */
	uint8_t			main_status;		/* last value read from 0x3F4 */
	uint16_t		irq_fired;

	/* desired state */
	uint8_t			use_dma:1;		/* if set, use DMA. else, use PIO data transfer */

	/* what we know about the controller */
	uint8_t			version;		/* result of command 0x10 (determine controller version) or 0x00 if not yet queried */
	uint8_t			ps2_mode:1;		/* controller is in PS/2 mode (has status registers A and B I/O ports) */
	uint8_t			digital_out_rw:1;	/* digital out (0x3F2) is readable */

};

/* standard I/O ports for floppy controllers */
struct floppy_controller		floppy_standard_isa[2] = {
	/*base, IRQ, DMA*/
	{0x3F0, 6,   2},
	{0x370, 6,   2}
};

struct floppy_controller *floppy_get_standard_isa_port(int x) {
	if (x < 0 || x >= (sizeof(floppy_standard_isa)/sizeof(floppy_standard_isa[0]))) return NULL;
	return &floppy_standard_isa[x];
}

struct floppy_controller		floppy_controllers[MAX_FLOPPY_CONTROLLER];
int8_t					floppy_controllers_init = -1;

struct floppy_controller *floppy_get_controller(int x) {
	if (x < 0 || x >= MAX_FLOPPY_CONTROLLER) return NULL;
	if (floppy_controllers[x].base_io == 0) return NULL;
	return &floppy_controllers[x];
}

int wait_for_enter_or_escape() {
	int c;

	do {
		c = getch();
		if (c == 0) c = getch() << 8;
	} while (!(c == 13 || c == 27));

	return c;
}

struct floppy_controller *alloc_floppy_controller() {
	unsigned int i=0;

	while (i < MAX_FLOPPY_CONTROLLER) {
		if (floppy_controllers[i].base_io == 0)
			return &floppy_controllers[i];
	}

	return NULL;
}

static inline uint8_t floppy_controller_read_status(struct floppy_controller *i) {
	i->main_status = inp(i->base_io+4); /* 0x3F4 main status */
	return i->main_status;
}

static inline void floppy_controller_write_DOR(struct floppy_controller *i,unsigned char c) {
	i->digital_out = c;
	outp(i->base_io+2,c);	/* 0x3F2 Digital Output Register */
}

void floppy_controller_drive_select(struct floppy_controller *i,unsigned char drv) {
	if (drv > 3) return;

	i->digital_out &= ~0x03;
	i->digital_out |= drv;
	outp(i->base_io+2,i->digital_out);	/* 0x3F2 Digital Output Register */
}

void floppy_controller_set_motor_state(struct floppy_controller *i,unsigned char drv,unsigned char set) {
	if (drv > 3) return;

	i->digital_out &= ~(0x10 << drv);
	i->digital_out |= (set?(0x10 << drv):0x00);
	outp(i->base_io+2,i->digital_out);	/* 0x3F2 Digital Output Register */
}

void floppy_controller_enable_dma(struct floppy_controller *i,unsigned char set) {
	if (i->dma < 0 || i->irq < 0) set = 0;

	i->use_dma = !!set;
	i->digital_out &= ~0x08;
	i->digital_out |= (set?0x08:0x00);
	outp(i->base_io+2,i->digital_out);	/* 0x3F2 Digital Output Register */
}

void floppy_controller_enable_dma_otr(struct floppy_controller *i,unsigned char set) {
	unsigned char c;

	c = i->digital_out;
	c &= ~0x08;
	c |= (set?0x08:0x00);
	outp(i->base_io+2,c);			/* 0x3F2 Digital Output Register */
}

void floppy_controller_set_reset(struct floppy_controller *i,unsigned char set) {
	i->digital_out &= ~0x04;
	i->digital_out |= (set?0x00:0x04);	/* bit is INVERTED (0=reset 1=normal) */
	outp(i->base_io+2,i->digital_out);	/* 0x3F2 Digital Output Register */
}

struct floppy_controller *floppy_controller_probe(struct floppy_controller *i) {
	struct floppy_controller *ret = NULL;
	uint8_t t1;

	if (i == NULL) return NULL;
	if (i->base_io == 0) return NULL;

	/* is anything there? the best we can hope for is to probe the I/O port range and see if SOMETHING responds */
	t1 = inp(i->base_io+4); /* main status register (we can't assume the ability to read the digital output register, the data reg might be 0xFF) */
	if (t1 == 0xFF) return NULL;

	ret = alloc_floppy_controller();
	if (ret == NULL) return NULL;
	memset(ret,0,sizeof(*ret));

	ret->base_io = i->base_io;
	if (i->irq >= 2 && i->irq <= 15)
		ret->irq = i->irq;
	else
		ret->irq = -1;

	if (i->dma >= 0 && i->dma <= 7)
		ret->dma = i->dma;
	else
		ret->dma = -1;

	ret->use_dma = (ret->dma >= 0 && ret->irq >= 0); /* FIXME: I'm sure there's a way to do DMA without IRQ */

	/* if something appears at 0x3F0-0x3F1, assume "PS/2 mode".
	 * there are finer intricate details where the meaning of some bits
	 * completely change or invert their meaning between PS/2 and Model 30,
	 * so we don't concern ourself with them, we only care that there's
	 * something there and we can let the program using this lib figure it out. */
	t1 = inp(ret->base_io+0);
	t1 &= inp(ret->base_io+1);
	if (t1 != 0xFF) ret->ps2_mode = 1;

	/* and ... guess */
	floppy_controller_write_DOR(ret,0x04+(ret->use_dma?0x08:0x00));	/* most BIOSes: DMA/IRQ enable, !reset, motor off, drive A select */
	floppy_controller_read_status(ret);

	/* is the Digital Out port readable? */
	t1 = inp(ret->base_io+2);
	if (t1 == ret->digital_out) ret->digital_out_rw = 1;

	return ret;
}

int init_floppy_controller_lib() {
	if (floppy_controllers_init < 0) {
		memset(floppy_controllers,0,sizeof(floppy_controllers));
		floppy_controllers_init = 0;

		cpu_probe();
		probe_dos();
		detect_windows();

		/* do NOT under any circumstances talk directly to the floppy from under Windows! */
		if (windows_mode != WINDOWS_NONE) return (floppy_controllers_init=0);

		/* init OK */
		floppy_controllers_init = 1;
	}

	return floppy_controllers_init;
}

void free_floppy_controller_lib() {
}

/*------------------------------------------------------------------------*/

char	tmp[1024];

static void (interrupt *my_floppy_old_irq)() = NULL;
static struct floppy_controller *my_floppy_irq_floppy = NULL;
static unsigned long floppy_irq_counter = 0;
static int my_floppy_irq_number = -1;

void do_floppy_controller_unhook_irq(struct floppy_controller *fdc);

static void interrupt my_floppy_irq() {
	int i;

	i = vga_width*(vga_height-1);

	_cli();
	if (my_floppy_irq_floppy != NULL) {
		my_floppy_irq_floppy->irq_fired++;

		/* ack PIC */
		if (my_floppy_irq_floppy->irq >= 8) p8259_OCW2(8,P8259_OCW2_NON_SPECIFIC_EOI);
		p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);

		/* If too many IRQs fired, then unhook the IRQ and use polling from now on. */
		if (my_floppy_irq_floppy->irq_fired >= 0xFFFEU) {
			do_floppy_controller_unhook_irq(my_floppy_irq_floppy);
			vga_alpha_ram[i+12] = 0x1C00 | '!';
			my_floppy_irq_floppy->irq_fired = ~0; /* make sure the IRQ counter is as large as possible */
		}
	}

	/* we CANNOT use sprintf() here. sprintf() doesn't work to well from within an interrupt handler,
	 * and can cause crashes in 16-bit realmode builds. */
	vga_alpha_ram[i++] = 0x1F00 | 'I';
	vga_alpha_ram[i++] = 0x1F00 | 'R';
	vga_alpha_ram[i++] = 0x1F00 | 'Q';
	vga_alpha_ram[i++] = 0x1F00 | ':';
	vga_alpha_ram[i++] = 0x1F00 | ('0' + ((floppy_irq_counter / 100000UL) % 10UL));
	vga_alpha_ram[i++] = 0x1F00 | ('0' + ((floppy_irq_counter /  10000UL) % 10UL));
	vga_alpha_ram[i++] = 0x1F00 | ('0' + ((floppy_irq_counter /   1000UL) % 10UL));
	vga_alpha_ram[i++] = 0x1F00 | ('0' + ((floppy_irq_counter /    100UL) % 10UL));
	vga_alpha_ram[i++] = 0x1F00 | ('0' + ((floppy_irq_counter /     10UL) % 10UL));
	vga_alpha_ram[i++] = 0x1F00 | ('0' + ((floppy_irq_counter /       1L) % 10UL));
	vga_alpha_ram[i++] = 0x1F00 | ' ';

	floppy_irq_counter++;
}

void do_floppy_controller_hook_irq(struct floppy_controller *fdc) {
	if (my_floppy_irq_number >= 0 || my_floppy_old_irq != NULL || fdc->irq < 0)
		return;

	/* let the IRQ know what floppy controller */
	my_floppy_irq_floppy = fdc;

	/* enable on floppy controller */
	p8259_mask(fdc->irq);
	floppy_controller_enable_dma(fdc,1);

	/* hook IRQ */
	my_floppy_old_irq = _dos_getvect(irq2int(fdc->irq));
	_dos_setvect(irq2int(fdc->irq),my_floppy_irq);
	my_floppy_irq_number = fdc->irq;

	/* enable at PIC */
	p8259_unmask(fdc->irq);
}

void do_floppy_controller_unhook_irq(struct floppy_controller *fdc) {
	if (my_floppy_irq_number < 0 || my_floppy_old_irq == NULL || fdc->irq < 0)
		return;

	/* disable on floppy controller, then mask at PIC */
	p8259_mask(fdc->irq);
	floppy_controller_enable_dma(fdc,0);

	/* restore the original vector */
	_dos_setvect(irq2int(fdc->irq),my_floppy_old_irq);
	my_floppy_irq_number = -1;
	my_floppy_old_irq = NULL;
}

void do_floppy_controller_enable_irq(struct floppy_controller *fdc,unsigned char en) {
	if (!en || fdc->irq < 0 || fdc->irq != my_floppy_irq_number)
		do_floppy_controller_unhook_irq(fdc);
	if (en && fdc->irq >= 0)
		do_floppy_controller_hook_irq(fdc);
}

void do_floppy_controller(struct floppy_controller *fdc) {
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	unsigned int x,y;
	char redraw=1;
	int select=-1;
	int c;

	/* if the floppy struct says to use interrupts, then do it */
	do_floppy_controller_enable_irq(fdc,fdc->use_dma);

	while (1) {
		if (backredraw) {
			vga = vga_alpha_ram;
			backredraw = 0;
			redraw = 1;

			for (y=0;y < vga_height;y++) {
				for (x=0;x < vga_width;x++) {
					*vga++ = 0x1E00 + 177;
				}
			}

			vga_moveto(0,0);

			vga_write_color(0x1F);
			vga_write("        Floppy controller ");
			sprintf(tmp,"@%X",fdc->base_io);
			vga_write(tmp);
			if (fdc->irq >= 0) {
				sprintf(tmp," IRQ %d",fdc->irq);
				vga_write(tmp);
			}
			if (fdc->dma >= 0) {
				sprintf(tmp," DMA %d",fdc->dma);
				vga_write(tmp);
			}
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');
		}

		if (redraw) {
			redraw = 0;

			floppy_controller_read_status(fdc);

			y = 3;
			vga_moveto(8,y++);
			vga_write_color(0x0F);
			sprintf(tmp,"DOR: 0x%02x Status: 0x%02x",fdc->digital_out,fdc->main_status);
			vga_write(tmp);

			y = 5;
			vga_moveto(8,y++);
			vga_write_color((select == -1) ? 0x70 : 0x0F);
			vga_write("Main menu");
			while (vga_pos_x < (vga_width-8) && vga_pos_x != 0) vga_writec(' ');
			y++;

			vga_moveto(8,y++);
			vga_write_color((select == 0) ? 0x70 : 0x0F);
			vga_write("DMA/IRQ: ");
			vga_write(fdc->use_dma ? "Enabled" : "Disabled");
			vga_write(" (hit enter to toggle)");
			while (vga_pos_x < (vga_width-8) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 1) ? 0x70 : 0x0F);
			vga_write("Reset signal: ");
			vga_write((fdc->digital_out&0x04) ? "Off" : "On");
			vga_write(" (hit enter to toggle)");
			while (vga_pos_x < (vga_width-8) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 2) ? 0x70 : 0x0F);
			vga_write("Drive A motor: ");
			vga_write((fdc->digital_out&0x10) ? "On" : "Off");
			vga_write(" (hit enter to toggle)");
			while (vga_pos_x < (vga_width-8) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 3) ? 0x70 : 0x0F);
			vga_write("Drive B motor: ");
			vga_write((fdc->digital_out&0x20) ? "On" : "Off");
			vga_write(" (hit enter to toggle)");
			while (vga_pos_x < (vga_width-8) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 4) ? 0x70 : 0x0F);
			vga_write("Drive C motor: ");
			vga_write((fdc->digital_out&0x40) ? "On" : "Off");
			vga_write(" (hit enter to toggle)");
			while (vga_pos_x < (vga_width-8) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 5) ? 0x70 : 0x0F);
			vga_write("Drive D motor: ");
			vga_write((fdc->digital_out&0x80) ? "On" : "Off");
			vga_write(" (hit enter to toggle)");
			while (vga_pos_x < (vga_width-8) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 6) ? 0x70 : 0x0F);
			vga_write("Drive select: ");
			sprintf(tmp,"%c(%u)",(fdc->digital_out&3)+'A',fdc->digital_out&3);
			vga_write(tmp);
			vga_write(" (hit enter to toggle)");
			while (vga_pos_x < (vga_width-8) && vga_pos_x != 0) vga_writec(' ');
		}

		c = getch();
		if (c == 0) c = getch() << 8;

		if (c == 27) {
			break;
		}
		else if (c == 13) {
			if (select == -1) {
				break;
			}
			else if (select == 0) { /* DMA/IRQ enable */
				floppy_controller_enable_dma(fdc,!fdc->use_dma);
				redraw = 1;
			}
			else if (select == 1) { /* reset */
				floppy_controller_set_reset(fdc,!!(fdc->digital_out&0x04)); /* bit is INVERTED 1=normal 0=reset */
				redraw = 1;
			}
			else if (select == 2) { /* Drive A motor */
				floppy_controller_set_motor_state(fdc,0/*A*/,!(fdc->digital_out&0x10));
				redraw = 1;
			}
			else if (select == 3) { /* Drive B motor */
				floppy_controller_set_motor_state(fdc,1/*B*/,!(fdc->digital_out&0x20));
				redraw = 1;
			}
			else if (select == 4) { /* Drive C motor */
				floppy_controller_set_motor_state(fdc,2/*C*/,!(fdc->digital_out&0x40));
				redraw = 1;
			}
			else if (select == 5) { /* Drive D motor */
				floppy_controller_set_motor_state(fdc,3/*D*/,!(fdc->digital_out&0x80));
				redraw = 1;
			}
			else if (select == 6) { /* Drive select */
				floppy_controller_drive_select(fdc,((fdc->digital_out&3)+1)&3);
				redraw = 1;
			}
		}
		else if (c == 0x4800) {
			if (--select < -1)
				select = 6;

			redraw = 1;
		}
		else if (c == 0x5000) {
			if (++select > 6)
				select = -1;

			redraw = 1;
		}
	}

	do_floppy_controller_enable_irq(fdc,0);
	floppy_controller_enable_dma_otr(fdc,1); /* because BIOSes probably won't */
	p8259_unmask(fdc->irq);
}

void do_main_menu() {
	char redraw=1;
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	struct floppy_controller *floppy;
	unsigned int x,y,i;
	int select=-1;
	int c;

	while (1) {
		if (backredraw) {
			vga = vga_alpha_ram;
			backredraw = 0;
			redraw = 1;

			for (y=0;y < vga_height;y++) {
				for (x=0;x < vga_width;x++) {
					*vga++ = 0x1E00 + 177;
				}
			}

			vga_moveto(0,0);

			vga_write_color(0x1F);
			vga_write("        Floppy controller test program");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');
		}

		if (redraw) {
			redraw = 0;

			y = 5;
			vga_moveto(8,y++);
			vga_write_color((select == -1) ? 0x70 : 0x0F);
			vga_write("Exit program");
			y++;

			for (i=0;i < MAX_FLOPPY_CONTROLLER;i++) {
				floppy = floppy_get_controller(i);
				if (floppy != NULL) {
					vga_moveto(8,y++);
					vga_write_color((select == (int)i) ? 0x70 : 0x0F);

					sprintf(tmp,"Controller @ %04X",floppy->base_io);
					vga_write(tmp);

					if (floppy->irq >= 0) {
						sprintf(tmp," IRQ %2d",floppy->irq);
						vga_write(tmp);
					}

					if (floppy->dma >= 0) {
						sprintf(tmp," DMA %2d",floppy->dma);
						vga_write(tmp);
					}
				}
			}
		}

		c = getch();
		if (c == 0) c = getch() << 8;

		if (c == 27) {
			break;
		}
		else if (c == 13) {
			if (select == -1) {
				break;
			}
			else if (select >= 0 && select < MAX_FLOPPY_CONTROLLER) {
				floppy = floppy_get_controller(select);
				if (floppy != NULL) do_floppy_controller(floppy);
				backredraw = redraw = 1;
			}
		}
		else if (c == 0x4800) {
			if (select <= -1)
				select = MAX_FLOPPY_CONTROLLER - 1;
			else
				select--;

			while (select >= 0 && floppy_get_controller(select) == NULL)
				select--;

			redraw = 1;
		}
		else if (c == 0x5000) {
			select++;
			while (select >= 0 && select < MAX_FLOPPY_CONTROLLER && floppy_get_controller(select) == NULL)
				select++;
			if (select >= MAX_FLOPPY_CONTROLLER)
				select = -1;

			redraw = 1;
		}
	}
}

int main(int argc,char **argv) {
	struct floppy_controller *reffdc;
	struct floppy_controller *newfdc;
	int i;

	/* we take a GUI-based approach (kind of) */
	if (!probe_vga()) {
		printf("Cannot init VGA\n");
		return 1;
	}
	/* the floppy code has some timing requirements and we'll use the 8254 to do it */
	/* newer motherboards don't even have a floppy controller and it's probable they'll stop implementing the 8254 at some point too */
	if (!probe_8254()) {
		printf("8254 chip not detected\n");
		return 1;
	}
	if (!probe_8259()) {
		printf("8259 chip not detected\n");
		return 1;
	}
	if (!init_floppy_controller_lib()) {
		printf("Failed to init floppy controller\n");
		return 1;
	}

	printf("Probing standard FDC ports...\n");
	for (i=0;(reffdc = (struct floppy_controller*)floppy_get_standard_isa_port(i)) != NULL;i++) {
		printf("   %3X IRQ %d DMA %d: ",reffdc->base_io,reffdc->irq,reffdc->dma); fflush(stdout);

		if ((newfdc = floppy_controller_probe(reffdc)) != NULL) {
			printf("FOUND. PS/2 mode=%u use dma=%u DOR R/W=%u DOR=0x%02x mstat=0x%02x\n",
				newfdc->ps2_mode,
				newfdc->use_dma,
				newfdc->digital_out_rw,
				newfdc->digital_out,
				newfdc->main_status);
		}
		else {
			printf("\x0D                             \x0D"); fflush(stdout);
		}
	}

	printf("Hit ENTER to continue, ESC to cancel\n");
	i = wait_for_enter_or_escape();
	if (i == 27) {
		free_floppy_controller_lib();
		return 0;
	}

	do_main_menu();
	free_floppy_controller_lib();
	return 0;
}

