
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#include <hw/8237/8237.h>		/* 8237 DMA */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/ultrasnd/ultrasnd.h>
#include <hw/dos/tgusmega.h>
#include <hw/dos/tgussbos.h>
#include <hw/dos/doswin.h>
#include <hw/vga/vgagui.h>
#include <hw/vga/vgatty.h>

static struct ultrasnd_ctx*	gus = NULL;

static volatile unsigned char	IRQ_anim = 0;
static volatile unsigned char	gus_irq_count = 0;

static unsigned char		animator = 0;
static unsigned char		old_irq_masked = 0;
static unsigned char		dont_chain_irq = 0;
static unsigned char		no_dma=0;

static const struct vga_menu_item menu_separator =
	{(char*)1,		's',	0,	0};

static const struct vga_menu_item main_menu_file_quit =
	{"Quit",		'q',	0,	0};

static const struct vga_menu_item* main_menu_file[] = {
	&main_menu_file_quit,
	NULL
};

static const struct vga_menu_item main_menu_help_about =
	{"About",		'r',	0,	0};

static const struct vga_menu_item* main_menu_help[] = {
	&main_menu_help_about,
	NULL
};

static const struct vga_menu_bar_item main_menu_bar[] = {
	/* name                 key     scan    x       w       id */
	{" File ",		'F',	0x21,	0,	6,	&main_menu_file}, /* ALT-F */
	{" Help ",		'H',	0x23,	6,	6,	&main_menu_help}, /* ALT-H */
	{NULL,			0,	0x00,	0,	0,	0}
};

static void ui_anim(int force) {
	VGA_ALPHA_PTR wr = vga_alpha_ram + 10;

	{
		static const unsigned char anims[] = {'-','/','|','\\'};
		if (++animator >= 4) animator = 0;
		wr = vga_alpha_ram + 79;
		*wr = anims[animator] | 0x1E00;
	}
}
	
static void my_vga_menu_idle() {
	ui_anim(0);
}

static void draw_irq_indicator() {
	VGA_ALPHA_PTR wr = vga_alpha_ram;
	unsigned char i;

	wr[0] = 0x1E00 | 'G';
	wr[1] = 0x1E00 | 'U';
	wr[2] = 0x1E00 | 'S';
	wr[3] = 0x1E00 | 'I';
	wr[4] = 0x1E00 | 'R';
	wr[5] = 0x1E00 | 'Q';
	for (i=0;i < 4;i++) wr[i+6] = (uint16_t)(i == IRQ_anim ? 'x' : '-') | 0x1E00;
}

/* WARNING!!! This interrupt handler calls subroutines. To avoid system
 * instability in the event the IRQ fires while, say, executing a routine
 * in the DOS kernel, you must compile this code with the -zu flag in
 * 16-bit real mode Large and Compact memory models! Without -zu, minor
 * memory corruption in the DOS kernel will result and the system will
 * hang and/or crash. */
static void (interrupt *old_irq)() = NULL;
static void interrupt gus_irq() {
	gus_irq_count++;
	if (++IRQ_anim >= 4) IRQ_anim = 0;
	draw_irq_indicator();

	if (old_irq_masked || old_irq == NULL || dont_chain_irq) {
		/* ack the interrupt ourself, do not chain */
		if (gus->irq1 >= 8) p8259_OCW2(8,P8259_OCW2_NON_SPECIFIC_EOI);
		p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
	}
	else {
		/* chain to the previous IRQ, who will acknowledge the interrupt */
		old_irq();
	}
}

static void help() {
	printf("test [options]\n");
#if !(TARGET_MSDOS == 16 && defined(__COMPACT__)) /* this is too much to cram into a small model EXE */
	printf(" /h /help             This help\n");
	printf(" /nochain             Don't chain to previous IRQ (sound blaster IRQ)\n");
	printf(" /nodma               Don't use DMA channel\n");
	printf(" /d                   Enable GUS library debug messages\n");
#endif
}

int confirm_quit() {
	/* FIXME: Why does this cause Direct DSP playback to horrifically slow down? */
	return confirm_yes_no_dialog("Are you sure you want to exit to DOS?");
}

int main(int argc,char **argv) {
	const struct vga_menu_item *mitem = NULL;
	int i,loop,redraw,bkgndredraw,cc;
	VGA_ALPHA_PTR vga;

	printf("Gravis Ultrasound test program\n");
	probe_dos();
	detect_windows();
	if (!probe_vga()) {
		printf("Cannot init VGA\n");
		return 1;
	}
	if (!probe_8237()) {
		printf("Cannot init 8237 DMA\n");
		return 1;
	}
	if (!probe_8259()) {
		printf("Cannot init 8259 PIC\n");
		return 1;
	}
	if (!probe_8254()) {
		printf("Cannot init 8254 timer\n");
		return 1;
	}
	if (!init_ultrasnd()) {
		printf("Cannot init library\n");
		return 1;
	}

	for (i=1;i < argc;i++) {
		char *a = argv[i];

		if (*a == '-' || *a == '/') {
			unsigned char m = *a++;
			while (*a == m) a++;

			if (!strcmp(a,"d"))
				ultrasnd_debug(1);
			else if (!strcmp(a,"nodma"))
				no_dma = 1;
			else if (!strcmp(a,"nochain"))
				dont_chain_irq = 1;
			else {
				help();
				return 1;
			}
		}
	}

	if (ultrasnd_try_ultrasnd_env() != NULL) {
		printf("ULTRASND environment variable: PORT=0x%03x IRQ=%d,%d DMA=%d,%d\n",
			ultrasnd_env->port,
			ultrasnd_env->irq1,
			ultrasnd_env->irq2,
			ultrasnd_env->dma1,
			ultrasnd_env->dma2);

		/* NTS: Most programs linked against the GUS SDK program the IRQ and DMA into the card
		 *      from the ULTRASND environment variable. So that's what we do here. */
		if (ultrasnd_probe(ultrasnd_env,1)) {
			printf("Never mind, doesn't work\n");
			ultrasnd_free_card(ultrasnd_env);
		}
	}

	/* NTS: We no longer auto-probe for the card (at this time) because it is also standard for DOS
	 *      programs using the GUS SDK to expect the ULTRASND variable */

	for (i=0;i < MAX_ULTRASND;i++) {
		gus = &ultrasnd_card[i];
		if (ultrasnd_card_taken(gus)) {
			printf("[%u] RAM=%dKB PORT=0x%03x IRQ=%d,%d DMA=%d,%d 256KB-boundary=%u voices=%u/%uHz\n",
				i+1,
				(int)(gus->total_ram >> 10UL),
				gus->port,
				gus->irq1,
				gus->irq2,
				gus->dma1,
				gus->dma2,
				gus->boundary256k,
				gus->active_voices,
				gus->output_rate);
		}
		gus = NULL;
	}
	printf("Which card to use? "); fflush(stdout);
	i = -1; scanf("%d",&i); i--;
	if (i < 0 || i >= MAX_ULTRASND || !ultrasnd_card_taken(&ultrasnd_card[i])) return 1;
	gus = &ultrasnd_card[i];
	if (no_dma) gus->use_dma = 0;

	if (gus->irq1 >= 0) {
		old_irq_masked = p8259_is_masked(gus->irq1);
		old_irq = _dos_getvect(irq2int(gus->irq1));
		_dos_setvect(irq2int(gus->irq1),gus_irq);
		p8259_unmask(gus->irq1);
	}

	i = int10_getmode();
	if (i != 3) int10_setmode(3);
	
	vga_write_color(0x07);
	vga_clear();

	loop=1;
	redraw=1;
	bkgndredraw=1;

	vga_menu_bar.bar = main_menu_bar;
	vga_menu_bar.sel = -1;
	vga_menu_bar.row = 1;
	vga_menu_idle = my_vga_menu_idle;
	
	while (loop) {
		if ((mitem = vga_menu_bar_keymon()) != NULL) {
			/* act on it */
			if (mitem == &main_menu_file_quit) {
				if (confirm_quit()) {
					loop = 0;
					break;
				}
			}
			else if (mitem == &main_menu_help_about) {
				struct vga_msg_box box;
				vga_msg_box_create(&box,"Gravis Ultrasound test program v1.0 for DOS\n\n(C) 2011-2016 Jonathan Campbell\nALL RIGHTS RESERVED\n"
#if TARGET_MSDOS == 32
					"32-bit protected mode version"
#elif defined(__LARGE__)
					"16-bit real mode (large model) version"
#elif defined(__MEDIUM__)
					"16-bit real mode (medium model) version"
#elif defined(__COMPACT__)
					"16-bit real mode (compact model) version"
#else
					"16-bit real mode (small model) version"
#endif
					,0,0);
				while (1) {
					ui_anim(0);
					if (kbhit()) {
						i = getch();
						if (i == 0) i = getch() << 8;
						if (i == 13 || i == 27) break;
					}
				}
				vga_msg_box_destroy(&box);
			}
		}

		if (redraw || bkgndredraw) {
			if (bkgndredraw) {
				for (vga=vga_alpha_ram,cc=0;cc < (80*1);cc++) *vga++ = 0x1E00 | 32;
				for (vga=vga_alpha_ram+(80*2),cc=0;cc < (80*23);cc++) *vga++ = 0x1E00 | 177;
				vga_menu_bar_draw();
				draw_irq_indicator();
			}
			_cli();
			bkgndredraw = 0;
			redraw = 0;
			_sti();
		}

		if (kbhit()) {
			i = getch();
			if (i == 0) i = getch() << 8;

			if (i == 27) {
				if (confirm_quit()) {
					loop = 0;
					break;
				}
			}
		}

		ui_anim(0);
	}
	
	vga_write_sync();
	if (gus->irq1 >= 0 && old_irq_masked)
		p8259_mask(gus->irq1);

	printf("Releasing IRQ...\n");
	if (gus->irq1 >= 0)
		_dos_setvect(irq2int(gus->irq1),old_irq);

	ultrasnd_abort_dma_transfer(gus);
	ultrasnd_stop_all_voices(gus);
	ultrasnd_stop_timers(gus);
	ultrasnd_drain_irq_events(gus);
	printf("Freeing buffer...\n");
	return 0;
}

