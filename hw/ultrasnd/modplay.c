
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

static struct ultrasnd_ctx*	gus = NULL;

static volatile unsigned char	IRQ_anim = 0;
static volatile unsigned char	gus_irq_count = 0;

static unsigned char		old_irq_masked = 0;
static unsigned char		dont_chain_irq = 1;	// FIXME: I don't know why IRQ chaining is a problem, but it is.
static unsigned char		no_dma=0;

static void draw_irq_indicator() {
	VGA_ALPHA_PTR wr = vga_state.vga_alpha_ram;
	unsigned char i;

	wr[0] = 0x1E00 | 'G';
	wr[1] = 0x1E00 | 'U';
	wr[2] = 0x1E00 | 'S';
	wr[3] = 0x1E00 | 'I';
	wr[4] = 0x1E00 | 'R';
	wr[5] = 0x1E00 | 'Q';
	for (i=0;i < 4;i++) wr[i+6] = (uint16_t)(i == IRQ_anim ? 'x' : '-') | 0x1E00;
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
	printf("modplay [options] -m <mod file>\n");
#if !(TARGET_MSDOS == 16 && defined(__COMPACT__)) /* this is too much to cram into a small model EXE */
	printf(" /h /help             This help\n");
	printf(" /nochain             Don't chain to previous IRQ (sound blaster IRQ)\n");
	printf(" /nodma               Don't use DMA channel\n");
	printf(" /d                   Enable GUS library debug messages\n");
#endif
}

char *mod_file = NULL;
unsigned int mod_samples = 0;
unsigned int mod_patterns = 0;
unsigned int mod_song_length = 0;

unsigned char mod_pattern[128];

unsigned char temp[4096];

struct mod_sample {
    unsigned long   file_offset;
    unsigned long   ram_offset;
    unsigned long   size;

    signed char     finetune;
    unsigned char   volume;
    unsigned long   repeat_point;
    unsigned long   repeat_length;
};

struct mod_sample mod_sample[32];

unsigned long pattern_block_size = 0;
#if TARGET_MSDOS == 32
unsigned char *pattern_data = NULL;
#else
unsigned char far *pattern_data = NULL;
#endif

int load_mod() {
    unsigned long rammax;
    unsigned long ramofs[4];
    unsigned long sof;
    unsigned long tof;
    unsigned int i,ri;
    int fd;

    rammax = 256ul << 10ul;

    ramofs[0] = 0;
    ramofs[1] = 0;
    ramofs[2] = 0;
    ramofs[3] = 0;

    fd = open(mod_file,O_RDONLY | O_BINARY);
    if (fd < 0) return 0;

    /* first 20 bytes: Song name */
    /* 20 + (30 * sample): Sample info */
    /* offset 1080: 'M.K.', or 'M!K!' or sometimes other IDs as well */
    if (lseek(fd,1080,SEEK_SET) != 1080 || read(fd,temp,4) != 4) goto fail;

    pattern_block_size = 256u * 4u;

    mod_samples = 15;
    if (!memcmp(temp,"M.K.",4) ||
        !memcmp(temp,"M!K!",4) ||
        !memcmp(temp,"FLT4",4) ||
        !memcmp(temp,"FLT8",4) ||
        !memcmp(temp,"10CH",4))
        mod_samples = 31;

    if (!memcmp(temp,"10CH",4))
        pattern_block_size = 256u * 10u;

    tof = 20ul + (30ul * (unsigned long)mod_samples);
    if (lseek(fd,tof,SEEK_SET) != tof || read(fd,temp,2+128) != (2+128)) goto fail;
    mod_song_length = temp[0];
    memcpy(mod_pattern,temp+2,128);

    mod_patterns = 0;
    for (i=0;i < 128;i++) {
        unsigned int pn = (unsigned int)mod_pattern[i] + 1u;
        if (mod_patterns < pn) mod_patterns = pn;
    }

    printf("MOD: samples=%u patterns=%u song_length=%u\n",
        mod_samples,mod_patterns,mod_song_length);

    tof = 20ul + (30ul * (unsigned long)mod_samples) + 2u + 128u;
    if (mod_samples != 15) tof += 4u;
    printf("     pattern_ofs=%lu\n",(unsigned long)tof);
    if (lseek(fd,tof,SEEK_SET) != tof) goto fail;

    {
        unsigned long sz = (unsigned long)mod_patterns * (unsigned long)pattern_block_size;

#if TARGET_MSDOS == 32
        pattern_data = malloc(sz);
        if (pattern_data == NULL) {
            printf("Allocation failure (pattern data)\n");
            goto fail;
        }
        printf("     Pattern data: %p\n",pattern_data);
        if (read(fd,pattern_data,sz) != sz) {
            printf("Read failure (pattern data)\n");
            goto fail;
        }
#else
        {
            unsigned sg = 0;
            if (_dos_allocmem((unsigned)(sz >> 4UL),&sg) != 0) {
                printf("Allocation failure (pattern data)\n");
                goto fail;
            }
            pattern_data = MK_FP(sg,0);
        }
        printf("     Pattern data: %Fp\n",pattern_data);
        {
            unsigned sg = FP_SEG(pattern_data);
            for (i=0;i < mod_patterns;i++) {
                unsigned long o = pattern_block_size * (unsigned long)i;
                unsigned char far *p = MK_FP(sg + (unsigned)(o >> 4ul),(unsigned)(o & 0xful));
                unsigned rd = 0;

                if (_dos_read(fd,p,(unsigned)pattern_block_size,&rd) != 0 || rd != (unsigned)pattern_block_size) {
                    printf("Read failure (pattern data)\n");
                    goto fail;
                }
            }
        }
#endif
    }

    sof = 20ul + (30ul * (unsigned long)mod_samples) + 2u + 128u + ((unsigned long)mod_patterns * (unsigned long)pattern_block_size);
    if (mod_samples != 15) sof += 4ul;
    printf("     samples_ofs=%lu\n",(unsigned long)sof);
    for (i=0;i < mod_samples;i++) {
        struct mod_sample *s = &mod_sample[i];

        memset(s,0,sizeof(*s));

        tof = 20ul + (30ul * (unsigned long)i);
        if (lseek(fd,tof,SEEK_SET) != tof || read(fd,temp,30) != 30) goto fail;

        /* +0-21 Sample name
         * +22 WORD, sample length in words
         * +24 finetune (low 4 bits)
         * +25 volume for sample (0x00-0x40)
         * +26 repeat point, words
         * +28 repeat length, words
         * =30 */
        s->file_offset = sof + 4ul; /* 4 byte header? */
        s->size = ((unsigned long)(((unsigned)temp[22] << 8u) + ((unsigned)temp[23]))) * 2ul;
        s->finetune = (signed char)(temp[24] & 0xF);
        if (s->finetune >= 8) s->finetune -= 0x10;
        s->volume = temp[25];
        s->repeat_point = ((unsigned long)(((unsigned)temp[26] << 8u) + ((unsigned)temp[27]))) * 2ul;
        s->repeat_length = ((unsigned long)(((unsigned)temp[28] << 8u) + ((unsigned)temp[29]))) * 2ul;

        sof += s->size;

        s->ram_offset = ~0ul;

        if (s->size != 0ul) {
            // make room in GUS RAM for the sample,
            // taking into consideration that you shouldn't cross 256KB boundaries.
            // also take into consideration that DMA with the GUS requires DRAM offsets
            // to be a multiple of 16 (32 if 16-bit PCM)
            for (ri=0;ri < 4;ri++) {
                if ((ramofs[ri] + s->size + 0x20) < rammax) {
                    ramofs[ri] = (ramofs[ri] + 0x1F) & (~0x1F);
                    s->ram_offset = ramofs[ri] + (rammax * (unsigned long)ri);
                    ramofs[ri] += s->size;
                    break;
                }
            }
        }

        printf("     sample[%u]: fofs=%lu rofs=%lu size=%lu ft=%d vol=%u rep=%lu rlen=%lu\n",
            i,(unsigned long)s->file_offset,(unsigned long)s->ram_offset,
            (unsigned long)s->size,s->finetune,
            s->volume,
            (unsigned long)s->repeat_point,
            (unsigned long)s->repeat_length);

        if (s->size != 0ul) {
            if (s->ram_offset == (~0ul)) {
                printf("Not enough room in GUS RAM\n");
                goto fail;
            }
        }
    }

    {
        size_t bufsize = 16384;
        unsigned char FAR *dram_buf = ultrasnd_dram_buffer_alloc(gus,bufsize);
        if (dram_buf == NULL) {
            printf("Cannot alloc DMA buffer\n");
            goto fail;
        }

        /* load samples into GUS RAM. Use DMA, of course */
        for (i=0;i < mod_samples;i++) {
            struct mod_sample *s = &mod_sample[i];

            if (s->size != 0 && s->ram_offset != (~0ul)) {
                unsigned long rem,ramoff;
                unsigned rd;

                printf("Loading sample %u\n",i);

                if (lseek(fd,s->file_offset,SEEK_SET) != s->file_offset) {
                    printf("Seek error, samples\n");
                    goto fail;
                }

                rem = s->size;
                ramoff = s->ram_offset;
                while (rem >= bufsize) {
                    rd = 0;
                    if (_dos_read(fd,dram_buf,bufsize,&rd) != 0) {
                        printf("Read error, samples\n");
                        goto fail;
                    }
                    if (ultrasnd_send_dram_buffer(gus,ramoff,bufsize,ULTRASND_DMA_TC_IRQ) == 0) {
                        printf("Send to GUS error\n");
                        goto fail;
                    }
                    ramoff += bufsize;
                    rem -= bufsize;
                }
                if (rem != 0) {
                    rd = 0;
                    if (_dos_read(fd,dram_buf,rem,&rd) != 0) {
                        printf("Read error, samples\n");
                        goto fail;
                    }
                    if (ultrasnd_send_dram_buffer(gus,ramoff,rem,ULTRASND_DMA_TC_IRQ) == 0) {
                        printf("Send to GUS error\n");
                        goto fail;
                    }
                }
            }
        }

        ultrasnd_dram_buffer_free(gus);
    }

    close(fd);
    return 1;
fail:
    ultrasnd_dram_buffer_free(gus);
    close(fd);
    return 0;
}

int main(int argc,char **argv) {
	int i;

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
            else if (!strcmp(a,"m")) {
                a = argv[++i];
                if (*a == NULL) return 1;
                mod_file = a;
            }
			else if (!strcmp(a,"nodma"))
				no_dma = 1;
			else if (!strcmp(a,"nochain"))
				dont_chain_irq = 1;
			else if (!strcmp(a,"chain"))
				dont_chain_irq = 0;
			else {
				help();
				return 1;
			}
		}
	}

    if (mod_file == NULL) {
        fprintf(stderr,"MOD file required\n");
        return 1;
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

	i = 0;
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
    else {
        fprintf(stderr,"Need GUS card\n");
        return 1;
    }
    if (no_dma) gus->use_dma = 0;
    printf("Using DMA: %u\n",gus->use_dma);

	if (gus->irq1 >= 0) {
		old_irq_masked = p8259_is_masked(gus->irq1);
		old_irq = _dos_getvect(irq2int(gus->irq1));
		_dos_setvect(irq2int(gus->irq1),gus_irq);
		p8259_unmask(gus->irq1);
	}

	i = int10_getmode();
	if (i != 3) int10_setmode(3);

    if (load_mod()) {
        printf("MOD loaded\n");
    }

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

