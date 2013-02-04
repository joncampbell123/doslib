/* Castus/ISP AAC audio ADTS frame synchronization and detection */

#ifndef ___MPEG_AAC_AUDIO_H
#define ___MPEG_AAC_AUDIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MPEG_AAC_AUDIO_ADTS_HEADER_SIZE 7

typedef uint64_t mpeg_aac_adts_audio_sync_raw_pattern;

typedef struct mpeg_aac_adts_audio_sync_info {
	/* sync pattern                                    +0+0   12 bits */
	unsigned char		id;			/* +1+4    1 bit  */
	unsigned char		layer;			/* +1+5    2 bit  */
	unsigned char		protection;		/* +1+7    1 bit  */
	unsigned char		object_type;		/* +2+0    2 bits */
	unsigned char		sample_rate_index;	/* +2+2    4 bits */
	unsigned char		private_bit;		/* +2+6    1 bit  */
	unsigned char		channel_configuration;	/* +2+7    3 bits */
	unsigned char		original_copy;		/* +3+2    1 bit  */
	unsigned char		home;			/* +3+3    1 bit  */
	unsigned char		copyright_id;		/* +3+4    1 bit  */
	unsigned char		copyright_start;	/* +3+5    1 bit  */
	unsigned short		aac_frame_length;	/* +3+6   13 bits */
	unsigned short		buffer_fullness;	/* +5+3   11 bits */
	unsigned char		add_raw_data_blocks;	/* +6+6    2 bits */
							/* +7+0   TOTAL 7 bytes */
	unsigned int		sample_rate;
	unsigned char		total_raw_data_blocks;
	unsigned int		samples_per_frame;
	unsigned int		total_samples;
	unsigned char		channels;

	mpeg_aac_adts_audio_sync_raw_pattern	sync;
} mpeg_aac_adts_audio_sync_info;

uint8_t *mpeg_aac_adts_audio_sync(uint8_t *ptr,uint8_t *fence,mpeg_aac_adts_audio_sync_raw_pattern *p,mpeg_aac_adts_audio_sync_info *i);
void mpeg_aac_adts_audio_get_sync_info(mpeg_aac_adts_audio_sync_raw_pattern sp,mpeg_aac_adts_audio_sync_info *i);
int mpeg_aac_adts_audio_sync_same_format(mpeg_aac_adts_audio_sync_info *a,mpeg_aac_adts_audio_sync_info *b);

#ifdef __cplusplus
}
#endif

#endif /* ___MPEG_AAC_AUDIO_H */

