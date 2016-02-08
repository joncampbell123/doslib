
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

static char			temp_str[1024];

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

static const struct vga_menu_item main_menu_file_load_wav =
	{"Load WAV",		'l',	0,	0};

static const struct vga_menu_item* main_menu_file[] = {
	&main_menu_file_load_wav,
	&menu_separator,
	&main_menu_file_quit,
	NULL
};

static const struct vga_menu_item main_menu_help_about =
	{"About",		'r',	0,	0};

static const struct vga_menu_item* main_menu_help[] = {
	&main_menu_help_about,
	NULL
};

static const struct vga_menu_item main_menu_hardware_reset =
	{"Reset",		'r',	0,	0};

static const struct vga_menu_item main_menu_hardware_zero_gus_ram =
	{"Zero GUS RAM",	'z',	0,	0};

static const struct vga_menu_item main_menu_hardware_play_voice =
	{"Play voice",		'p',	0,	0};

static const struct vga_menu_item* main_menu_hardware[] = {
	&main_menu_hardware_reset,
	&main_menu_hardware_zero_gus_ram,
	&menu_separator,
	&main_menu_hardware_play_voice,
	NULL
};

static const struct vga_menu_bar_item main_menu_bar[] = {
	/* name                 key     scan    x       w       id */
	{" File ",		'F',	0x21,	0,	6,	&main_menu_file}, /* ALT-F */
	{" Hardware ",          'a',    0x1E,   6,      10,     &main_menu_hardware}, /* ALT-A */
	{" Help ",		'H',	0x23,	16,	6,	&main_menu_help}, /* ALT-H */
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

static char wav_file[256] = {0};

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

int wav_fd = -1;
unsigned int wav_bits = 0;
unsigned int wav_channels = 0;
unsigned long wav_sample_rate = 0;
unsigned long wav_data_offset = 0;
unsigned long wav_data_size = 0;

void close_wav() {
	if (wav_fd >= 0) {
		close(wav_fd);
		wav_fd = -1;
	}
}

int open_wav() {
	unsigned long len;
	unsigned char tmp[128];

	if (wav_fd >= 0)
		return 1;
	if (wav_file[0] == 0)
		return 0;

	wav_fd = open(wav_file,O_RDONLY | O_BINARY);
	if (wav_fd < 0) return 0;

	if (read(wav_fd,tmp,12) < 12) {
		close_wav();
		return 0;
	}
	if (memcmp(tmp,"RIFF",4) != 0 || memcmp(tmp+8,"WAVE",4) != 0) {
		close_wav();
		return 0;
	}

	/* next should be 'fmt ' */
	if (read(wav_fd,tmp,8) < 8) {
		close_wav();
		return 0;
	}
	if (memcmp(tmp,"fmt ",4) != 0) {
		close_wav();
		return 0;
	}
	len = *((uint32_t*)(tmp+4));
	if (len > 128UL || len < 16UL) {
		close_wav();
		return 0;
	}
	if (read(wav_fd,tmp,(int)len) < (int)len) {
		close_wav();
		return 0;
	}
	/*
	 * WORD  wFormatTag;                        +0
	 * WORD  nChannels;                         +2
	 * DWORD nSamplesPerSec;                    +4
	 * DWORD nAvgBytesPerSec;                   +8
	 * WORD  nBlockAlign;                       +12
	 * WORD  wBitsPerSample;                    +14
	 *                                          =16
	 */
	if (*((uint16_t*)(tmp+0)) != 1/*WAVE_FORMAT_PCM*/) {
		close_wav();
		return 0;
	}
	wav_bits = *((uint16_t*)(tmp+14));
	wav_channels = *((uint16_t*)(tmp+2));
	wav_sample_rate = *((uint32_t*)(tmp+4));
	if ((wav_bits != 8 && wav_bits != 16) || wav_channels < 1 || wav_channels > 2 || wav_sample_rate < 1000UL || wav_sample_rate > 192000UL) {
		close_wav();
		return 0;
	}

again:	if (read(wav_fd,tmp,8) < 8) {
		close_wav();
		return 0;
	}
	len = *((uint32_t*)(tmp+4));
	if (memcmp(tmp,"data",4) != 0) {
		lseek(wav_fd,len,SEEK_CUR);
		goto again;
	}
	wav_data_offset = lseek(wav_fd,0,SEEK_CUR);
	wav_data_size = len;
	return 1;
}

int prompt_wav(unsigned char rec) {
	int ok = 0;
	unsigned char gredraw = 1;
#if TARGET_MSDOS == 16 && (defined(__COMPACT__) || defined(__SMALL__))
#else
	struct find_t ft;
#endif

	{
		const char *rp;
		char temp[sizeof(wav_file)];
		int cursor = strlen(wav_file),i,c,redraw=1;
		memcpy(temp,wav_file,strlen(wav_file)+1);
		while (!ok) {
			if (gredraw) {
#if TARGET_MSDOS == 16 && (defined(__COMPACT__) || defined(__SMALL__))
#else
				char *cwd;
#endif

				gredraw = 0;
				vga_clear();
				vga_moveto(0,4);
				vga_write_color(0x07);
				vga_write("Enter WAV file path:\n");
				vga_write_sync();
				draw_irq_indicator();
				ui_anim(1);
				redraw = 1;

#if TARGET_MSDOS == 16 && (defined(__COMPACT__) || defined(__SMALL__))
#else
				cwd = getcwd(NULL,0);
				if (cwd) {
					vga_moveto(0,6);
					vga_write_color(0x0B);
					vga_write(cwd);
					vga_write_sync();
				}

				if (_dos_findfirst("*.*",_A_NORMAL|_A_RDONLY,&ft) == 0) {
					int x=0,y=7,cw = 14,i;
					char *ex;

					do {
						ex = strrchr(ft.name,'.');
						if (!ex) ex = "";

						if (ft.attrib&_A_SUBDIR) {
							vga_write_color(0x0F);
						}
						else if (!strcasecmp(ex,".wav")) {
							vga_write_color(0x1E);
						}
						else {
							vga_write_color(0x07);
						}
						vga_moveto(x,y);
						for (i=0;i < 13 && ft.name[i] != 0;) vga_writec(ft.name[i++]);
						for (;i < 14;i++) vga_writec(' ');

						x += cw;
						if ((x+cw) > vga_width) {
							x = 0;
							if (y >= vga_height) break;
							y++;
						}
					} while (_dos_findnext(&ft) == 0);

					_dos_findclose(&ft);
				}
#endif
			}
			if (redraw) {
				rp = (const char*)temp;
				vga_moveto(0,5);
				vga_write_color(0x0E);
				for (i=0;i < 80;i++) {
					if (*rp != 0)	vga_writec(*rp++);
					else		vga_writec(' ');	
				}
				vga_moveto(cursor,5);
				vga_write_sync();
				redraw=0;
			}

			if (kbhit()) {
				c = getch();
				if (c == 0) c = getch() << 8;

				if (c == 27) {
					ok = -1;
				}
				else if (c == 13) {
#if TARGET_MSDOS == 16 && (defined(__COMPACT__) || defined(__SMALL__))
					ok = 1;
#else
					struct stat st;

					if (isalpha(temp[0]) && temp[1] == ':' && temp[2] == 0) {
						unsigned int total;

						_dos_setdrive(tolower(temp[0])+1-'a',&total);
						temp[0] = 0;
						gredraw = 1;
						cursor = 0;
					}
					else if (stat(temp,&st) == 0) {
						if (S_ISDIR(st.st_mode)) {
							chdir(temp);
							temp[0] = 0;
							gredraw = 1;
							cursor = 0;
						}
						else {
							ok = 1;
						}
					}
					else {
						ok = 1;
					}
#endif
				}
				else if (c == 8) {
					if (cursor != 0) {
						temp[--cursor] = 0;
						redraw = 1;
					}
				}
				else if (c >= 32 && c < 256) {
					if (cursor < 79) {
						temp[cursor++] = (char)c;
						temp[cursor  ] = (char)0;
						redraw = 1;
					}
				}
			}
			else {
				ui_anim(0);
			}
		}

		if (ok == 1) {
			memcpy(wav_file,temp,strlen(temp)+1);
		}
	}

	return ok;
}

static unsigned char gus_dma_tc_ignore = 0;

static unsigned long gus_dma_tc = 0;
static unsigned long gus_irq_voice = 0;
static unsigned long gus_timer_ticks = 0;
static unsigned char gus_timer_ctl = 0;
static unsigned char gus_ignore_irq = 0;

/* WARNING!!! This interrupt handler calls subroutines. To avoid system
 * instability in the event the IRQ fires while, say, executing a routine
 * in the DOS kernel, you must compile this code with the -zu flag in
 * 16-bit real mode Large and Compact memory models! Without -zu, minor
 * memory corruption in the DOS kernel will result and the system will
 * hang and/or crash. */
static void (interrupt *old_irq)() = NULL;
static void interrupt gus_irq() {
	unsigned char irq_stat,c;

	gus_irq_count++;
	if (++IRQ_anim >= 4) IRQ_anim = 0;
	draw_irq_indicator();

	do {
		irq_stat = inp(gus->port+6) & (~gus_ignore_irq);

		if ((irq_stat & 0x80) && (!gus_dma_tc_ignore)) { // bit 7
			/* DMA TC. read and clear. */
			c = ultrasnd_select_read(gus,0x41);
			if (c & 0x40) {
				gus->dma_tc_irq_happened = 1;
				gus_dma_tc++;
			}
		}
		else if (irq_stat & 0x60) { // bits 6-5
			c = ultrasnd_select_read(gus,0x8F);
			if ((c & 0xC0) != 0xC0) {
				gus_irq_voice++;
			}
		}
		else if (irq_stat & 0x0C) { // bit 3-4
			if (gus_timer_ctl != 0) {
				ultrasnd_select_write(gus,0x45,0x00);
				ultrasnd_select_write(gus,0x45,gus_timer_ctl); /* enable timer 1 IRQ */
			}

			gus_timer_ticks++;
		}
		else {
			break;
		}
	} while (1);

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

unsigned char select_voice = 0;

static void do_play_voice() {
	uint16_t a,b;
	int c,rows=3+3,cols=78;
	uint16_t voice_freq=0;
	uint32_t voice_current=0,voice_start=0,voice_end=0,cc32;
	unsigned char voice_mode = 0,cc;
	unsigned char fredraw = 1;
	unsigned char redraw = 1;
	struct vga_msg_box box;

	vga_msg_box_create(&box,"",rows,cols); /* 3 rows 70 cols */

	while (1) {
		ui_anim(0);

		_cli();

		ultrasnd_select_voice(gus,select_voice);
		a = ultrasnd_select_read16(gus,0x81);
		if (voice_freq != a) redraw = 1;
		voice_freq = a;

		ultrasnd_select_voice(gus,select_voice);
		cc32  = (uint32_t)ultrasnd_select_read16(gus,0x82) << (uint32_t)16UL;
		cc32 |= (uint32_t)ultrasnd_select_read16(gus,0x83);
		cc32 &= (uint32_t)0x1FFFFFE0UL; /* bits 15-13 not used. bits 4-0 not used */
		if (voice_start != cc32) redraw = 1;
		voice_start = cc32;

		ultrasnd_select_voice(gus,select_voice);
		cc32  = (uint32_t)ultrasnd_select_read16(gus,0x84) << (uint32_t)16UL;
		cc32 |= (uint32_t)ultrasnd_select_read16(gus,0x85);
		cc32 &= (uint32_t)0x1FFFFFE0UL; /* bits 15-13 not used. bits 4-0 not used */
		if (voice_end != cc32) redraw = 1;
		voice_end = cc32;

		ultrasnd_select_voice(gus,select_voice);
		do {
			a = ultrasnd_select_read16(gus,0x8A);
			cc32 = (uint32_t)ultrasnd_select_read16(gus,0x8B);
			b = ultrasnd_select_read16(gus,0x8A);
		} while (a != b);
		cc32 |= (uint32_t)a << (uint32_t)16;
		cc32 &= (uint32_t)0x1FFFFFE0UL; /* bits 15-13 not used. bits 4-0 not used */
		if (voice_current != cc32) redraw = 1;
		voice_current = cc32;

		cc = ultrasnd_read_voice_mode(gus,select_voice);
		if (voice_mode != cc) redraw = 1;
		voice_mode = cc;

		_sti();

		if (redraw || fredraw) {
			if (fredraw) {
				vga_moveto(box.x+2,box.y+1);
				vga_write_color(0x1E);
				sprintf(temp_str,"GUS voice #%d ",select_voice+1);
				vga_write(temp_str);
			}
			vga_moveto(box.x+2+17,box.y+1);
			vga_write_color(0x1F);
			sprintf(temp_str,"STOP=%u:%u %2u-bit LOOP=%u BIDIR=%u IRQ=%u BACK=%u PEND=%u",
				(voice_mode&ULTRASND_VOICE_MODE_IS_STOPPED)		?  1 : 0,
				(voice_mode&ULTRASND_VOICE_MODE_STOP)			?  1 : 0,
				(voice_mode&ULTRASND_VOICE_MODE_16BIT)			? 16 : 8,
				(voice_mode&ULTRASND_VOICE_MODE_LOOP)			?  1 : 0,
				(voice_mode&ULTRASND_VOICE_MODE_BIDIR)			?  1 : 0,
				(voice_mode&ULTRASND_VOICE_MODE_IRQ)			?  1 : 0,
				(voice_mode&ULTRASND_VOICE_MODE_BACKWARDS)		?  1 : 0,
				(voice_mode&ULTRASND_VOICE_MODE_IRQ_PENDING)		?  1 : 0);
			vga_write(temp_str);

			if (fredraw) {
				vga_moveto(box.x+2,box.y+2);
				vga_write_color(0x1E);
				sprintf(temp_str,"Position: ");
				vga_write(temp_str);
			}
			vga_moveto(box.x+2+17,box.y+2);
			vga_write_color(0x1F);
			sprintf(temp_str,"Start %05lx.%x  Current %05lx.%x  End %05lx.%x  FC %02lx.%03x",
				(unsigned long)((voice_start >> 9UL) & 0xFFFFFUL),
				(unsigned int) ((voice_start >> 5UL) & 0xFUL),

				(unsigned long)((voice_current >> 9UL) & 0xFFFFFUL),
				(unsigned int) ((voice_current >> 5UL) & 0xFUL),

				(unsigned long)((voice_end >> 9UL) & 0xFFFFFUL),
				(unsigned int) ((voice_end >> 5UL) & 0xFUL),
				
				(unsigned long)((voice_freq >> 10UL) & 0xFFUL),
				(unsigned int)(((voice_freq >>  1UL) & 0x1FFUL) << 3UL));
			vga_write(temp_str);

			vga_moveto(box.x+2,box.y+5);
			vga_write_color(0x1F);
			vga_write("6=8/16-bit ");
			vga_write_color((gus_ignore_irq & 0x60) ? 0x1E : 0x1F);
			vga_write("I=ignorevoiceIRQ ");
			vga_write_color(0x1F);
			vga_write("S=read2X6 V=clearvoiceIRQ D=clearDMAIRQ");

			vga_moveto(box.x+2,box.y+6);
			vga_write_color(0x1F);
			vga_write("b=bidir r=reverse l=loop p=pending i=irq F=FF s>start e>end <>step SPC=play");

			fredraw = 0;
			redraw = 0;
		}

		if (kbhit()) {
			c = getch();
			if (c == 0) c = getch() << 8;

			if (c == 27) {
				break;
			}
			else if (c == '6') {
				_cli();
				voice_mode = ultrasnd_read_voice_mode(gus,select_voice);
				ultrasnd_set_voice_mode(gus,select_voice,voice_mode ^ ULTRASND_VOICE_MODE_16BIT);
				_sti();
			}
			else if (c == 'b') {
				_cli();
				voice_mode = ultrasnd_read_voice_mode(gus,select_voice);
				ultrasnd_set_voice_mode(gus,select_voice,voice_mode ^ ULTRASND_VOICE_MODE_BIDIR);
				_sti();
			}
			else if (c == 'r') {
				_cli();
				voice_mode = ultrasnd_read_voice_mode(gus,select_voice);
				ultrasnd_set_voice_mode(gus,select_voice,voice_mode ^ ULTRASND_VOICE_MODE_BACKWARDS);
				_sti();
			}
			else if (c == 'l') {
				_cli();
				voice_mode = ultrasnd_read_voice_mode(gus,select_voice);
				ultrasnd_set_voice_mode(gus,select_voice,voice_mode ^ ULTRASND_VOICE_MODE_LOOP);
				_sti();
			}
			else if (c == 'p') {
				_cli();
				voice_mode = ultrasnd_read_voice_mode(gus,select_voice);
				ultrasnd_set_voice_mode(gus,select_voice,voice_mode ^ ULTRASND_VOICE_MODE_IRQ_PENDING);
				_sti();
			}
			else if (c == 'i') {
				_cli();
				voice_mode = ultrasnd_read_voice_mode(gus,select_voice);
				ultrasnd_set_voice_mode(gus,select_voice,voice_mode ^ ULTRASND_VOICE_MODE_IRQ);
				_sti();
			}
			else if (c == 'F') {
				_cli();
				ultrasnd_select_voice(gus,select_voice);
				ultrasnd_select_write(gus,0x00,0xFF);
				_sti();
			}
			else if (c == ' ') {
				if (voice_mode & (ULTRASND_VOICE_MODE_STOP|ULTRASND_VOICE_MODE_IS_STOPPED))
					ultrasnd_start_voice(gus,select_voice);
				else
					ultrasnd_stop_voice(gus,select_voice);
			}
			else if (c == 's') {
				_cli();
				if ((voice_mode&3) == 0) ultrasnd_stop_voice(gus,select_voice);
				ultrasnd_select_voice(gus,select_voice);
				ultrasnd_select_write16(gus,0x0A,voice_start >> 16UL);
				ultrasnd_select_write16(gus,0x0B,voice_start);
				if ((voice_mode & (ULTRASND_VOICE_MODE_STOP|ULTRASND_VOICE_MODE_IS_STOPPED)) == 0)
					ultrasnd_start_voice(gus,select_voice);
				_sti();
			}
			else if (c == 'e') {
				_cli();
				if ((voice_mode&3) == 0) ultrasnd_stop_voice(gus,select_voice);
				ultrasnd_select_voice(gus,select_voice);
				ultrasnd_select_write16(gus,0x0A,voice_end >> 16UL);
				ultrasnd_select_write16(gus,0x0B,voice_end);
				if ((voice_mode & (ULTRASND_VOICE_MODE_STOP|ULTRASND_VOICE_MODE_IS_STOPPED)) == 0)
					ultrasnd_start_voice(gus,select_voice);
				_sti();
			}
			else if (c == '.' || c == '>') { /* > */
				// confirm my theory the GUS is *always* rendering the voice's position whether it's moving or not,
				// by allowing the user to step the pointer
				_cli();
				if ((voice_mode&3) == 0) ultrasnd_stop_voice(gus,select_voice);
				ultrasnd_select_voice(gus,select_voice);
				voice_current += (c == '>' ? (512UL << 9UL) : (1UL << 9UL));
				ultrasnd_select_write16(gus,0x0A,voice_current >> 16UL);
				ultrasnd_select_write16(gus,0x0B,voice_current);
				redraw = 1;
				_sti();
			}
			else if (c == ',' || c == '<') { /* < */
				// confirm my theory the GUS is *always* rendering the voice's position whether it's moving or not,
				// by allowing the user to step the pointer
				_cli();
				if ((voice_mode&3) == 0) ultrasnd_stop_voice(gus,select_voice);
				ultrasnd_select_voice(gus,select_voice);
				voice_current -= (c == '<' ? (512UL << 9UL) : (1UL << 9UL));
				ultrasnd_select_write16(gus,0x0A,voice_current >> 16UL);
				ultrasnd_select_write16(gus,0x0B,voice_current);
				redraw = 1;
				_sti();
			}
			else if (c == 0x4800) { //up
				if (select_voice == 0) select_voice = gus->active_voices - 1;
				else select_voice--;
				redraw = fredraw = 1;
			}
			else if (c == 0x5000) { //down
				if (select_voice == (gus->active_voices-1)) select_voice = 0;
				else select_voice++;
				redraw = fredraw = 1;
			}
			else if (c == 'I') {
				if (gus_ignore_irq == 0)
					gus_ignore_irq = 0x60; /* ignore voice/ramp */
				else
					gus_ignore_irq = 0;

				redraw = 1;
			}
			else if (c == 'S') {
				_cli();
				inp(gus->port+6); // probably won't do anything, just checking
				_sti();
			}
			else if (c == 'V') {
				_cli();
				ultrasnd_select_read(gus,0x8F); // to clear stuck voice IRQs
				_sti();
			}
			else if (c == 'D') {
				_cli();
				ultrasnd_select_read(gus,0x41); // to clear stuck DMA IRQs
				_sti();
			}
		}
	}

	vga_msg_box_destroy(&box);
	gus_ignore_irq = 0;
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

	ultrasnd_set_active_voices(gus,14);

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
			else if (mitem == &main_menu_hardware_play_voice) {
				do_play_voice();
			}
			else if (mitem == &main_menu_hardware_zero_gus_ram) {
				uint32_t o;
				unsigned int count;
				struct vga_msg_box box;
				vga_msg_box_create(&box,"Clearing DRAM...     \n",0,0);

				_cli();
				ultrasnd_abort_dma_transfer(gus);
				_sti();

				for (o=0;o < gus->total_ram;o += 0x400UL/*1KB*/) {
					vga_moveto(box.x+2,box.y+2);
					vga_write_color(0x1F);
					sprintf(temp_str,"%%%02u %uKB/%uKB",
					(unsigned int)((o * 100UL) / gus->total_ram),
						(unsigned int)(o >> 10UL),
						(unsigned int)(gus->total_ram >> 10UL));
					vga_write(temp_str);

					if (kbhit()) {
						if (getch() == 27)
							break;
					}

					for (count=0;count < 0x400/*1KB*/;count++)
						ultrasnd_poke(gus,o+(uint32_t)count,0x00);
				}

				vga_msg_box_destroy(&box);
			}
			else if (mitem == &main_menu_file_load_wav) {
				unsigned char FAR *buf;

				buf = ultrasnd_dram_buffer_alloc(gus,32768);
				if (buf == NULL) buf = ultrasnd_dram_buffer_alloc(gus,16384);
				if (buf == NULL) buf = ultrasnd_dram_buffer_alloc(gus,8192);
				if (buf == NULL) buf = ultrasnd_dram_buffer_alloc(gus,4096);
				if (buf == NULL) buf = ultrasnd_dram_buffer_alloc(gus,2048);

				redraw=1;
				bkgndredraw=1;
				close_wav();
				if (prompt_wav(0) < 0) {
				}
				else if (!open_wav() || wav_data_size == 0UL) {
					fflush(stdin);
					vga_moveto(0,0);
					vga_clear();
					vga_write_sync();
					vga_sync_bios_cursor();

					printf("WAV rejected\n");
					while (getch() != 13);
				}
				else if (buf == NULL) {
					fflush(stdin);
					vga_moveto(0,0);
					vga_clear();
					vga_write_sync();
					vga_sync_bios_cursor();
	
					printf("Failed to alloc GUS DRAM buffer\n");
					while (getch() != 13);
				}
				else {
					unsigned long offset=0,end,o,rem;
					struct vga_msg_box box;
					int channel=-1,rd;

					fflush(stdin);
					vga_moveto(0,0);
					vga_clear();
					vga_write_sync();
					vga_sync_bios_cursor();

					printf("%u-bits/sample %u-channel %luHz %lu bytes long\n",
						wav_bits,
						wav_channels,
						wav_sample_rate,
						wav_data_size);

					printf("Assign to GUS channel (1-32)? "); fflush(stdout);
					fgets(temp_str,sizeof(temp_str)-1,stdin);
					channel = isdigit(temp_str[0]) ? atoi(temp_str) : -1;

					if (channel >= 1 && channel <= 32) {
						channel--;
						select_voice = channel;

						printf("GUS RAM offset? "); fflush(stdout);
						fgets(temp_str,sizeof(temp_str)-1,stdin);
						offset = isdigit(temp_str[0]) ? strtoul(temp_str,NULL,0) : (~0UL);

						if (offset < (unsigned long)gus->total_ram) {
							end = offset + wav_data_size;
							if (end > gus->total_ram) end = gus->total_ram;

							printf("Stop at address (0x%lx)? ",(unsigned long)end);
							fgets(temp_str,sizeof(temp_str)-1,stdin);
							o = isdigit(temp_str[0]) ? strtoul(temp_str,NULL,0) : (~0UL);
							if (o > offset && o != (~0UL) && o <= gus->total_ram) end = o;

							printf("Will load into GUS RAM 0x%lx-0x%lx.\n",
								(unsigned long)offset,(unsigned long)end-1UL);
							if (gus->boundary256k && (offset & 0xFFFC0000UL) != ((end-1UL)&0xFFFC0000UL))
								printf("WARNING: Which also crosses a 256KKB boundary!\n");

							printf("Continue (y/n)? "); fflush(stdout);
							fgets(temp_str,sizeof(temp_str)-1,stdin);
							if (tolower(temp_str[0]) == 'y') {
								vga_msg_box_create(&box,"Uploading.......\n",0,0);

								ultrasnd_stop_voice(gus,channel);

								o = offset;
								rem = end - offset;
								lseek(wav_fd,wav_data_offset,SEEK_SET);
								while (rem > 0UL) {
									vga_moveto(box.x+2,box.y+2);
									vga_write_color(0x1F);
									sprintf(temp_str,"%%%02u %uKB/%uKB",
										(unsigned int)(((o-offset) * 100UL) / wav_data_size),
										(unsigned int)((o-offset) >> 10UL),
										(unsigned int)(((end-offset)+0x3FFUL) >> 10UL));
									vga_write(temp_str);

									if (kbhit()) {
										if (getch() == 27)
											break;
									}

									if (rem > (unsigned long)gus->dram_xfer_a->length)
										rd = gus->dram_xfer_a->length;
									else
										rd = (int)rem;

									_dos_xread(wav_fd,buf,rd);
									ultrasnd_send_dram_buffer(gus,o,rd,
										(wav_bits == 16 ? ULTRASND_DMA_DATA_SIZE_16BIT : ULTRASND_DMA_FLIP_MSB) |
										(gus->irq1 >= 0 ? ULTRASND_DMA_TC_IRQ : 0));
									rem -= (unsigned long)rd;
									o += (unsigned long)rd;
								}

								vga_msg_box_destroy(&box);

								ultrasnd_set_voice_mode(gus,channel,
									(wav_bits == 16 ? ULTRASND_VOICE_MODE_16BIT : 0) |
									ULTRASND_VOICE_MODE_STOP | ULTRASND_VOICE_MODE_IS_STOPPED);
								ultrasnd_set_voice_fc(gus,channel,
									ultrasnd_sample_rate_to_fc(gus,(unsigned long)wav_sample_rate * (unsigned long)wav_channels));
								ultrasnd_set_voice_ramp_rate(gus,channel,0,0);
								ultrasnd_set_voice_ramp_start(gus,channel,0xF0); /* NTS: You have to set the ramp start/end because it will override your current volume */
								ultrasnd_set_voice_ramp_end(gus,channel,0xF0);
								ultrasnd_set_voice_volume(gus,channel,0xFFF0); /* full vol */
								ultrasnd_set_voice_pan(gus,channel,7);
								ultrasnd_set_voice_ramp_control(gus,channel,0);
								ultrasnd_set_voice_start(gus,channel,offset);
								ultrasnd_set_voice_end(gus,channel,end-(wav_bits == 16 ? 2 : 1));
								ultrasnd_set_voice_current(gus,channel,offset);
								ultrasnd_set_voice_mode(gus,channel,
									(wav_bits == 16 ? ULTRASND_VOICE_MODE_16BIT : 0) |
									(gus->irq1 >= 0 ? ULTRASND_VOICE_MODE_IRQ : 0) |
									ULTRASND_VOICE_MODE_STOP | ULTRASND_VOICE_MODE_IS_STOPPED);
							}
						}
					}
				}
				close_wav();
			}
			else if (mitem == &main_menu_hardware_reset) {
				struct vga_msg_box box;
				vga_msg_box_create(&box,"Resetting...",0,0);

				_cli();
				ultrasnd_abort_dma_transfer(gus);
				ultrasnd_stop_all_voices(gus);
				ultrasnd_stop_timers(gus);
				ultrasnd_drain_irq_events(gus);

				ultrasnd_select_write(gus,0x4C,0x00); /* 0x4C: reset register -> master reset (bit 0=0) disable DAC (bit 1=0) master IRQ disable (bit 2=0) */
				t8254_wait(t8254_us2ticks(20000)); /* wait 20ms for good measure, most docs imply 14 CPU cycles via IN statements but I don't think that's enough */
				ultrasnd_select_write(gus,0x4C,0x01);
				ultrasnd_select_write(gus,0x4C,0x03);
				if (gus->irq1 >= 0) ultrasnd_select_write(gus,0x4C,0x07);

				ultrasnd_abort_dma_transfer(gus);
				ultrasnd_stop_all_voices(gus);
				ultrasnd_stop_timers(gus);
				ultrasnd_drain_irq_events(gus);

				t8254_wait(t8254_us2ticks(500000));
				_sti();

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

