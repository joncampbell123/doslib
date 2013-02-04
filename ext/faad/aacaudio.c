/* Castus/ISP AAC audio ADTS frame synchronization and detection */
/* (C) 2010-2012 Jonathan Campbell */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "aacaudio.h"

static const unsigned int mpeg_aac_adts_audio_sample_rates[16] = {
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 
    16000, 12000, 11025, 8000,  7350,  0,     0,     0
};

static const unsigned char mpeg_aac_adts_audio_channels[8] = {
    0, 1, 2, 3, 4, 5, 6, 8
};

void mpeg_aac_adts_audio_get_sync_info(mpeg_aac_adts_audio_sync_raw_pattern sp,mpeg_aac_adts_audio_sync_info *i) {
	i->id =				(unsigned  char)((sp >> (56ULL-(12ULL+ 1ULL))) & 0x0001);
	i->layer =			(unsigned  char)((sp >> (56ULL-(13ULL+ 2ULL))) & 0x0003);
	i->protection =			(unsigned  char)((sp >> (56ULL-(15ULL+ 1ULL))) & 0x0001);
	i->object_type =		(unsigned  char)((sp >> (56ULL-(16ULL+ 2ULL))) & 0x0003);
	i->sample_rate_index =		(unsigned  char)((sp >> (56ULL-(18ULL+ 4ULL))) & 0x000F);
	i->private_bit =		(unsigned  char)((sp >> (56ULL-(22ULL+ 1ULL))) & 0x0001);
	i->channel_configuration =	(unsigned  char)((sp >> (56ULL-(23ULL+ 3ULL))) & 0x0007);
	i->original_copy =		(unsigned  char)((sp >> (56ULL-(26ULL+ 1ULL))) & 0x0001);
	i->home =			(unsigned  char)((sp >> (56ULL-(27ULL+ 1ULL))) & 0x0001);
	i->copyright_id =		(unsigned  char)((sp >> (56ULL-(28ULL+ 1ULL))) & 0x0001);
	i->copyright_start =		(unsigned  char)((sp >> (56ULL-(29ULL+ 1ULL))) & 0x0001);
	i->aac_frame_length =		(unsigned short)((sp >> (56ULL-(30ULL+13ULL))) & 0x1FFF);
	i->buffer_fullness =		(unsigned short)((sp >> (56ULL-(43ULL+11ULL))) & 0x07FF);
	i->add_raw_data_blocks =	(unsigned  char)((sp >> (56ULL-(54ULL+ 2ULL))) & 0x0003);
	i->samples_per_frame =		1024;	/* FIXME: Digital Radio Mondale has 960 samples/frame? Do we need to support it? */

	i->sample_rate = mpeg_aac_adts_audio_sample_rates[i->sample_rate_index];
	i->channels = mpeg_aac_adts_audio_channels[i->channel_configuration];
	i->total_raw_data_blocks = i->add_raw_data_blocks+1;
	i->total_samples = i->samples_per_frame * i->total_raw_data_blocks;
	i->sync = sp;
}


uint8_t *mpeg_aac_adts_audio_sync(uint8_t *ptr,uint8_t *fence,mpeg_aac_adts_audio_sync_raw_pattern *p,mpeg_aac_adts_audio_sync_info *i) {
	mpeg_aac_adts_audio_sync_raw_pattern sp = 0;

	if ((ptr+7) > fence)
		return ptr;

	memset(i,0,sizeof(*i));
	while ((ptr+7) <= fence) {
		sp <<= 8ULL;
		sp  |= (mpeg_aac_adts_audio_sync_raw_pattern)(*ptr++);

		if (sp == (mpeg_aac_adts_audio_sync_raw_pattern)0xFFFFFFFFFFFFFFFFULL) {
			ptr += 6-1;
			continue;
		}
		if ((sp&0x00FFFF0000000000ULL) == 0x00FFFF0000000000ULL) {
			continue;
		}
		if ((sp&0x00FFF00000000000ULL) != 0x00FFF00000000000ULL) /* 12-bit sync pattern */
			continue;
		if (p) *p = sp;

		mpeg_aac_adts_audio_get_sync_info(sp,i);

		/* NTS: aac_frame_length includes the ADTS header */
		if (i->aac_frame_length < 7)
			continue;

		i->sample_rate = mpeg_aac_adts_audio_sample_rates[i->sample_rate_index];
		if (i->sample_rate == 0)
			continue;

		i->channels = mpeg_aac_adts_audio_channels[i->channel_configuration];
		if (i->channels == 0)
			continue;

		i->total_raw_data_blocks = i->add_raw_data_blocks+1;
		i->total_samples = i->samples_per_frame * i->total_raw_data_blocks;
		i->sync = sp;
		return ptr-7;
	}

	return fence-7;
}

int mpeg_aac_adts_audio_sync_same_format(mpeg_aac_adts_audio_sync_info *a,mpeg_aac_adts_audio_sync_info *b) {
	return (a->id				== b->id &&
		a->layer			== b->layer &&
		a->object_type			== b->object_type &&
		a->sample_rate_index		== b->sample_rate_index &&
		a->channel_configuration	== b->channel_configuration);
}


