
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
#include <hw/dos/doswin.h>
#include <hw/dos/tgusmega.h>
#include <hw/dos/tgussbos.h>
#include <hw/dos/tgusumid.h>

#include <hw/isapnp/isapnp.h>
#include <hw/sndsb/sndsbpnp.h>

static char                 line[256];
static unsigned char        tmp[512];
static unsigned char far	devnode_raw[4096];

static struct sndsb_ctx*	sb_card = NULL;

static void chomp(char *s) {
    char *e = s + strlen(s) - 1;

    while (e >= s && *e == '\n') *e-- = 0;
}

int main(int argc,char **argv) {
    unsigned char bval,bval2;
	int sc_idx = -1;
	int i,loop=1;
    char *scan;

	printf("Sound Blaster 16 ASP/CSP test program\n");
	for (i=1;i < argc;) {
		char *a = argv[i++];

		if (*a == '-' || *a == '/') {
			unsigned char m = *a++;
			while (*a == m) a++;

            if (0) {
            }
			else {
				return 1;
			}
		}
	}

	/* TODO: Make sure this code still runs reliably without DMA (Direct DAC etc) if DMA not available! */
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

#if !(TARGET_MSDOS == 16 && (defined(__SMALL__) || defined(__COMPACT__)))
	/* it's up to us now to tell it certain minor things */
	sndsb_detect_virtualbox();		// whether or not we're running in VirtualBox
	/* sndsb now allows us to keep the EXE small by not referring to extra sound card support */
	sndsb_enable_sb16_support();		// SB16 support
	sndsb_enable_sc400_support();		// SC400 support
	sndsb_enable_ess_audiodrive_support();	// ESS AudioDrive support
#endif

#if TARGET_MSDOS == 32
	if (sndsb_nmi_32_hook > 0) /* it means the NMI hook is taken */
		printf("Sound Blaster NMI hook/reflection active\n");

	if (gravis_mega_em_detect(&megaem_info)) {
		/* let the user know we're a 32-bit program and MEGA-EM's emulation
		 * won't work with 32-bit DOS programs */
		printf("WARNING: Gravis MEGA-EM detected. Sound Blaster emulation doesn't work\n");
		printf("         with 32-bit protected mode programs (like myself). If you want\n");
		printf("         to test it's Sound Blaster emulation use the 16-bit real mode\n");
		printf("         builds instead.\n");
	}
	if (gravis_sbos_detect() >= 0) {
		printf("WARNING: Gravis SBOS emulation is not 100%% compatible with 32-bit builds.\n");
		printf("         It may work for awhile, but eventually the simulated IRQ will go\n");
		printf("         missing and playback will stall. Please consider using the 16-bit\n");
		printf("         real-mode builds instead. When a workaround is possible, it will\n");
		printf("         be implemented and this warning will be removed.\n");
	}
#elif TARGET_MSDOS == 16
# if defined(__LARGE__)
	if (gravis_sbos_detect() >= 0) {
		printf("WARNING: 16-bit large model builds of the SNDSB program have a known, but not\n");
		printf("         yet understood incompatability with Gravis SBOS emulation. Use the\n");
		printf("         dos86s, dos86m, or dos86c builds.\n");
	}
# endif
#endif
	if (!init_isa_pnp_bios()) {
		printf("Cannot init ISA PnP\n");
		return 1;
	}
    if (find_isa_pnp_bios()) {
        int ret;
        char tmp[192];
        unsigned int j,nodesize=0;
        const char *whatis = NULL;
        unsigned char csn,node=0,numnodes=0xFF,data[192];

        memset(data,0,sizeof(data));
        if (isa_pnp_bios_get_pnp_isa_cfg(data) == 0) {
            struct isapnp_pnp_isa_cfg *nfo = (struct isapnp_pnp_isa_cfg*)data;
            isapnp_probe_next_csn = nfo->total_csn;
            isapnp_read_data = nfo->isa_pnp_port;
        }
        else {
            printf("  ISA PnP BIOS failed to return configuration info\n");
        }

        /* enumerate device nodes reported by the BIOS */
        if (isa_pnp_bios_number_of_sysdev_nodes(&numnodes,&nodesize) == 0 && numnodes != 0xFF && nodesize <= sizeof(devnode_raw)) {
            for (node=0;node != 0xFF;) {
                struct isa_pnp_device_node far *devn;
                unsigned char this_node;

                /* apparently, start with 0. call updates node to
                 * next node number, or 0xFF to signify end */
                this_node = node;
                if (isa_pnp_bios_get_sysdev_node(&node,devnode_raw,ISA_PNP_BIOS_GET_SYSDEV_NODE_CTRL_NOW) != 0) break;

                devn = (struct isa_pnp_device_node far*)devnode_raw;
                if (isa_pnp_is_sound_blaster_compatible_id(devn->product_id,&whatis)) {
                    isa_pnp_product_id_to_str(tmp,devn->product_id);
                    if ((ret = sndsb_try_isa_pnp_bios(devn->product_id,this_node,devn,sizeof(devnode_raw))) <= 0)
                        printf("ISA PnP BIOS: error %d for %s '%s'\n",ret,tmp,whatis);
                    else
                        printf("ISA PnP BIOS: found %s '%s'\n",tmp,whatis);
                }
            }
        }

        /* enumerate the ISA bus directly */
        if (isapnp_read_data != 0) {
            printf("Scanning ISA PnP devices...\n");
            for (csn=1;csn < 255;csn++) {
                isa_pnp_init_key();
                isa_pnp_wake_csn(csn);

                isa_pnp_write_address(0x06); /* CSN */
                if (isa_pnp_read_data() == csn) {
                    /* apparently doing this lets us read back the serial and vendor ID in addition to resource data */
                    /* if we don't, then we only read back the resource data */
                    isa_pnp_init_key();
                    isa_pnp_wake_csn(csn);

                    for (j=0;j < 9;j++) data[j] = isa_pnp_read_config();

                    if (isa_pnp_is_sound_blaster_compatible_id(*((uint32_t*)data),&whatis)) {
                        isa_pnp_product_id_to_str(tmp,*((uint32_t*)data));
                        if ((ret = sndsb_try_isa_pnp(*((uint32_t*)data),csn)) <= 0)
                            printf("ISA PnP: error %d for %s '%s'\n",ret,tmp,whatis);
                        else
                            printf("ISA PnP: found %s '%s'\n",tmp,whatis);
                    }
                }

                /* return back to "wait for key" state */
                isa_pnp_write_data_register(0x02,0x02);	/* bit 1: set -> return to Wait For Key state (or else a Pentium Pro system I own eventually locks up and hangs) */
            }
        }
    }
    /* Non-plug & play scan */
	if (sndsb_try_blaster_var() != NULL) {
		printf("Created card ent. for BLASTER variable. IO=%X MPU=%X DMA=%d DMA16=%d IRQ=%d\n",
			sndsb_card_blaster->baseio,
			sndsb_card_blaster->mpuio,
			sndsb_card_blaster->dma8,
			sndsb_card_blaster->dma16,
			sndsb_card_blaster->irq);
		if (!sndsb_init_card(sndsb_card_blaster)) {
			printf("Nope, didn't work\n");
			sndsb_free_card(sndsb_card_blaster);
		}
	}

    if (sndsb_try_base(0x220))
        printf("Also found one at 0x220\n");
    if (sndsb_try_base(0x240))
        printf("Also found one at 0x240\n");

	/* init card no longer probes the mixer */
	for (i=0;i < SNDSB_MAX_CARDS;i++) {
		struct sndsb_ctx *cx = sndsb_index_to_ctx(i);
		if (cx->baseio == 0) continue;

#if !(TARGET_MSDOS == 16 && (defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
		if (!cx->mixer_probed)
			sndsb_probe_mixer(cx);
#endif

		// having IRQ and DMA changes the ideal playback method and capabilities
		sndsb_update_capabilities(cx);
		sndsb_determine_ideal_dsp_play_method(cx);
	}

	if (sc_idx < 0) {
		int count=0;
		for (i=0;i < SNDSB_MAX_CARDS;i++) {
			const char *ess_str;
			const char *mixer_str;

			struct sndsb_ctx *cx = sndsb_index_to_ctx(i);
			if (cx->baseio == 0) continue;

#if !(TARGET_MSDOS == 16 && (defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
			mixer_str = sndsb_mixer_chip_str(cx->mixer_chip);
			ess_str = sndsb_ess_chipset_str(cx->ess_chipset);
#else
			mixer_str = "";
			ess_str = "";
#endif

			printf("  [%u] base=%X mpu=%X dma=%d dma16=%d irq=%d DSP=%u 1.XXAI=%u\n",
					i+1,cx->baseio,cx->mpuio,cx->dma8,cx->dma16,cx->irq,cx->dsp_ok,cx->dsp_autoinit_dma);
			printf("      MIXER=%u[%s] DSPv=%u.%u SC6600=%u OPL=%X GAME=%X AWE=%X\n",
					cx->mixer_ok,mixer_str,(unsigned int)cx->dsp_vmaj,(unsigned int)cx->dsp_vmin,
					cx->is_gallant_sc6600,cx->oplio,cx->gameio,cx->aweio);
			printf("      ESS=%u[%s] use=%u wss=%X OPL3SAx=%X\n",
					cx->ess_chipset,ess_str,cx->ess_extensions,cx->wssio,cx->opl3sax_controlio);
#ifdef ISAPNP
			if (cx->pnp_name != NULL) {
				isa_pnp_product_id_to_str(temp_str,cx->pnp_id);
				printf("      ISA PnP[%u]: %s %s\n",cx->pnp_csn,temp_str,cx->pnp_name);
			}
#endif
			printf("      '%s'\n",cx->dsp_copyright);

            if (sndsb_check_for_sb16_asp(cx))
                printf("      CSP/ASP chip detected: version=0x%02x\n",cx->asp_chip_version_id);

			count++;
		}
		if (count == 0) {
			printf("No cards found.\n");
			return 1;
		}
		printf("-----------\n");
		printf("Select the card you wish to test: "); fflush(stdout);
		i = getch();
		printf("\n");
		if (i == 27) return 0;
		if (i == 13 || i == 10) i = '1';
		sc_idx = i - '0';
	}

	if (sc_idx < 1 || sc_idx > SNDSB_MAX_CARDS) {
		printf("Sound card index out of range\n");
		return 1;
	}

	sb_card = &sndsb_card[sc_idx-1];
	if (sb_card->baseio == 0) {
		printf("No such card\n");
		return 1;
	}

    if (sb_card->dsp_vmaj != 4 || sb_card->is_gallant_sc6600) {
        printf("Card cannot support ASP chip\n");
        return 1;
    }

    if (!sb_card->has_asp_chip) {
        printf("Card does not have ASP chip (that I know of).\n");
        printf("I will allow you to poke at these commands regardless.\n");
    }

    while (loop) {
        printf("ASP> "); fflush(stdout);
        if (fgets(line,sizeof(line)-1,stdin) == NULL) break;
        chomp(line);

        if (!strcmp(line,"exit")) {
            break;
        }
        else if (!strcmp(line,"dspreset")) {
            if (sndsb_reset_dsp(sb_card))
                printf("DSP reset complete\n");
            else
                printf("DSP reset failed\n");
        }
        else if (!strncmp(line,"setmode ",8)) {
            scan = line+8;
            while (*scan == ' ') scan++;
            bval = (unsigned char)strtoul(scan,&scan,0);

            if (sndsb_sb16asp_set_mode_register(sb_card,bval) < 0) {
                printf("DSP command failed. Resetting...\n");
                sndsb_reset_dsp(sb_card);
            }
        }
        else if (!strncmp(line,"setcparm ",9)) {
            scan = line+9;
            while (*scan == ' ') scan++;
            bval = (unsigned char)strtoul(scan,&scan,0);
            while (*scan == ' ') scan++;
            bval2 = (unsigned char)strtoul(scan,&scan,0);

            if (sndsb_sb16asp_set_codec_parameter(sb_card,bval,bval2) < 0) {
                printf("DSP command failed. Resetting...\n");
                sndsb_reset_dsp(sb_card);
            }
        }
        else if (!strncmp(line,"setreg ",7)) {
            scan = line+7;
            while (*scan == ' ') scan++;
            bval = (unsigned char)strtoul(scan,&scan,0);
            while (*scan == ' ') scan++;
            bval2 = (unsigned char)strtoul(scan,&scan,0);

            if (sndsb_sb16asp_set_register(sb_card,bval,bval2) < 0) {
                printf("DSP command failed. Resetting...\n");
                sndsb_reset_dsp(sb_card);
            }
        }
        else if (!strncmp(line,"getreg ",7)) {
            int i;

            scan = line+7;
            while (*scan == ' ') scan++;
            bval = (unsigned char)strtoul(scan,&scan,0);

            i = sndsb_sb16asp_get_register(sb_card,bval);
            if (i >= 0)
                printf("Register value: 0x%02x\n",i);
            else {
                printf("Failed to read register value\n");
                sndsb_reset_dsp(sb_card);
            }
        }
        else if (!strncmp(line,"getregc ",8)) {
            int i,c;

            scan = line+8;
            while (*scan == ' ') scan++;
            bval = (unsigned char)strtoul(scan,&scan,0);

            printf("Hit SPACE to read more bytes, ENTER to return to command line\n");

            do {
                i = sndsb_sb16asp_get_register(sb_card,bval);
                if (i >= 0)
                    printf("Register value: 0x%02x\n",i);
                else {
                    printf("Failed to read register value\n");
                    sndsb_reset_dsp(sb_card);
                }

                c = getch();
            } while (c != 13 && c != 0);
        }
        else if (!strncmp(line,"getregd ",8)) {
            unsigned long count,i;
            unsigned int tmpi=0;
            FILE *fp;
            int val;

            scan = line+8;
            while (*scan == ' ') scan++;
            bval = (unsigned char)strtoul(scan,&scan,0);
            while (*scan == ' ') scan++;
            count = strtoul(scan,&scan,0);

            sprintf(line,"ASPRGD%02X.BIN",bval);
            fp = fopen(line,"wb");
            if (fp != NULL) {
                for (i=0;i < count;i++) {
                    val = sndsb_sb16asp_get_register(sb_card,bval);
                    if (val >= 0)
                        tmp[tmpi++] = (unsigned char)val;
                    else {
                        printf("Failed to read register value\n");
                        sndsb_reset_dsp(sb_card);
                        break;
                    }

                    if ((i+1) == count || tmpi == sizeof(tmp)) {
                        fwrite(tmp,tmpi,1,fp);
                        tmpi = 0;
                    }
                }

                fclose(fp);
            }
        }
        else if (!strcmp(line,"readaspparm")) {
            int x = sndsb_sb16asp_read_last_codec_parameter(sb_card);

            if (x >= 0)
                printf("Last ASP codec param is 0x%02x\n",x);
            else {
                printf("Unable to read\n");
                sndsb_reset_dsp(sb_card);
            }
        }
        else if (!strcmp(line,"getver")) {
            if (sndsb_sb16asp_get_version(sb_card) >= 0)
                printf("ASP version is 0x%02x\n",sb_card->asp_chip_version_id);
            else {
                printf("Failed to read ASP version\n");
                sndsb_reset_dsp(sb_card);
            }
        }
        else if (!strcmp(line,"dumpregs")) {
            unsigned int x,y;

            printf("ASP register contents:\n");
            for (y=0;y < 256;y += 0x10) {
                printf("%02x: ",(unsigned char)(y));
                for (x=0;x < 16;x++) {
                    printf("%02x ",(unsigned char)sndsb_sb16asp_get_register(sb_card,y+x));
                }

                printf("\n");
            }
        }
        else if (!strcmp(line,"shell")) {
            system("COMMAND");
        }
        else if (!strncmp(line,"f9 ",3)) {
            int i;

            scan = line+3;
            while (*scan == ' ') scan++;
            bval = (unsigned char)strtoul(scan,&scan,0);

            i = sndsb_write_dsp(sb_card,0xF9);
            if (i >= 0) i = sndsb_write_dsp(sb_card,bval);
            if (i >= 0) {
                i = sndsb_read_dsp(sb_card);
                if (i >= 0) printf("Result: 0x%02x\n",i);
            }
            if (i < 0) {
                printf("Command failed\n");
                sndsb_reset_dsp(sb_card);
            }
        }

        else if (!strcmp(line,"help")) {
            printf("exit                    Exit this program\n");
            printf("dspreset                Reset DSP\n");
            printf("setmode 0xXX            ASP set mode 0xXX (common: 0xFC, 0xFA, 0xF9, 0x00)\n");
            printf("setcparm 0xXX 0xXX      ASP set codec param 0xXX value 0xXX\n");
            printf("readaspparm             ASP read last codec param\n");
            printf("setreg 0xXX 0xXX        ASP set register reg 0xXX value 0xXX\n");
            printf("getreg 0xXX             ASP read register reg 0xXX\n");
            printf("getregc 0xXX            ASP read register reg 0xXX continously\n");
            printf("getregd 0xXX 0xXX       ASP read register reg 0xXX dump to file 0xXX bytes\n");
            printf("getver                  ASP read version\n");
            printf("shell                   Shell to DOS, type exit to return\n");
            printf("dumpregs                ASP dump all registers\n");
            printf("f9 0xXX                 ASP unknown command 0xF9, index? 0x00\n");
        }
        else {
            printf("Unknown command '%s'\n",line);
        }
    }

	sndsb_free_card(sb_card);
	free_sndsb(); /* will also de-ref/unhook the NMI reflection */
	return 0;
}

