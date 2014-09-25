
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

	/* desired state */
	uint8_t			use_dma:1;		/* if set, use DMA. else, use PIO data transfer */

	/* what we know about the controller */
	uint8_t			ps2_mode:1;		/* controller is in PS/2 mode (has status registers A and B I/O ports) */
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

struct floppy_controller *alloc_floppy_controller() {
	unsigned int i=0;

	while (i < MAX_FLOPPY_CONTROLLER) {
		if (floppy_controllers[i].base_io == 0)
			return &floppy_controllers[i];
	}

	return NULL;
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

	ret->use_dma = (ret->dma >= 0);

	/* if something appears at 0x3F0-0x3F1, assume "PS/2 mode".
	 * there are finer intricate details where the meaning of some bits
	 * completely change or invert their meaning between PS/2 and Model 30,
	 * so we don't concern ourself with them, we only care that there's
	 * something there and we can let the program using this lib figure it out. */
	t1 = inp(i->base_io+0);
	t1 &= inp(i->base_io+1);
	if (t1 != 0xFF) ret->ps2_mode = 1;

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
			printf("FOUND\n");
		}
		else {
			printf("\x0D                             \x0D"); fflush(stdout);
		}
	}

	free_floppy_controller_lib();
	return 0;
}

