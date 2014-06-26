/* test.c
 *
 * Adlib OPL2/OPL3 FM synthesizer chipset test program.
 * (C) 2010-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box]
 *
 * This test program uses a "text user interface" to allow you to play
 * with the OPL2/OPL3 chipset and it's parameters. Some "instruments"
 * are preset for you if you want to make noise faster.
 */
 
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
#include <hw/vga/vgagui.h>
#include <hw/vga/vgatty.h>
#include <hw/adlib/adlib.h>

static unsigned int musical_scale[18] = {
	0x1B0,			/* E */
	0x1CA,			/* F */
	0x1E5,			/* f# */
	0x202,			/* G */
	0x220,			/* G# */
	0x241,			/* A */
	0x263,			/* A# */
	0x287,			/* B */
	0x2AE,			/* C */

	0x2B0,			/* E */
	0x2CA,			/* F */
	0x2E5,			/* f# */
	0x302,			/* G */
	0x320,			/* G# */
	0x341,			/* A */
	0x363,			/* A# */
	0x387,			/* B */
	0x3AE,			/* C */
};

int main(int argc,char **argv) {
	int i,loop,redraw,c,cc,selector=0,redrawln=0,hselect=0,selectsub=0;
	VGA_ALPHA_PTR vga;
	char tmp[128];

	printf("ADLIB FM test program\n");

	if (!probe_vga()) {
		printf("Cannot init VGA\n");
		return 1;
	}
	if (!init_adlib()) {
		printf("Cannot init library\n");
		return 1;
	}

	int10_setmode(3);

	/* for VGA: free up space if needed by going to 80x50 */
	if (adlib_fm_voices > 9)
		vga_bios_set_80x50_text();

	memset(adlib_fm,0,sizeof(adlib_fm));
	memset(&adlib_reg_bd,0,sizeof(adlib_reg_bd));
	for (i=0;i < adlib_fm_voices;i++) {
		struct adlib_fm_operator *f;
		f = &adlib_fm[i].mod;
		f->ch_a = f->ch_b = f->ch_c = f->ch_d = 1;
		f = &adlib_fm[i].car;
		f->ch_a = f->ch_b = f->ch_c = f->ch_d = 1;
	}

	for (i=0;i < adlib_fm_voices;i++) {
		struct adlib_fm_operator *f;

		f = &adlib_fm[i].mod;
		f->mod_multiple = 1;
		f->total_level = 63 - 16;
		f->attack_rate = 15;
		f->decay_rate = 0;
		f->sustain_level = 7;
		f->release_rate = 7;
		f->f_number = musical_scale[i%18];
		f->octave = 4;
		f->key_on = 0;

		f = &adlib_fm[i].car;
		f->mod_multiple = 1;
		f->total_level = 63 - 16;
		f->attack_rate = 15;
		f->decay_rate = 0;
		f->sustain_level = 7;
		f->release_rate = 7;
		f->f_number = 0;
		f->octave = 0;
		f->key_on = 0;
	}

	adlib_apply_all();

	vga_write_color(0x07);
	vga_clear();

	loop=1;
	redraw=1;
	while (loop) {
		if (redraw || redrawln) {
			if (redraw) {
				for (vga=vga_alpha_ram,cc=0;cc < (80*vga_height);cc++) *vga++ = 0x1E00 | 177;
				vga_moveto(0,0);
				vga_write_color(0x1F);
				sprintf(tmp,"Adlib FM, %u-voice %s. Use Z & X to adj  F10=PRESET F1=QUIET ",adlib_fm_voices,
					(adlib_flags & ADLIB_FM_OPL3) ? "OPL3" :
					(adlib_flags & ADLIB_FM_DUAL_OPL2) ? "Dual OPL2" : "OPL2");
				vga_write(tmp);
				if (adlib_flags & ADLIB_FM_OPL3) vga_write("F2=OPL3 off ");
			}

			if (redrawln || redraw) {
				struct adlib_reg_bd *bd = &adlib_reg_bd;
				static const char *hsel_str[18] = {
					"Amplitude modulatation",
					"Vibrato",
					"Sustain",
					"Key scaling rate",
					"Modulator frequency multiple",
					"Level key scaling",
					"Total level",
					"Attack rate",
					"Decay rate",
					"Sustain level",
					"Release rate",
					"KEY ON",
					"Octave",
					"F-Number",
					"Feedback",
					"Connection (operator 1 -> operator 2)",
					"Waveform",
					"Channel mapping (OPL3)"
				};

				vga_write_color(0x1A);

				vga_moveto(0,2);
				sprintf(tmp,"AM=%u VB=%u RYTHM=%u BAS=%u SNA=%u TOM=%u CYM=%u HI=%u\n",
					bd->am_depth,
					bd->vibrato_depth,
					bd->rythm_enable,
					bd->bass_drum_on,
					bd->snare_drum_on,
					bd->tom_tom_on,
					bd->cymbal_on,
					bd->hi_hat_on);
				vga_write(tmp);

				vga_moveto(0,3);
				vga_write("                                                       ");
				vga_moveto(0,3);
				vga_write(hsel_str[hselect]);

				vga_moveto(0,4);
				vga_write_color(hselect ==  0 ? 0x70 : 0x1E);		vga_write("AM ");
				vga_write_color(hselect ==  1 ? 0x70 : 0x1E);		vga_write("VB ");
				vga_write_color(hselect ==  2 ? 0x70 : 0x1E);		vga_write("SUST ");
				vga_write_color(hselect ==  3 ? 0x70 : 0x1E);		vga_write("KSR ");
				vga_write_color(hselect ==  4 ? 0x70 : 0x1E);		vga_write("MMUL ");
				vga_write_color(hselect ==  5 ? 0x70 : 0x1E);		vga_write("LKS ");
				vga_write_color(hselect ==  6 ? 0x70 : 0x1E);		vga_write("TL ");
				vga_write_color(hselect ==  7 ? 0x70 : 0x1E);		vga_write("AR ");
				vga_write_color(hselect ==  8 ? 0x70 : 0x1E);		vga_write("DR ");
				vga_write_color(hselect ==  9 ? 0x70 : 0x1E);		vga_write("SL ");
				vga_write_color(hselect == 10 ? 0x70 : 0x1E);		vga_write("RR ");
				vga_write_color(hselect == 11 ? 0x70 : 0x1E);		vga_write("KEY ");
				vga_write_color(hselect == 12 ? 0x70 : 0x1E);		vga_write("OCT ");
				vga_write_color(hselect == 13 ? 0x70 : 0x1E);		vga_write("FNUM ");
				vga_write_color(hselect == 14 ? 0x70 : 0x1E);		vga_write("FEED ");
				vga_write_color(hselect == 15 ? 0x70 : 0x1E);		vga_write("CON ");
				vga_write_color(hselect == 16 ? 0x70 : 0x1E);		vga_write("WV ");
				vga_write_color(hselect == 17 ? 0x70 : 0x1E);		vga_write("ABCD ");

				for (i=0;i < adlib_fm_voices;i++) {
					struct adlib_fm_operator *f;
					double freq;
					
					f = &adlib_fm[i].mod;
					vga_moveto(0,5+i*2);
					freq = adlib_fm_op_to_freq(f);
					vga_write_color(i == selector && selectsub == 0 ? 0x70 : 0x1E);
					cc = sprintf(tmp,"%u  %u  %u    %u   %-2u   %u   %-2u %-2u %-2u %-2u %-2u %u   %u   %-4u %u    %u   %u  %c%c%c%c %u %.1fHz ",
						f->am,			f->vibrato,		f->sustain,		f->key_scaling_rate,
						f->mod_multiple,	f->level_key_scale,	f->total_level,		f->attack_rate,
						f->decay_rate,		f->sustain_level,	f->release_rate,	f->key_on,
						f->octave,		f->f_number,		f->feedback,		f->connection,
						f->waveform,		f->ch_a?'*':'-',	f->ch_b?'*':'-',	f->ch_c?'*':'-',
						f->ch_d?'*':'-',	i+1,			freq);
					vga_write(tmp);

					f = &adlib_fm[i].car;
					vga_moveto(0,5+i*2+1);
					vga_write_color(i == selector && selectsub == 1 ? 0x70 : 0x1E);
					cc = sprintf(tmp,"%u  %u  %u    %u   %-2u   %u   %-2u %-2u %-2u %-2u %-2u                       %u       CAR   ",
						f->am,			f->vibrato,		f->sustain,		f->key_scaling_rate,
						f->mod_multiple,	f->level_key_scale,	f->total_level,		f->attack_rate,
						f->decay_rate,		f->sustain_level,	f->release_rate,	f->waveform);
					vga_write(tmp);
				}
			}

			redrawln = 0;
			redraw = 0;
		}

		if (kbhit()) {
			c = getch();
			if (c == 0) c = getch() << 8;

			if (c == 27) {
				loop = 0;
			}
			else if (c == 0x3B00) { /* F1 */
				for (i=0;i < adlib_fm_voices;i++) {
					adlib_fm[i].mod.key_on = 0;
					adlib_fm[i].car.key_on = 0;
					adlib_update_groupA0(i,&adlib_fm[i]);
				}
				redrawln = 1;
			}
			else if (c == 0x3C00) { /* F2 */
				if (adlib_flags & ADLIB_FM_OPL3) {
					shutdown_adlib_opl3();
					int10_setmode(3);
					redraw = 1;
				}
			}
			else if (c == 0x4400) { /* F10 */
				unsigned short op = adlib_voice_to_op[selector];

				vga_write_color(0x07);
				vga_clear();
				vga_moveto(0,0);

				vga_write("Choose an instrument to load into the channel:\n");
				vga_write(" 1. Violin     2. Piano     3. Harpsichord     4. Horn      5. Deep bass drum\n");
				vga_write(" 6. Small drum \n");
				vga_write_sync();

				c = getch();

				if (c == '1')
					adlib_fm[selector] =
						(adlib_flags & ADLIB_FM_OPL3 ?
						 adlib_fm_preset_violin_opl3 : adlib_fm_preset_violin_opl2);
				else if (c == '2')
					adlib_fm[selector] = adlib_fm_preset_piano;
				else if (c == '3')
					adlib_fm[selector] = adlib_fm_preset_harpsichord;
				else if (c == '4')
					adlib_fm[selector] = adlib_fm_preset_horn;
				else if (c == '5')
					adlib_fm[selector] = adlib_fm_preset_deep_bass_drum;
				else if (c == '6')
					adlib_fm[selector] = adlib_fm_preset_small_drum;

				adlib_update_groupA0(selector,&adlib_fm[selector]);
				adlib_update_groupC0(selector,&adlib_fm[selector]);
				adlib_update_operator(op,&adlib_fm[selector].mod);
				adlib_update_operator(op+3,&adlib_fm[selector].car);

				redraw = 1;
			}
			else if (c == ' ') {
				adlib_fm[selector].mod.key_on ^= 1;
				adlib_update_groupA0(selector,&adlib_fm[selector]);
				redrawln=1;
			}
			else if (c == 'a') {
				if (hselect == 17) {
					struct adlib_fm_operator *f = &adlib_fm[selector].mod; f->ch_a ^= 1;
					adlib_update_groupC0(selector,&adlib_fm[selector]);
				}
				else {
					adlib_reg_bd.am_depth ^= 1;
					adlib_update_bd(&adlib_reg_bd);
				}
				redrawln = 1;
			}
			else if (c == 'v') {
				adlib_reg_bd.vibrato_depth ^= 1;
				adlib_update_bd(&adlib_reg_bd);
				redrawln = 1;
			}
			else if (c == 'r') {
				adlib_reg_bd.rythm_enable ^= 1;
				adlib_update_bd(&adlib_reg_bd);
				redrawln = 1;
			}
			else if (c == 'b') {
				if (hselect == 17) {
					struct adlib_fm_operator *f = &adlib_fm[selector].mod; f->ch_b ^= 1;
					adlib_update_groupC0(selector,&adlib_fm[selector]);
				}
				else {
					adlib_reg_bd.bass_drum_on ^= 1;
					adlib_update_bd(&adlib_reg_bd);
				}
				redrawln = 1;
			}
			else if (c == 's') {
				adlib_reg_bd.snare_drum_on ^= 1;
				adlib_update_bd(&adlib_reg_bd);
				redrawln = 1;
			}
			else if (c == 't') {
				adlib_reg_bd.tom_tom_on ^= 1;
				adlib_update_bd(&adlib_reg_bd);
				redrawln = 1;
			}
			else if (c == 'c') {
				if (hselect == 17) {
					struct adlib_fm_operator *f = &adlib_fm[selector].mod; f->ch_c ^= 1;
					adlib_update_groupC0(selector,&adlib_fm[selector]);
				}
				else {
					adlib_reg_bd.cymbal_on ^= 1;
					adlib_update_bd(&adlib_reg_bd);
				}
				redrawln = 1;
			}
			else if (c == 'd') {
				if (hselect == 17) {
					struct adlib_fm_operator *f = &adlib_fm[selector].mod; f->ch_d ^= 1;
					adlib_update_groupC0(selector,&adlib_fm[selector]);
				}
				else {
				}
				redrawln = 1;
			}
			else if (c == 'h') {
				adlib_reg_bd.hi_hat_on ^= 1;
				adlib_update_bd(&adlib_reg_bd);
				redrawln = 1;
			}
			else if (c == 'z' || c == 'Z' || c == 'x' || c == 'X') {
				struct adlib_fm_operator *f;
				int dec = tolower(c) == 'z';
				unsigned short operator;

				switch (hselect) {
					case 11:selectsub = 0;
						break;
				}

				if (selectsub) f = &adlib_fm[selector].car;
				else           f = &adlib_fm[selector].mod;
				operator = adlib_voice_to_op[selector] + (selectsub*3);

				switch (hselect) {
					case 0:		f->am ^= 1;			adlib_update_group20(operator,f); break;
					case 11:	f->key_on ^= 1;			adlib_update_groupA0(selector,&adlib_fm[selector]); break;
					case 1:		f->vibrato ^= 1;		adlib_update_group20(operator,f); break;
					case 2:		f->sustain ^= 1;		adlib_update_group20(operator,f); break;
					case 15:	f->connection ^= 1;		adlib_update_group20(operator,f); break;
					case 3:		f->key_scaling_rate ^= 1;	adlib_update_group20(operator,f); break;

					case 4:		if (dec) f->mod_multiple--; else f->mod_multiple++;
							adlib_update_group20(operator,f); break;
					case 5:		if (dec) f->level_key_scale--; else f->level_key_scale++;
							adlib_update_group40(operator,f); break;
					case 6:		if (dec) f->total_level--; else f->total_level++;
							adlib_update_group40(operator,f); break;
					case 7:		if (dec) f->attack_rate--; else f->attack_rate++;
							adlib_update_group60(operator,f); break;
					case 8:		if (dec) f->decay_rate--; else f->decay_rate++;
							adlib_update_group60(operator,f); break;
					case 9:		if (dec) f->sustain_level--; else f->sustain_level++;
							adlib_update_group80(operator,f); break;
					case 10:	if (dec) f->release_rate--; else f->release_rate++;
							adlib_update_group80(operator,f); break;
					case 12:	if (dec) f->octave--; else f->octave++;
							adlib_update_groupA0(selector,&adlib_fm[selector]); break;
					case 13:	if (dec) f->f_number--; else f->f_number++;
							adlib_update_groupA0(selector,&adlib_fm[selector]); break;
					case 14:	if (dec) f->feedback--; else f->feedback++;
							adlib_update_groupC0(selector,&adlib_fm[selector]); break;
					case 16:	if (dec) f->waveform--; else f->waveform++;
							adlib_update_groupE0(operator,f); break;
				};

				redrawln=1;
			}
			else if (c == 0x4800) {
				if (selectsub && !(hselect >= 11 && hselect <= 15)) {
					selectsub = 0;
					redrawln = 1;
				}
				else if (selector > 0) {
					selectsub = !(hselect >= 11 && hselect <= 15);
					selector--;
					redrawln = 1;
				}
			}
			else if (c == 0x4B00) {
				if (hselect > 0) {
					hselect--;
					redrawln=1;
				}
			}
			else if (c == 0x4D00) {
				if (hselect < 17) {
					hselect++;
					redrawln=1;
				}
			}
			else if (c == 0x5000) {
				if (selectsub == 0 && !(hselect >= 11 && hselect <= 15)) {
					selectsub = 1;
					redrawln = 1;
				}
				else if ((selector+1) < adlib_fm_voices) {
					selectsub = 0;
					selector++;
					redrawln=1;
				}
			}

		}
	}

	shutdown_adlib();
	int10_setmode(3);

	return 0;
}

