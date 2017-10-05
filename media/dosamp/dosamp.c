
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <malloc.h>
#include <direct.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/vga/vga.h>
#include <hw/dos/dos.h>
#include <hw/8237/8237.h>		/* 8237 DMA */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/sndsb/sb16asp.h>
#include <hw/vga/vgagui.h>
#include <hw/vga/vgatty.h>
#include <hw/dos/doswin.h>
#include <hw/dos/tgusmega.h>
#include <hw/dos/tgussbos.h>
#include <hw/dos/tgusumid.h>

static struct dma_8237_allocation *sb_dma = NULL; /* DMA buffer */

static struct sndsb_ctx*	sb_card = NULL;

/*============================TODO: move to library=============================*/
static int vector_is_iret(const unsigned char vector) {
	const unsigned char far *p;
	uint32_t rvector;

#if TARGET_MSDOS == 32
	rvector = ((uint32_t*)0)[vector];
	if (rvector == 0) return 0;
	p = (const unsigned char*)(((rvector >> 16UL) << 4UL) + (rvector & 0xFFFFUL));
#else
	rvector = *((uint32_t far*)MK_FP(0,(vector*4)));
	if (rvector == 0) return 0;
	p = (const unsigned char far*)MK_FP(rvector>>16UL,rvector&0xFFFFUL);
#endif

	if (*p == 0xCF) {
		// IRET. Yep.
		return 1;
	}
	else if (p[0] == 0xFE && p[1] == 0x38) {
		// DOSBox callback. Probably not going to ACK the interrupt.
		return 1;
	}

	return 0;
}
/*==============================================================================*/

static int			wav_fd = -1;
static char			wav_file[130] = {0};
static unsigned char		wav_stereo = 0,wav_16bit = 0,wav_bytes_per_sample = 1;
static unsigned long		wav_data_offset = 44,wav_data_length = 0,wav_sample_rate = 8000,wav_position = 0,wav_buffer_filepos = 0;
static unsigned char		wav_playing = 0;

static void stop_play();

/* WARNING!!! This interrupt handler calls subroutines. To avoid system
 * instability in the event the IRQ fires while, say, executing a routine
 * in the DOS kernel, you must compile this code with the -zu flag in
 * 16-bit real mode Large and Compact memory models! Without -zu, minor
 * memory corruption in the DOS kernel will result and the system will
 * hang and/or crash. */
unsigned char old_irq_masked = 0;
static void (interrupt *old_irq)() = NULL;
static void interrupt sb_irq() {
	unsigned char c;

	sb_card->irq_counter++;

	/* ack soundblaster DSP if DSP was the cause of the interrupt */
	/* NTS: Experience says if you ack the wrong event on DSP 4.xx it
	   will just re-fire the IRQ until you ack it correctly...
	   or until your program crashes from stack overflow, whichever
	   comes first */
	c = sndsb_interrupt_reason(sb_card);
	sndsb_interrupt_ack(sb_card,c);

	/* FIXME: The sndsb library should NOT do anything in
	   send_buffer_again() if it knows playback has not started! */
	/* for non-auto-init modes, start another buffer */
	if (wav_playing) sndsb_irq_continue(sb_card,c);

	/* NTS: we assume that if the IRQ was masked when we took it, that we must not
	 *      chain to the previous IRQ handler. This is very important considering
	 *      that on most DOS systems an IRQ is masked for a very good reason---the
	 *      interrupt handler doesn't exist! In fact, the IRQ vector could easily
	 *      be unitialized or 0000:0000 for it! CALLing to that address is obviously
	 *      not advised! */
	if (old_irq_masked || old_irq == NULL) {
		/* ack the interrupt ourself, do not chain */
		if (sb_card->irq >= 8) p8259_OCW2(8,P8259_OCW2_NON_SPECIFIC_EOI);
		p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
	}
	else {
		/* chain to the previous IRQ, who will acknowledge the interrupt */
		old_irq();
	}
}

static void load_audio(struct sndsb_ctx *cx,uint32_t up_to,uint32_t min,uint32_t max,uint8_t initial) { /* load audio up to point or max */
	unsigned char FAR *buffer = sb_dma->lin;
	VGA_ALPHA_PTR wr = vga_state.vga_alpha_ram + 80 - 6;
	unsigned char load=0;
	uint16_t prev[6];
	int rd,i,bufe=0;
	uint32_t how;

	/* caller should be rounding! */
	assert((up_to & 3UL) == 0UL);
	if (up_to >= cx->buffer_size) return;
	if (cx->buffer_size < 32) return;
	if (cx->buffer_last_io == up_to) return;

	if (sb_card->dsp_adpcm > 0 && (wav_16bit || wav_stereo)) return;
	if (max == 0) max = cx->buffer_size/4;
	if (max < 16) return;
	lseek(wav_fd,wav_data_offset + (wav_position * (unsigned long)wav_bytes_per_sample),SEEK_SET);

	if (cx->buffer_last_io == 0)
		wav_buffer_filepos = wav_position;

	while (max > 0UL) {
		if (cx->backwards) {
			if (up_to > cx->buffer_last_io) {
				how = cx->buffer_last_io;
				if (how == 0) how = cx->buffer_size - up_to;
				bufe = 1;
			}
			else {
				how = (cx->buffer_last_io - up_to);
				bufe = 0;
			}
		}
		else {
			if (up_to < cx->buffer_last_io) {
				how = (cx->buffer_size - cx->buffer_last_io); /* from last IO to end of buffer */
				bufe = 1;
			}
			else {
				how = (up_to - cx->buffer_last_io); /* from last IO to up_to */
				bufe = 0;
			}
		}

		if (how > 16384UL)
			how = 16384UL;

		if (how == 0UL)
			break;
		else if (how > max)
			how = max;
		else if (!bufe && how < min)
			break;

		if (!load) {
			load = 1;
			prev[0] = wr[0];
			wr[0] = '[' | 0x0400;
			prev[1] = wr[1];
			wr[1] = 'L' | 0x0400;
			prev[2] = wr[2];
			wr[2] = 'O' | 0x0400;
			prev[3] = wr[3];
			wr[3] = 'A' | 0x0400;
			prev[4] = wr[4];
			wr[4] = 'D' | 0x0400;
			prev[5] = wr[5];
			wr[5] = ']' | 0x0400;
		}

		if (cx->buffer_last_io == 0)
			wav_buffer_filepos = wav_position;

        {
            uint32_t oa,adj;

			oa = cx->buffer_last_io;
			if (cx->backwards) {
				if (cx->buffer_last_io == 0) {
					cx->buffer_last_io = cx->buffer_size - how;
				}
				else if (cx->buffer_last_io >= how) {
					cx->buffer_last_io -= how;
				}
				else {
					abort();
				}

				adj = (uint32_t)how / wav_bytes_per_sample;
				if (wav_position >= adj) wav_position -= adj;
				else if (wav_position != 0UL) wav_position = 0;
				else {
					wav_position = lseek(wav_fd,0,SEEK_END);
					if (wav_position >= adj) wav_position -= adj;
					else if (wav_position != 0UL) wav_position = 0;
					wav_position /= wav_bytes_per_sample;
				}

				lseek(wav_fd,wav_data_offset + (wav_position * (unsigned long)wav_bytes_per_sample),SEEK_SET);
			}

			assert(cx->buffer_last_io <= cx->buffer_size);
#if TARGET_MSDOS == 32
			rd = _dos_xread(wav_fd,buffer + cx->buffer_last_io,how);
#else
            {
                uint32_t o;

                o  = (uint32_t)FP_SEG(buffer) << 4UL;
                o += (uint32_t)FP_OFF(buffer);
                o += cx->buffer_last_io;
                rd = _dos_xread(wav_fd,MK_FP(o >> 4UL,o & 0xFUL),how);
            }
#endif
			if (rd == 0 || rd == -1) {
				if (!cx->backwards) {
					wav_position = 0;
					lseek(wav_fd,wav_data_offset + (wav_position * (unsigned long)wav_bytes_per_sample),SEEK_SET);
					rd = _dos_xread(wav_fd,buffer + cx->buffer_last_io,how);
					if (rd == 0 || rd == -1) {
						/* hmph, fine */
#if TARGET_MSDOS == 32
						memset(buffer+cx->buffer_last_io,128,how);
#else
						_fmemset(buffer+cx->buffer_last_io,128,how);
#endif
						rd = (int)how;
					}
				}
				else {
					rd = (int)how;
				}
			}

			assert((cx->buffer_last_io+((uint32_t)rd)) <= cx->buffer_size);
			if (sb_card->audio_data_flipped_sign) {
				if (wav_16bit)
					for (i=0;i < (rd-1);i += 2) buffer[cx->buffer_last_io+i+1] ^= 0x80;
				else
					for (i=0;i < rd;i++) buffer[cx->buffer_last_io+i] ^= 0x80;
			}

			if (!cx->backwards) {
				cx->buffer_last_io += (uint32_t)rd;
				wav_position += (uint32_t)rd / wav_bytes_per_sample;
			}
		}

		assert(cx->buffer_last_io <= cx->buffer_size);
		if (!cx->backwards) {
			if (cx->buffer_last_io == cx->buffer_size) cx->buffer_last_io = 0;
		}
		max -= (uint32_t)rd;
	}

	if (cx->buffer_last_io == 0)
		wav_buffer_filepos = wav_position;

	if (load) {
		for (i=0;i < 6;i++)
			wr[i] = prev[i];
	}
}

#define DMA_WRAP_DEBUG

static void wav_idle() {
	const unsigned int leeway = sb_card->buffer_size / 100;
	uint32_t pos;
#ifdef DMA_WRAP_DEBUG
	uint32_t pos2;
#endif

	if (!wav_playing || wav_fd < 0)
		return;

	/* if we're playing without an IRQ handler, then we'll want this function
	 * to poll the sound card's IRQ status and handle it directly so playback
	 * continues to work. if we don't, playback will halt on actual Creative
	 * Sound Blaster 16 hardware until it gets the I/O read to ack the IRQ */
	sndsb_main_idle(sb_card);

	_cli();
#ifdef DMA_WRAP_DEBUG
	pos2 = sndsb_read_dma_buffer_position(sb_card);
#endif
	pos = sndsb_read_dma_buffer_position(sb_card);
#ifdef DMA_WRAP_DEBUG
	if (sb_card->backwards) {
		if (pos2 < 0x1000 && pos >= (sb_card->buffer_size-0x1000)) {
			/* normal DMA wrap-around, no problem */
		}
		else {
			if (pos > pos2)	fprintf(stderr,"DMA glitch! 0x%04lx 0x%04lx\n",pos,pos2);
			else		pos = max(pos,pos2);
		}

		pos += leeway;
		if (pos >= sb_card->buffer_size) pos -= sb_card->buffer_size;
	}
	else {
		if (pos < 0x1000 && pos2 >= (sb_card->buffer_size-0x1000)) {
			/* normal DMA wrap-around, no problem */
		}
		else {
			if (pos < pos2)	fprintf(stderr,"DMA glitch! 0x%04lx 0x%04lx\n",pos,pos2);
			else		pos = min(pos,pos2);
		}

		if (pos < leeway) pos += sb_card->buffer_size - leeway;
		else pos -= leeway;
	}
#endif
	pos &= (~3UL); /* round down */
	_sti();

    /* load from disk */
    load_audio(sb_card,pos,min(wav_sample_rate/8,4096)/*min*/,
        sb_card->buffer_size/4/*max*/,0/*first block*/);
}

static void update_cfg();

static unsigned long playback_live_position() {
	signed long xx = (signed long)sndsb_read_dma_buffer_position(sb_card);

	if (sb_card->backwards) {
		if (sb_card->buffer_last_io >= (unsigned long)xx)
			xx += sb_card->buffer_size;

		xx -= sb_card->buffer_size; /* because we started from the end */
	}
	else {
		if (sb_card->buffer_last_io <= (unsigned long)xx)
			xx -= sb_card->buffer_size;
	}

	if (sb_card->dsp_adpcm == ADPCM_4BIT) xx *= 2;
	else if (sb_card->dsp_adpcm == ADPCM_2_6BIT) xx *= 3;
	else if (sb_card->dsp_adpcm == ADPCM_2BIT) xx *= 4;
	xx += wav_buffer_filepos * wav_bytes_per_sample;
	if (xx < 0) xx += wav_data_length;
	return ((unsigned long)xx) / wav_bytes_per_sample;
}

static void close_wav() {
	if (wav_fd >= 0) {
		close(wav_fd);
		wav_fd = -1;
	}
}

static void open_wav() {
	char tmp[64];

	wav_position = 0;
	if (wav_fd < 0) {
		if (strlen(wav_file) < 1) return;
		wav_fd = open(wav_file,O_RDONLY|O_BINARY);
		if (wav_fd < 0) return;
		wav_data_offset = 0;
		wav_data_length = (unsigned long)lseek(wav_fd,0,SEEK_END);
		lseek(wav_fd,0,SEEK_SET);
		read(wav_fd,tmp,sizeof(tmp));

		/* FIXME: This is a dumb quick and dirty WAVE header reader */
		if (!memcmp(tmp,"RIFF",4) && !memcmp(tmp+8,"WAVEfmt ",8) && wav_data_length > 44) {
			unsigned char *fmtc = tmp + 20;
			/* fmt chunk at 12, where 'fmt '@12 and length of fmt @ 16, fmt data @ 20, 16 bytes long */
			/* WORD    wFormatTag
			 * WORD    nChannels
			 * DWORD   nSamplesPerSec
			 * DWORD   nAvgBytesPerSec
			 * WORD    nBlockAlign
			 * WORD    wBitsPerSample */
			wav_sample_rate = *((uint32_t*)(fmtc + 4));
            if (wav_sample_rate == 0UL) wav_sample_rate = 1UL;
			wav_stereo = *((uint16_t*)(fmtc + 2)) > 1;
			wav_16bit = *((uint16_t*)(fmtc + 14)) > 8;
			wav_bytes_per_sample = (wav_stereo ? 2 : 1) * (wav_16bit ? 2 : 1);
			wav_data_offset = 44;
			wav_data_length -= 44;
			wav_data_length -= wav_data_length % wav_bytes_per_sample;
		}
	}
}

static void free_dma_buffer() {
    if (sb_dma != NULL) {
        dma_8237_free_buffer(sb_dma);
        sb_dma = NULL;
    }
}

static void realloc_dma_buffer() {
    uint32_t choice;
    int8_t ch;

    free_dma_buffer();

    ch = sndsb_dsp_playback_will_use_dma_channel(sb_card,wav_sample_rate,wav_stereo,wav_16bit);

    if (ch >= 4)
        choice = sndsb_recommended_16bit_dma_buffer_size(sb_card,0);
    else
        choice = sndsb_recommended_dma_buffer_size(sb_card,0);

    do {
        if (ch >= 4)
            sb_dma = dma_8237_alloc_buffer_dw(choice,16);
        else
            sb_dma = dma_8237_alloc_buffer_dw(choice,8);

        if (sb_dma == NULL) choice -= 4096UL;
    } while (sb_dma == NULL && choice > 4096UL);

    if (!sndsb_assign_dma_buffer(sb_card,sb_dma))
        return;
    if (sb_dma == NULL)
        return;
}

static void begin_play() {
	unsigned long choice_rate;

	if (wav_playing)
		return;

	if (sb_card->dsp_play_method == SNDSB_DSPOUTMETHOD_DIRECT)
        return;

    if (sb_dma == NULL)
        realloc_dma_buffer();

    {
        int8_t ch = sndsb_dsp_playback_will_use_dma_channel(sb_card,wav_sample_rate,wav_stereo,wav_16bit);
        if (ch >= 0) {
            if (sb_dma->dma_width != (ch >= 4 ? 16 : 8))
                realloc_dma_buffer();
            if (sb_dma == NULL)
                return;
        }
    }

    if (sb_dma != NULL) {
        if (!sndsb_assign_dma_buffer(sb_card,sb_dma))
            return;
    }

	if (wav_fd < 0)
		return;

	choice_rate = wav_sample_rate;

	update_cfg();
	if (!sndsb_prepare_dsp_playback(sb_card,choice_rate,wav_stereo,wav_16bit))
		return;

	sndsb_setup_dma(sb_card);

    load_audio(sb_card,sb_card->buffer_size/2,0/*min*/,0/*max*/,1/*first block*/);

	/* make sure the IRQ is acked */
	if (sb_card->irq >= 8) {
		p8259_OCW2(8,P8259_OCW2_SPECIFIC_EOI | (sb_card->irq & 7));
		p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | 2);
	}
	else if (sb_card->irq >= 0) {
		p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | sb_card->irq);
	}
	if (sb_card->irq >= 0)
		p8259_unmask(sb_card->irq);

	if (!sndsb_begin_dsp_playback(sb_card))
		return;

	_cli();
	wav_playing = 1;
	_sti();
}

static void stop_play() {
	if (!wav_playing) return;

    wav_position = playback_live_position();
    wav_position -= wav_position % (unsigned long)wav_bytes_per_sample;

	_cli();
	sndsb_stop_dsp_playback(sb_card);
	wav_playing = 0;
	_sti();
}

static const struct vga_menu_item main_menu_file_set =
	{"Set file...",		's',	0,	0};
static const struct vga_menu_item main_menu_file_quit =
	{"Quit",		'q',	0,	0};

static const struct vga_menu_item* main_menu_file[] = {
	&main_menu_file_set,
	&main_menu_file_quit,
	NULL
};

static const struct vga_menu_bar_item main_menu_bar[] = {
    /* name                 key     scan    x       w       id */
    {" File ",              'F',    0x21,   0,  6,  &main_menu_file}, /* ALT-F */
    {NULL,                  0,      0x00,   0,  0,  0}
};

static void my_vga_menu_idle() {
    wav_idle();
}

static int confirm_quit() {
	/* FIXME: Why does this cause Direct DSP playback to horrifically slow down? */
	return confirm_yes_no_dialog("Are you sure you want to exit to DOS?");
}

static void update_cfg() {
	unsigned int r;

	sb_card->dsp_adpcm = sb_card->dsp_adpcm;
	r = wav_sample_rate;
	if (sb_card->dsp_adpcm == ADPCM_4BIT) r /= 2;
	else if (sb_card->dsp_adpcm == ADPCM_2_6BIT) r /= 3;
	else if (sb_card->dsp_adpcm == ADPCM_2BIT) r /= 4;

    sb_card->buffer_irq_interval =
        sb_card->buffer_size / wav_bytes_per_sample;
}

static void prompt_play_wav(unsigned char rec) {
	unsigned char gredraw = 1;
#if TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__COMPACT__) || defined(__SMALL__))
#else
	struct find_t ft;
#endif

	{
		const char *rp;
		char temp[sizeof(wav_file)];
		int cursor = strlen(wav_file),i,c,redraw=1,ok=0;
		memcpy(temp,wav_file,strlen(wav_file)+1);
		while (!ok) {
			if (gredraw) {
#if TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__COMPACT__) || defined(__SMALL__))
#else
				char *cwd;
#endif

				gredraw = 0;
				vga_clear();
				vga_moveto(0,4);
				vga_write_color(0x07);
				vga_write("Enter WAV file path:\n");
				vga_write_sync();
				redraw = 1;

#if TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__COMPACT__) || defined(__SMALL__))
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
						if ((x+cw) > vga_state.vga_width) {
							x = 0;
							if (y >= vga_state.vga_height) break;
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
#if TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__COMPACT__) || defined(__SMALL__))
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
		}

		if (ok == 1) {
			unsigned char wp = wav_playing;
			stop_play();
			close_wav();
			memcpy(wav_file,temp,strlen(temp)+1);
			open_wav();
			if (wp) begin_play();
		}
	}
}

static void help() {
#if TARGET_MSDOS == 16 && defined(__TINY__)
    printf("See source code for options.");
#else
	printf("test [options]\n");
# if !(TARGET_MSDOS == 16 && defined(__COMPACT__)) /* this is too much to cram into a small model EXE */
	printf(" /h /help             This help\n");
# endif
#endif
}

int main(int argc,char **argv) {
	int i,loop,redraw,bkgndredraw,cc;
	const struct vga_menu_item *mitem = NULL;
	VGA_ALPHA_PTR vga;

	printf("Sound Blaster test program\n");
	for (i=1;i < argc;) {
		char *a = argv[i++];

		if (*a == '-' || *a == '/') {
			unsigned char m = *a++;
			while (*a == m) a++;

			if (!strcmp(a,"h") || !strcmp(a,"help")) {
				help();
				return 1;
			}
			else {
				help();
				return 1;
			}
		}
	}

	if (!probe_vga()) {
		printf("Cannot init VGA\n");
		return 1;
	}

	if (!probe_8237())
		printf("WARNING: Cannot init 8237 DMA\n");

	if (!probe_8259()) {
		printf("Cannot init 8259 PIC\n");
		return 1;
	}
	if (!probe_8254()) {
		printf("Cannot init 8254 timer\n");
		return 1;
	}
	if (!init_sndsb()) {
		printf("Cannot init library\n");
		return 1;
	}

    /* we want to know if certain emulation TSRs exist */
    gravis_mega_em_detect(&megaem_info);
    gravis_sbos_detect();

	/* it's up to us now to tell it certain minor things */
	sndsb_detect_virtualbox();		// whether or not we're running in VirtualBox
	/* sndsb now allows us to keep the EXE small by not referring to extra sound card support */
	sndsb_enable_sb16_support();		// SB16 support
	sndsb_enable_sc400_support();		// SC400 support
	sndsb_enable_ess_audiodrive_support();	// ESS AudioDrive support

    /* Non-plug & play scan */
	if (sndsb_try_blaster_var() != NULL) {
		if (!sndsb_init_card(sndsb_card_blaster))
			sndsb_free_card(sndsb_card_blaster);
	}

    /* Most SB cards exist at 220h or 240h */
    sndsb_try_base(0x220);
    sndsb_try_base(0x240);

    /* now let the user choose */
	for (i=0;i < SNDSB_MAX_CARDS;i++) {
		struct sndsb_ctx *cx = sndsb_index_to_ctx(i);
		if (cx->baseio == 0) continue;

		if (cx->irq < 0)
			sndsb_probe_irq_F2(cx);
		if (cx->irq < 0)
			sndsb_probe_irq_80(cx);
		if (cx->dma8 < 0)
			sndsb_probe_dma8_E2(cx);
		if (cx->dma8 < 0)
			sndsb_probe_dma8_14(cx);

		// having IRQ and DMA changes the ideal playback method and capabilities
		sndsb_update_capabilities(cx);
		sndsb_determine_ideal_dsp_play_method(cx);
	}

	{
		unsigned char count = 0;
        int sc_idx = -1;

		for (i=0;i < SNDSB_MAX_CARDS;i++) {
			struct sndsb_ctx *cx = sndsb_index_to_ctx(i);
			if (cx->baseio == 0) continue;

			printf("  [%u] base=%X dma=%d dma16=%d irq=%d DSPv=%u.%u\n",
					i+1,cx->baseio,cx->dma8,cx->dma16,cx->irq,(unsigned int)cx->dsp_vmaj,(unsigned int)cx->dsp_vmin);

			count++;
		}

		if (count == 0) {
			printf("No cards found.\n");
			return 1;
		}

        printf("-----------\n");
		printf("Which card?: "); fflush(stdout);

		i = getch();
		printf("\n");
		if (i == 27) return 0;
		if (i == 13 || i == 10) i = '1';
		sc_idx = i - '0';

        if (sc_idx < 1 || sc_idx > SNDSB_MAX_CARDS) {
            printf("Sound card index out of range\n");
            return 1;
        }

        sb_card = &sndsb_card[sc_idx-1];
        if (sb_card->baseio == 0)
            return 1;
    }

    realloc_dma_buffer();

	if (!sndsb_assign_dma_buffer(sb_card,sb_dma)) {
		printf("Cannot assign DMA buffer\n");
		return 1;
	}

	i = int10_getmode();
	if (i != 3) int10_setmode(3);

	if (sb_card->irq != -1) {
		old_irq_masked = p8259_is_masked(sb_card->irq);
		if (vector_is_iret(irq2int(sb_card->irq)))
			old_irq_masked = 1;

		old_irq = _dos_getvect(irq2int(sb_card->irq));
		_dos_setvect(irq2int(sb_card->irq),sb_irq);
		p8259_unmask(sb_card->irq);
	}
	
	vga_write_color(0x07);
	vga_clear();

	loop=1;
	redraw=1;
	bkgndredraw=1;
	vga_menu_bar.bar = main_menu_bar;
	vga_menu_bar.sel = -1;
	vga_menu_bar.row = 0;
	vga_menu_idle = my_vga_menu_idle;
	update_cfg();

	while (loop) {
        wav_idle();

		if ((mitem = vga_menu_bar_keymon()) != NULL) {
			/* act on it */
			if (mitem == &main_menu_file_quit) {
				if (confirm_quit()) {
					loop = 0;
					break;
				}
			}
			else if (mitem == &main_menu_file_set) {
				prompt_play_wav(0);
				bkgndredraw = 1;
				redraw = 1;
			}
		}

		if (redraw || bkgndredraw) {
			if (bkgndredraw) {
				for (vga=vga_state.vga_alpha_ram+(80*1),cc=0;cc < (80*23);cc++) *vga++ = 0x1E00 | 177;
				vga_menu_bar_draw();
			}

			bkgndredraw = 0;
			redraw = 0;
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
			else if (i == ' ') {
                if (wav_playing) stop_play();
                else begin_play();
            }
		}
	}

	_sti();
	vga_write_sync();
	stop_play();
	close_wav();
    free_dma_buffer();

	if (sb_card->irq >= 0 && old_irq_masked)
		p8259_mask(sb_card->irq);

	if (sb_card->irq != -1)
		_dos_setvect(irq2int(sb_card->irq),old_irq);

	sndsb_free_card(sb_card);
	free_sndsb(); /* will also de-ref/unhook the NMI reflection */
	return 0;
}

