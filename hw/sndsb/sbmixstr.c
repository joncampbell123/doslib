
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/dos/dosbox.h>
#include <hw/8237/8237.h>		/* 8237 DMA */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/dos/doswin.h>
#include <hw/dos/tgusmega.h>
#include <hw/dos/tgussbos.h>

/* Windows 9x VxD enumeration */
#include <windows/w9xvmm/vxd_enum.h>

/* CT1335 */
static const struct sndsb_mixer_control sndsb_mixer_ct1335[] = {
/*      index, of,ln,name */
	{0x02, 1, 3, "Master"},
	{0x06, 1, 3, "MIDI"},
	{0x08, 1, 3, "CD"},
	{0x0A, 1, 2, "Voice"}
};

/* CT1345 */
static const struct sndsb_mixer_control sndsb_mixer_ct1345[] = {
/*      index, of,ln,name */
	{0x04, 1, 3, "Voice R"},		{0x04, 5, 3, "Voice L"},
	{0x0A, 1, 2, "Mic"},
	{0x0C, 1, 2, "Input source"},		{0x0C, 3, 1, "8.8KHz lowpass filter"},		{0x0C, 5, 1, "Input filter bypass"},
	{0x0E, 1, 1, "Stereo switch"},		{0x0E, 5, 1, "Output filter bypass"},
	{0x22, 1, 3, "Master R"},		{0x22, 5, 3, "Master L"},
	{0x26, 1, 3, "MIDI R"},			{0x26, 5, 3, "MIDI L"},
	{0x28, 1, 3, "CD R"},			{0x28, 5, 3, "CD L"},
	{0x2E, 1, 3, "Line R"},			{0x2E, 5, 3, "Line L"}
};

/* CT1745 */
static const struct sndsb_mixer_control sndsb_mixer_ct1745[] = {
/*      index, of,ln,name */
	{0x30, 3, 5, "Master L"},		{0x31, 3, 5, "Master R"},
	{0x32, 3, 5, "Volume L"},		{0x33, 3, 5, "Volume R"},
	{0x34, 3, 5, "MIDI L"},			{0x35, 3, 5, "MIDI R"},
	{0x36, 3, 5, "CD L"},			{0x37, 3, 5, "CD R"},
	{0x38, 3, 5, "Line L"},			{0x39, 3, 5, "Line R"},
	{0x3A, 3, 5, "Mic"},
	{0x3B, 6, 2, "PC speaker"},

	{0x3C, 0, 1, "Out mixer: mic"},
	{0x3C, 1, 1, "Out mixer: CD R"},
	{0x3C, 2, 1, "Out mixer: CD L"},
	{0x3C, 3, 1, "Out mixer: Line R"},
	{0x3C, 4, 1, "Out mixer: Line L"},

	{0x3D, 0, 1, "In mixer L: Mic"},
	{0x3D, 1, 1, "In mixer L: CD R"},
	{0x3D, 2, 1, "In mixer L: CD L"},
	{0x3D, 3, 1, "In mixer L: Line R"},
	{0x3D, 4, 1, "In mixer L: Line L"},
	{0x3D, 5, 1, "In mixer L: MIDI R"},
	{0x3D, 6, 1, "In mixer L: MIDI L"},

	{0x3E, 0, 1, "In mixer R: Mic"},
	{0x3E, 1, 1, "In mixer R: CD R"},
	{0x3E, 2, 1, "In mixer R: CD L"},
	{0x3E, 3, 1, "In mixer R: Line R"},
	{0x3E, 4, 1, "In mixer R: Line L"},
	{0x3E, 5, 1, "In mixer R: MIDI R"},
	{0x3E, 6, 1, "In mixer R: MIDI L"},

	{0x3F, 6, 2, "Input gain L"},
	{0x40, 6, 2, "Input gain R"},
	{0x41, 6, 2, "Output gain L"},
	{0x42, 6, 2, "Output gain R"},
	{0x43, 0, 1, "AGC"},
	{0x44, 4, 4, "Treble L"},
	{0x45, 4, 4, "Treble R"},
	{0x46, 4, 4, "Bass L"},
	{0x47, 4, 4, "Bass R"}
};

/* ESS AudioDrive 688 */
static const struct sndsb_mixer_control sndsb_mixer_ess688[] = {
/*      index, of,ln,name */
	{0x14, 4, 4, "Audio 1 Play Volume L"},		{0x14, 0, 4, "Audio 1 Play Volume R"},
	{0x1A, 4, 4, "Mic Mix Volume L"},		{0x1A, 0, 4, "Mic Mix Volume R"},
	{0x1C, 4, 1, "Ext. Record Source Mute"},	{0x1C, 0, 3, "Ext. Record Source"},	// 0=Mic 2=AuxA(CD) 4=Mic 6=Line see ESS 1688 for more info
	{0x32, 4, 4, "Master Volume L"},		{0x32, 0, 4, "Master Volume R"},
	{0x36, 4, 4, "FM Volume L"},			{0x36, 0, 4, "FM Volume R"},
	{0x38, 4, 4, "AuxA(CD) Volume L"},		{0x38, 0, 4, "AuxA(CD) Volume R"},
	{0x3A, 4, 4, "AuxB Volume L"},			{0x3A, 0, 4, "AuxB Volume R"},
	{0x3C, 0, 3, "PC speaker volume"},
	{0x3E, 4, 4, "Line Volume L"},			{0x3E, 0, 4, "Line Volume R"},
	{0x42, 7, 1, "Serial Mode Input Override"},	{0x42, 4, 3, "Serial Mode Source Select"},
	{0x42, 0, 4, "Serial Mode Mic Record Level"},
	{0x44, 7, 1, "Serial Mode Output Vol Override"},
	{0x44, 4, 3, "Serial Mode Output Select"},
	{0x44, 0, 4, "Serial Mode Master Volume"}

};

const struct sndsb_mixer_control *sndsb_get_mixer_controls(struct sndsb_ctx *card,signed short *count,signed char override) {
	signed char idx = override >= 0 ? override : card->mixer_chip;

	if (idx == SNDSB_MIXER_CT1335) {
		*count = (signed short)(sizeof(sndsb_mixer_ct1335) / sizeof(struct sndsb_mixer_control));
		return sndsb_mixer_ct1335;
	}
	else if (idx == SNDSB_MIXER_CT1345) {
		*count = (signed short)(sizeof(sndsb_mixer_ct1345) / sizeof(struct sndsb_mixer_control));
		return sndsb_mixer_ct1345;
	}
	else if (idx == SNDSB_MIXER_CT1745) {
		*count = (signed short)(sizeof(sndsb_mixer_ct1745) / sizeof(struct sndsb_mixer_control));
		return sndsb_mixer_ct1745;
	}
	else if (idx == SNDSB_MIXER_ESS688) {
		*count = (signed short)(sizeof(sndsb_mixer_ess688) / sizeof(struct sndsb_mixer_control));
		return sndsb_mixer_ess688;
	}

	*count = 0;
	return NULL;
}

void sndsb_write_mixer_entry(struct sndsb_ctx *sb,const struct sndsb_mixer_control *mc,unsigned char nb) {
	unsigned char b;
	if (mc->length == 0) return;
	else if (mc->length == 8) {
		sndsb_write_mixer(sb,mc->index,nb);
	}
	else {
		b = sndsb_read_mixer(sb,mc->index);
		b &= ~(((1 << mc->length) - 1) << mc->offset);
		b |= (nb & ((1 << mc->length) - 1)) << mc->offset;
		sndsb_write_mixer(sb,mc->index,b);
	}
}

unsigned char sndsb_read_mixer_entry(struct sndsb_ctx *sb,const struct sndsb_mixer_control *mc) {
	unsigned char b;
	if (mc->length == 0) return 0;
	b = sndsb_read_mixer(sb,mc->index);
	return (b >> mc->offset) & ((1 << mc->length) - 1);
}

