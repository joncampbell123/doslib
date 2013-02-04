
#include <sys/types.h>
#include <sys/stat.h>
#if defined(_MSC_VER)
# include <io.h>
#else
# include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>

#include <ext/zlib/zlib.h>

#include "rawint.h"

#ifdef __cplusplus
extern "C" {
#endif

#define quicktime_type_const(a,b,c,d) ( \
	(((quicktime_atom_type_t)(a)) << 24U) | \
	(((quicktime_atom_type_t)(b)) << 16U) | \
	(((quicktime_atom_type_t)(c)) <<  8U) | \
	(((quicktime_atom_type_t)(d)) <<  0U)   \
)

typedef uint32_t	quicktime_atom_type_t;

typedef uint16_t	quicktime_16_fixed_pt_8_8;
typedef uint32_t	quicktime_32_fixed_pt_16_16;
typedef uint32_t	quicktime_32_fixed_pt_2_30;

/* NTS: QuickTime docs officially state that an atom with size==0 can only happen at the top level,
 *      atoms deeper down can't do that */
typedef struct quicktime_atom {
	uint64_t		absolute_header_offset;
	uint64_t		absolute_data_offset;
	uint64_t		absolute_atom_end;
	uint64_t		data_length;
	uint64_t		read_offset;
	unsigned char		extended;	/* whether the chunk has size==1 and the 64-bit length after the header */
	unsigned char		extends_to_eof;	/* if set: chunk extends to eof and data_length was faked by reader */
	unsigned char		reader_knows_there_are_subchunks;
	uint32_t		reader_knows_subchunks_are_at;
	quicktime_atom_type_t	type;
} quicktime_atom;

typedef struct quicktime_stack {
	int			eof;
	int			depth;
	int			current;
	int			fd,ownfd;
	uint64_t		next_read;
	quicktime_atom		*stack,*top;
	int			(*read)(void *a,void *b,size_t c);
	int64_t			(*seek)(void *a,int64_t b);
	int64_t			(*filesize)(void *a);
	void*			mem_base;
	size_t			mem_size;
	size_t			mem_seek;
	void*			user;
} quicktime_stack;

#pragma pack(push,1)
typedef struct quicktime_matrix {
	quicktime_32_fixed_pt_16_16	a,b;
	quicktime_32_fixed_pt_2_30	u;

	quicktime_32_fixed_pt_16_16	c,d;
	quicktime_32_fixed_pt_2_30	v;

	quicktime_32_fixed_pt_16_16	x,y;
	quicktime_32_fixed_pt_2_30	w;
} quicktime_matrix;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct quicktime_mvhd_header {
	uint8_t				version;
	uint8_t				flags[3];
	uint32_t			creation_time;
	uint32_t			modification_time;
	uint32_t			time_scale;
	uint32_t			duration;
	quicktime_32_fixed_pt_16_16	preferred_rate;
	quicktime_16_fixed_pt_8_8	preferred_volume;
	uint8_t				reserved[10];
	quicktime_matrix		matrix;
	uint32_t			preview_time;
	uint32_t			preview_duration;
	uint32_t			poster_time;
	uint32_t			selection_time;
	uint32_t			selection_duration;
	uint32_t			current_time;
	uint32_t			next_track_id;
} quicktime_mvhd_header;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct quicktime_tkhd_header {
	uint8_t				version;
	uint8_t				flags[3];
	uint32_t			creation_time;
	uint32_t			modification_time;
	uint32_t			track_id;
	uint32_t			reserved1;
	uint32_t			duration;
	uint64_t			reserved2;
	uint16_t			layer;
	uint16_t			alternate_group;
	uint16_t			volume;
	uint16_t			reserved3;
	quicktime_matrix		matrix;
	uint32_t			track_width;
	uint32_t			track_height;
} quicktime_tkhd_header;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct quicktime_mdhd_header {
	uint8_t				version;
	uint8_t				flags[3];
	uint32_t			creation_time;
	uint32_t			modification_time;
	uint32_t			time_scale;
	uint32_t			duration;
	uint16_t			language;
	uint16_t			quality;
} quicktime_mdhd_header;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct quicktime_hdlr_header {
	uint8_t				version;
	uint8_t				flags[3];
	quicktime_atom_type_t		component_type;
	quicktime_atom_type_t		component_subtype;
	quicktime_atom_type_t		component_manufacturer;
	uint32_t			component_flags;
	uint32_t			component_flags_mask;
	/* component name, ASCII string */
} quicktime_hdlr_header;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct quicktime_dref_header {
	uint8_t				version;
	uint8_t				flags[3];
	uint32_t			number_of_entries;
	/* dref entries */
} quicktime_dref_header;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct quicktime_dref_entry {
	uint32_t			size;
	quicktime_atom_type_t		type;
	uint8_t				version;
	uint8_t				flags[3];
} quicktime_dref_entry;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct quicktime_stsd_header {
	uint8_t				version;
	uint8_t				flags[3];
	uint32_t			number_of_entries;
	/* stsd entries */
} quicktime_stsd_header;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct quicktime_stsd_entry_general {
	uint32_t			size;
	quicktime_atom_type_t		data_format;
	uint32_t			reserved1;
	uint16_t			reserved2;
	uint16_t			data_reference_index;
} quicktime_stsd_entry_general;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct quicktime_stsd_entry_video {
	quicktime_stsd_entry_general	general;
	uint16_t			version;
	uint16_t			revision;
	quicktime_atom_type_t		vendor;
	uint32_t			temporal_quality;
	uint32_t			spatial_quality;
	uint16_t			width,height;
	uint32_t			horizontal_resolution,vertical_resolution;
	uint32_t			data_size;	/* "which must be set to 0" why? */
	uint16_t			frame_count;	/* NOTE: per sample */
	uint8_t				compressor_name[32]; /* NOTE: Pascal-style string */
	uint16_t			depth;		/* NOTE: 0-31 for color, 32-63 to mean 0-31 grayscale */
	uint16_t			color_table_id;	/* NOTE: -1 to use default, 0 = color table follows */
} quicktime_stsd_entry_video;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct quicktime_stsd_entry_audio {
	quicktime_stsd_entry_general	general;
	uint16_t			version;	/* == 0 */
	uint16_t			revision;
	quicktime_atom_type_t		vendor;
	uint16_t			number_of_channels;
	uint16_t			sample_size;
	uint16_t			compression_id;
	uint16_t			packet_size;
	quicktime_32_fixed_pt_16_16	sample_rate;
} quicktime_stsd_entry_audio;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct quicktime_stsd_entry_audio_v1 {
	quicktime_stsd_entry_audio	v0;
	uint32_t			samplesPerPacket;
	uint32_t			bytesPerPacket;
	uint32_t			bytesPerFrame;
	uint32_t			bytesPerSample;
} quicktime_stsd_entry_audio_v1;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct quicktime_stts_header {
	uint8_t				version;
	uint8_t				flags[3];
	uint32_t			number_of_entries;
	/* stts entries */
} quicktime_stts_header;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct quicktime_stts_entry {
	uint32_t			count,duration;
} quicktime_stts_entry;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct quicktime_stsc_header {
	uint8_t				version;
	uint8_t				flags[3];
	uint32_t			number_of_entries;
	/* stts entries */
} quicktime_stsc_header;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct quicktime_stsc_entry {
	/* WARNING: first_chunk is 1-based, not zero based */
	uint32_t			first_chunk,samples_per_chunk,id;
} quicktime_stsc_entry;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct quicktime_stsz_header {
	uint8_t				version;
	uint8_t				flags[3];
	uint32_t			sample_size;
	uint32_t			number_of_entries; /* NTS: FFMPEG seems to store the total samples here if sample_size != 0 */
	/* array of uint32_t[] with sample sizes */
} quicktime_stsz_header;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct quicktime_stco_header {
	uint8_t				version;
	uint8_t				flags[3];
	uint32_t			number_of_entries;
	/* array of uint32_t[] with chunk offsets */
} quicktime_stco_header;
#pragma pack(pop)

typedef struct quicktime_cmvd_copy {
	void*			buffer;
	size_t			buffer_len;
	quicktime_stack*	stack;
} quicktime_cmvd_copy;

#pragma pack(push,1)
typedef struct quicktime_stss_header {
	uint8_t				version;
	uint8_t				flags[3];
	uint32_t			number_of_entries;
} quicktime_stss_header;
#pragma pack(pop)

quicktime_stack *quicktime_stack_create(int depth);
void quicktime_stack_empty(quicktime_stack *s);
quicktime_stack *quicktime_stack_destroy(quicktime_stack *s);
quicktime_atom *quicktime_stack_pop(quicktime_stack *s);
int quicktime_stack_push(quicktime_stack *s,quicktime_atom *a);
quicktime_atom *quicktime_stack_top(quicktime_stack *s);
int quicktime_stack_assign_fd(quicktime_stack *s,int fd);
int quicktime_stack_assign_mem(quicktime_stack *s,void *buffer,size_t len);
int64_t quicktime_stack_seek(quicktime_stack *s,quicktime_atom *a,int64_t offset);
int quicktime_stack_read(quicktime_stack *s,quicktime_atom *a,void *buffer,size_t len);
int quicktime_stack_auto_identify_atom_subchunks(quicktime_atom *a);
int quicktime_stack_read_atom(quicktime_stack *s,quicktime_atom *pa,quicktime_atom *a);
int quicktime_stack_force_chunk_subchunks(quicktime_stack *s,quicktime_atom *a,int force,uint32_t offset);
int quicktime_stack_eof(quicktime_stack *s);
void quicktime_stack_type_to_string(quicktime_atom_type_t a,char *t);
void quicktime_stack_debug_print_atom(FILE *fp,int current,quicktime_atom *a);
void quicktime_stack_debug_chunk_dump(FILE *fp,quicktime_stack *riff,quicktime_atom *chunk);
int quicktime_stack_pstring_to_cstring(char *dst,size_t dstmax,const unsigned char *src,size_t len);
void *quicktime_stack_decompress_cmvd_zlib(quicktime_stack *qt,quicktime_atom *atom,size_t dst_len);
void *quicktime_stack_decompress_cmvd(quicktime_stack *qt,quicktime_atom *atom,quicktime_atom_type_t dcom_type,size_t *uncom_size);
void quicktime_stack_decompressed_cmvd_free(void *buf);
quicktime_cmvd_copy *quicktime_cmvd_copy_destroy(quicktime_cmvd_copy *c);
quicktime_cmvd_copy *quicktime_stack_decompress_and_get_cmvd(quicktime_stack *s,quicktime_atom *a,quicktime_atom_type_t dcom_type);

#ifdef __cplusplus
}
#endif

