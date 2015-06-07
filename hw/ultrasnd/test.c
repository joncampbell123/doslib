
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

int main(int argc,char **argv) {
	struct ultrasnd_ctx *u;
	unsigned long ofs,ptr;
	unsigned char FAR *dmaptr;
	int i,j,fd,rd;
	int no_dma=0;
	int die=0;

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
		char *s = argv[i];

		if (!strcmp(s,"-d"))
			ultrasnd_debug(1);
		else if (!strcmp(s,"-nodma"))
			no_dma = 1;
	}

	if (ultrasnd_try_ultrasnd_env() != NULL) {
		printf("ULTRASND environment variable: PORT=0x%03x IRQ=%d,%d DMA=%d,%d\n",
			ultrasnd_env->port,
			ultrasnd_env->irq1,
			ultrasnd_env->irq2,
			ultrasnd_env->dma1,
			ultrasnd_env->dma2);
		if (ultrasnd_probe(ultrasnd_env,0)) {
			printf("Never mind, doesn't work\n");
			ultrasnd_free_card(ultrasnd_env);
		}
	}

	if (windows_mode == WINDOWS_NONE) {
		/* FIXME: Windows XP DOS Box: This probing function causes the (virtual) keyboard to stop responding. Why? */
		if (ultrasnd_try_base(0x220))
			printf("Also found one at 0x220\n");
		if (ultrasnd_try_base(0x240))
			printf("Also found one at 0x240\n");
	}

	for (i=0;i < MAX_ULTRASND;i++) {
		u = &ultrasnd_card[i];
		if (ultrasnd_card_taken(u)) {
			printf("[%u] RAM=%dKB PORT=0x%03x IRQ=%d,%d DMA=%d,%d 256KB-boundary=%u voices=%u/%uHz\n",
				i+1,
				(int)(u->total_ram >> 10UL),
				u->port,
				u->irq1,
				u->irq2,
				u->dma1,
				u->dma2,
				u->boundary256k,
				u->active_voices,
				u->output_rate);
		}
	}
	printf("Which card to use? "); fflush(stdout);
	i = -1; scanf("%d",&i); i--;
	if (i < 0 || i >= MAX_ULTRASND || !ultrasnd_card_taken(&ultrasnd_card[i])) return 1;
	u = &ultrasnd_card[i];

	if (no_dma) {
		printf("Disabling DMA at your request\n");
		u->use_dma = 0;
	}

	/* ------------------------------------------------------------------------------------------------------- */
	if (!die) {
		printf("8-bit 11KHz test\n");

		/* load something into RAM */
		fd = open("8_11khz.wav",O_RDONLY|O_BINARY);
		if (fd < 0) fd = open("..\\..\\8_11khz.wav",O_RDONLY|O_BINARY);
		if (fd < 0) fd = open("..\\..\\..\\8_11khz.wav",O_RDONLY|O_BINARY);
		if (fd < 0) {
			fprintf(stderr,"Cannot open .WAV\n");
			return 1;
		}
		lseek(fd,44,SEEK_SET);
		ofs=0;
		printf("Loading into DRAM "); fflush(stdout);
		if ((dmaptr = ultrasnd_dram_buffer_alloc(u,4096)) != NULL) {
		    printf("phys=0x%08lX ",(unsigned long)(u->dram_xfer_a->phys));
		    while ((rd = _dos_xread(fd,dmaptr,4096)) > 0) {
			ultrasnd_send_dram_buffer(u,ofs,rd,ULTRASND_DMA_FLIP_MSB);
			printf("."); fflush(stdout);
			ofs += rd;
		    }
		    printf("\n");
		    /* and then repeat the last sample to avoid click */
		    ultrasnd_poke(u,ofs,ultrasnd_peek(u,0));
		    ultrasnd_dram_buffer_free(u);
		}
		else {
		    printf("ERROR: Cannot alloc DMA transfer DRAM upload\n");
		}
		close(fd);

		/* get the audio going */
		ultrasnd_stop_voice(u,0);
		ultrasnd_set_voice_mode(u,0,ULTRASND_VOICE_MODE_LOOP | ULTRASND_VOICE_MODE_BIDIR);
		ultrasnd_set_voice_fc(u,0,ultrasnd_sample_rate_to_fc(u,11025));
		ultrasnd_set_voice_ramp_rate(u,0,0,0);
		ultrasnd_set_voice_ramp_start(u,0,0xF0); /* NTS: You have to set the ramp start/end because it will override your current volume */
		ultrasnd_set_voice_ramp_end(u,0,0xF0);
		ultrasnd_set_voice_volume(u,0,0xFFF0); /* full vol */
		ultrasnd_set_voice_pan(u,0,7); /* center (FIXME: Despite the code being present, DOSBox's GUS emulation fails to emulate the pan register and everything is rendered at center) */
		ultrasnd_set_voice_ramp_control(u,0,0);
		ultrasnd_set_voice_current(u,0,1);
		ultrasnd_set_voice_start(u,0,1); /* NTS: If 0, the card won't stop and will run into 0xFFFFF and beyond */
		ultrasnd_set_voice_end(u,0,ofs-1);
		ultrasnd_set_voice_mode(u,0,ULTRASND_VOICE_MODE_LOOP | ULTRASND_VOICE_MODE_BIDIR);
		ultrasnd_start_voice(u,0);

		/* wait */
		do {
			ptr = ultrasnd_read_voice_current(u,0);
			fprintf(stderr,"\x0D" "ESC to quit, ENTER to move on. 0x%08lx ",ptr); fflush(stderr);

			if (kbhit()) {
				i = getch();
				if (i == 27) {
					die = 1;
					break;
				}
				else if (i == 13) {
					break;
				}
				else if (i == 8) {
					ultrasnd_set_voice_mode(u,0,ultrasnd_read_voice_mode(u,0) ^ ULTRASND_VOICE_MODE_BACKWARDS);
				}
				else if (i == 32) {
					ultrasnd_set_voice_current(u,0,0);
					ultrasnd_set_voice_current(u,0,0);
					ultrasnd_set_voice_mode(u,0,ULTRASND_VOICE_MODE_LOOP | ULTRASND_VOICE_MODE_BIDIR);
				}
				else if (i == 'r' || i == 'R') {
					ultrasnd_set_voice_pan(u,0,0); /* center (FIXME: Despite the code being present, DOSBox's GUS emulation fails to emulate the pan register and everything is rendered at center) */
				}
				else if (i == 'l' || i == 'L') {
					ultrasnd_set_voice_pan(u,0,0xF); /* center (FIXME: Despite the code being present, DOSBox's GUS emulation fails to emulate the pan register and everything is rendered at center) */
				}
				else if (i == 'c' || i == 'C') {
					ultrasnd_set_voice_pan(u,0,0x7); /* center (FIXME: Despite the code being present, DOSBox's GUS emulation fails to emulate the pan register and everything is rendered at center) */
				}
			}
		} while(1);
		fprintf(stderr,"\n");

		/* shut up the GUS */
		for (i=0;i < u->active_voices;i++) {
			ultrasnd_stop_voice(u,i);
			for (j=0;j < 14;j++)
				ultrasnd_select_write16(u,j,0);
		}
	}

	/* ------------------------------------------------------------------------------------------------------- */
	if (!die) {
		printf("16-bit 44.1KHz test\n");

		/* load something into RAM */
		fd = open("16_44khz.wav",O_RDONLY|O_BINARY);
		if (fd < 0) fd = open("..\\..\\16_44khz.wav",O_RDONLY|O_BINARY);
		if (fd < 0) fd = open("..\\..\\..\\16_44khz.wav",O_RDONLY|O_BINARY);
		if (fd < 0) {
			fprintf(stderr,"Cannot open .WAV\n");
			return 1;
		}
		lseek(fd,44,SEEK_SET);
		ofs=0;
		printf("Loading into DRAM "); fflush(stdout);
		if ((dmaptr = ultrasnd_dram_buffer_alloc(u,4096)) != NULL) {
		    printf("phys=0x%08lX ",(unsigned long)(u->dram_xfer_a->phys));
		    while ((rd = _dos_xread(fd,dmaptr,4096)) > 0) {
			ultrasnd_send_dram_buffer(u,ofs,rd,ULTRASND_DMA_DATA_SIZE_16BIT);
			printf("."); fflush(stdout);
			ofs += rd;
		    }
		    printf("\n");
		    /* and then repeat the last sample to avoid click */
		    ultrasnd_poke(u,ofs,ultrasnd_peek(u,0));
		    ultrasnd_poke(u,ofs+1,ultrasnd_peek(u,1));
		    ultrasnd_dram_buffer_free(u);
		}
		else {
		    printf("ERROR: Cannot alloc DMA transfer DRAM upload\n");
		}
		/* and then repeat the last sample to avoid click */
		close(fd);

		/* get the audio going */
		ultrasnd_stop_voice(u,0);
		ultrasnd_set_voice_mode(u,0,ULTRASND_VOICE_MODE_LOOP | ULTRASND_VOICE_MODE_BIDIR | ULTRASND_VOICE_MODE_16BIT);
		ultrasnd_set_voice_fc(u,0,ultrasnd_sample_rate_to_fc(u,44100));
		ultrasnd_set_voice_ramp_rate(u,0,0,0);
		ultrasnd_set_voice_ramp_start(u,0,0xF0); /* NTS: You have to set the ramp start/end because it will override your current volume */
		ultrasnd_set_voice_ramp_end(u,0,0xF0);
		ultrasnd_set_voice_volume(u,0,0xFFF0); /* full vol */
		ultrasnd_set_voice_pan(u,0,7); /* center (FIXME: Despite the code being present, DOSBox's GUS emulation fails to emulate the pan register and everything is rendered at center) */
		ultrasnd_set_voice_ramp_control(u,0,0);
		ultrasnd_set_voice_start(u,0,2); /* NTS: If 0 or 1, the card won't stop and will run into 0xFFFFF and beyond */
		ultrasnd_set_voice_end(u,0,ofs-1);
		ultrasnd_set_voice_current(u,0,2); /* Start well within the range */
		ultrasnd_set_voice_mode(u,0,ULTRASND_VOICE_MODE_LOOP | ULTRASND_VOICE_MODE_BIDIR | ULTRASND_VOICE_MODE_16BIT);
		ultrasnd_start_voice(u,0);

		/* wait */
		do {
			ptr = ultrasnd_read_voice_current(u,0);
			fprintf(stderr,"\x0D" "ESC to quit, ENTER to move on. 0x%08lx ",ptr); fflush(stderr);

			if (kbhit()) {
				i = getch();
				if (i == 27) {
					die = 1;
					break;
				}
				else if (i == 13) {
					break;
				}
				else if (i == 8) {
					ultrasnd_set_voice_mode(u,0,ultrasnd_read_voice_mode(u,0) ^ ULTRASND_VOICE_MODE_BACKWARDS);
				}
				else if (i == 32) {
					ultrasnd_set_voice_current(u,0,2);
					ultrasnd_set_voice_current(u,0,2);
					ultrasnd_set_voice_mode(u,0,ULTRASND_VOICE_MODE_LOOP | ULTRASND_VOICE_MODE_BIDIR | ULTRASND_VOICE_MODE_16BIT);
				}
				else if (i == 'r' || i == 'R') {
					ultrasnd_set_voice_pan(u,0,0); /* center (FIXME: Despite the code being present, DOSBox's GUS emulation fails to emulate the pan register and everything is rendered at center) */
				}
				else if (i == 'l' || i == 'L') {
					ultrasnd_set_voice_pan(u,0,0xF); /* center (FIXME: Despite the code being present, DOSBox's GUS emulation fails to emulate the pan register and everything is rendered at center) */
				}
				else if (i == 'c' || i == 'C') {
					ultrasnd_set_voice_pan(u,0,0x7); /* center (FIXME: Despite the code being present, DOSBox's GUS emulation fails to emulate the pan register and everything is rendered at center) */
				}
			}
		} while(1);
		fprintf(stderr,"\n");

		/* shut up the GUS */
		for (i=0;i < u->active_voices;i++) {
			ultrasnd_stop_voice(u,i);
			for (j=0;j < 14;j++)
				ultrasnd_select_write16(u,j,0);
		}
	}

	/* ------------------------------------------------------------------------------------------------------- */
	if (!die) {
		printf("16-bit 44.1KHz stereo test (stereo 'aliasing')\n");
		/* ^ by that I mean using the Gravis recommended trick
		     of uploading interleaved data and setting the fractional
		     step to 2 so that the chip simply skips every other
		     sample. naturally this only works in very specific
		     cases, such as the 14-voice mode at 44.1KHz. */

		/* load something into RAM */
		fd = open("16_44khs.wav",O_RDONLY|O_BINARY);
		if (fd < 0) fd = open("..\\..\\16_44khs.wav",O_RDONLY|O_BINARY);
		if (fd < 0) fd = open("..\\..\\..\\16_44khs.wav",O_RDONLY|O_BINARY);
		if (fd < 0) {
			fprintf(stderr,"Cannot open .WAV\n");
			return 1;
		}
		lseek(fd,44,SEEK_SET);
		ofs=0;
		printf("Loading into DRAM "); fflush(stdout);
		if ((dmaptr = ultrasnd_dram_buffer_alloc(u,4096)) != NULL) {
		    printf("phys=0x%08lX ",(unsigned long)(u->dram_xfer_a->phys));
		    while (ofs < (120UL*1024UL) && (rd = _dos_xread(fd,dmaptr,4096)) > 0) {
			ultrasnd_send_dram_buffer(u,ofs,rd,ULTRASND_DMA_DATA_SIZE_16BIT);
			printf("."); fflush(stdout);
			ofs += rd;
		    }
		    printf("\n");
		    /* and then repeat the last sample to avoid click */
		    ultrasnd_poke(u,ofs,ultrasnd_peek(u,0));
		    ultrasnd_poke(u,ofs+1,ultrasnd_peek(u,1));
		    ultrasnd_dram_buffer_free(u);
		}
		else {
		    printf("ERROR: Cannot alloc DMA transfer DRAM upload\n");
		}
		/* and then repeat the last sample to avoid click */
		close(fd);

		/* get the audio going */
		ultrasnd_stop_voice(u,0);
		ultrasnd_set_voice_mode(u,0,ULTRASND_VOICE_MODE_LOOP | ULTRASND_VOICE_MODE_BIDIR | ULTRASND_VOICE_MODE_16BIT);
		ultrasnd_set_voice_fc(u,0,ultrasnd_sample_rate_to_fc(u,44100)*2); /* important: Must be precisely 2x 44.1KHz */
		ultrasnd_set_voice_ramp_rate(u,0,0,0);
		ultrasnd_set_voice_ramp_start(u,0,0xF0); /* NTS: You have to set the ramp start/end because it will override your current volume */
		ultrasnd_set_voice_ramp_end(u,0,0xF0);
		ultrasnd_set_voice_volume(u,0,0xFFF0); /* full vol */
		ultrasnd_set_voice_pan(u,0,0); /* full left */
		ultrasnd_set_voice_ramp_control(u,0,0);
		ultrasnd_set_voice_start(u,0,2+2); /* NTS: If 0 or 1, the card won't stop and will run into 0xFFFFF and beyond */
		ultrasnd_set_voice_end(u,0,ofs-1);
		ultrasnd_set_voice_current(u,0,2+2); /* Start well within the range */
		ultrasnd_set_voice_mode(u,0,ULTRASND_VOICE_MODE_LOOP | ULTRASND_VOICE_MODE_BIDIR | ULTRASND_VOICE_MODE_16BIT);

		ultrasnd_stop_voice(u,1);
		ultrasnd_set_voice_mode(u,1,ULTRASND_VOICE_MODE_LOOP | ULTRASND_VOICE_MODE_BIDIR | ULTRASND_VOICE_MODE_16BIT);
		ultrasnd_set_voice_fc(u,1,ultrasnd_sample_rate_to_fc(u,44100)*2); /* important: Must be precisely 2x 44.1KHz */
		ultrasnd_set_voice_ramp_rate(u,1,0,0);
		ultrasnd_set_voice_ramp_start(u,1,0xF0); /* NTS: You have to set the ramp start/end because it will override your current volume */
		ultrasnd_set_voice_ramp_end(u,1,0xF0);
		ultrasnd_set_voice_volume(u,1,0xFFF0); /* full vol */
		ultrasnd_set_voice_pan(u,1,15); /* full right */
		ultrasnd_set_voice_ramp_control(u,1,0);
		ultrasnd_set_voice_start(u,1,2+2+2); /* NTS: If 0 or 1, the card won't stop and will run into 0xFFFFF and beyond */
		ultrasnd_set_voice_end(u,1,ofs-1);
		ultrasnd_set_voice_current(u,1,2+2+2); /* Start well within the range */
		ultrasnd_set_voice_mode(u,1,ULTRASND_VOICE_MODE_LOOP | ULTRASND_VOICE_MODE_BIDIR | ULTRASND_VOICE_MODE_16BIT);

		ultrasnd_start_voice_imm(u,0);
		ultrasnd_start_voice_imm(u,1);

		/* wait */
		do {
			unsigned long ptr2;
			ptr = ultrasnd_read_voice_current(u,0);
			ptr2 = ultrasnd_read_voice_current(u,1);
			fprintf(stderr,"\x0D" "ESC to quit, ENTER to move on. 0x%08lx,0x%08lx ",ptr,ptr2); fflush(stderr);

			if (kbhit()) {
				i = getch();
				if (i == 27) {
					die = 1;
					break;
				}
				else if (i == 13) {
					break;
				}
				else if (i == 8) {
					ultrasnd_stop_voice(u,0);
					ultrasnd_stop_voice(u,1);

					ultrasnd_set_voice_mode(u,0,ultrasnd_read_voice_mode(u,0) ^ ULTRASND_VOICE_MODE_BACKWARDS);
					ultrasnd_set_voice_mode(u,1,ultrasnd_read_voice_mode(u,0));

					ultrasnd_start_voice(u,0);
					ultrasnd_start_voice(u,1);
				}
				else if (i == 32) {
					ultrasnd_stop_voice(u,0);
					ultrasnd_stop_voice(u,1);

					ultrasnd_set_voice_current(u,0,2);
					ultrasnd_set_voice_current(u,0,2);
					ultrasnd_set_voice_mode(u,0,ULTRASND_VOICE_MODE_LOOP | ULTRASND_VOICE_MODE_BIDIR | ULTRASND_VOICE_MODE_16BIT);

					ultrasnd_set_voice_current(u,1,2+2);
					ultrasnd_set_voice_current(u,1,2+2);
					ultrasnd_set_voice_mode(u,1,ULTRASND_VOICE_MODE_LOOP | ULTRASND_VOICE_MODE_BIDIR | ULTRASND_VOICE_MODE_16BIT);

					ultrasnd_start_voice(u,0);
					ultrasnd_start_voice(u,1);
				}
			}
		} while(1);
		fprintf(stderr,"\n");

		/* shut up the GUS */
		for (i=0;i < u->active_voices;i++) {
			ultrasnd_stop_voice(u,i);
			for (j=0;j < 14;j++)
				ultrasnd_select_write16(u,j,0);
		}
	}

	return 0;
}

