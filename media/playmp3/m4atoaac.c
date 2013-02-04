#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>

#include "qtreader.h"

int main(int argc,char **argv) {
	quicktime_reader *qtreader;
	int dump_indx;
	int fd;

	if (argc < 3) {
		fprintf(stderr,"qtdump_stream <quicktime file> <stream index>\n");
		return 1;
	}

#if (defined(WIN32) && defined(_MSC_VER)) || defined(TARGET_MSDOS)
	setmode(1,O_BINARY);
#endif

	fd = open(argv[1],O_RDONLY|O_BINARY);
	if (fd < 0) {
		fprintf(stderr,"Cannot open %s\n",argv[1]);
		return 1;
	}

	dump_indx = atoi(argv[2]);

	if ((qtreader = quicktime_reader_create()) == NULL) {
		fprintf(stderr,"Cannot create QT reader\n");
		return 1;
	}
	quicktime_reader_be_verbose(qtreader);
	if (!quicktime_reader_assign_fd(qtreader,fd)) {
		fprintf(stderr,"Cannot assign reader to file\n");
		return 1;
	}
	if (!quicktime_reader_parse(qtreader)) {
		fprintf(stderr,"Cannot open quicktime_reader_parse() file\n");
		return 1;
	}

	/* copy the stream by running through each chunk and copying down the samples */
	if (dump_indx < 0 || dump_indx >= qtreader->max_track) {
		fprintf(stderr,"Stream index out of range\n");
		return 1;
	}

	{
		int rate = -1;
		int channels = -1;
		int ratecode = -1;
		unsigned char buffer[32768];
		unsigned long chunk,sample,ssize=0,smax,scount;
		quicktime_reader_stsc_entry *ent;
		quicktime_reader_track *trk = qtreader->track + dump_indx;
		int trd;

		if (__be_u32(&trk->mhlr_header.component_subtype) != quicktime_type_const('s','o','u','n')) {
			fprintf(stderr,"This is not audio\n");
			return 1;
		}

		/* what's the format? */
		{
			quicktime_stack_empty(qtreader->r_moov.qt);
			quicktime_stack_push(qtreader->r_moov.qt,&trk->stsd);
			memset(buffer,0,sizeof(buffer));
			quicktime_stack_read(qtreader->r_moov.qt,quicktime_stack_top(qtreader->r_moov.qt),buffer,sizeof(buffer));
			quicktime_stack_empty(qtreader->r_moov.qt);

			{
				unsigned char *p = (unsigned char *)buffer;
				unsigned char *fence = p + sizeof(buffer) - 4096;
				quicktime_stsd_header *h = (quicktime_stsd_header*)buffer;
				unsigned long ent,max = __be_u32(&h->number_of_entries);
				p += sizeof(*h);
				assert(p < fence);
				if (max > 64) {
					fprintf(stderr,"The header seems to have way to many sample descriptions\n");
					return 1;
				}

				for (ent=0;ent < max && (p+32) < fence;ent++) {
					quicktime_stsd_entry_general *en;
					quicktime_stsd_entry_audio *aen;

					en = (quicktime_stsd_entry_general*)p;
					p += __be_u32(&en->size);

					/* must be "mp4a" */
					if (__be_u32(&en->data_format) != quicktime_type_const('m','p','4','a'))
						continue;

					aen = (quicktime_stsd_entry_audio*)en;

					rate = __be_u32(&aen->sample_rate) >> 16UL;
					channels = __be_u16(&aen->number_of_channels);
					break;
				}
			}
		}

		if (rate < 1000 || channels < 1 || channels > 2) {
			fprintf(stderr,"Invalid or missing rate = %d channels = %d\n",
				rate,channels);
			return 1;
		}

		switch (rate) {
			case 96000:	ratecode = 0; break;
			case 88200:	ratecode = 1; break;
			case 64000:	ratecode = 2; break;
			case 48000:	ratecode = 3; break;
			case 44100:	ratecode = 4; break;
			case 32000:	ratecode = 5; break;
			case 24000:	ratecode = 6; break;
			case 22050:	ratecode = 7; break;
			case 16000:	ratecode = 8; break;
			case 12000:	ratecode = 9; break;
			case 11025:	ratecode = 10; break;
			case 8000:	ratecode = 11; break;
			case 7350:	ratecode = 12; break;
		};

		if (ratecode < 0) {
			fprintf(stderr,"Cannot match ratecode\n");
			return 1;
		}

		if (trk->samplesize.constval != 0)
			ssize = trk->samplesize.constval;

		fprintf(stderr,"----- now ripping track %u -----\n",dump_indx);
		for (chunk=0;chunk < trk->chunk.total;chunk++) {
			uint64_t offset = trk->chunk.list[chunk];
			ent = trk->sampletochunk.list + chunk;
			smax = ent->samples_per_chunk;
			sample = ent->first_sample;
			if (smax == 0) {
				fprintf(stderr,"Warning: sample %lu chunk %lu samples/chunk is zero\n",sample,chunk);
				continue;
			}

			for (scount=0;scount < smax;scount++) {
				if (trk->samplesize.constval == 0) {
					if (trk->samplesize.list != NULL && sample < trk->samplesize.entries)
						ssize = trk->samplesize.list[sample];
					else
						ssize = 0;
				}

				if (ssize == 0) {
					fprintf(stderr,"Warning: sample %lu chunk %lu is zero-length, skipping\n",sample,chunk);
					continue;
				}

				if (scount == 0 || smax < 8)
					fprintf(stderr,"chunk[%lu] sample/chunk=%lu sample=%lu ssize=%lu offset=%llu\n",chunk,smax,sample,ssize,
						(unsigned long long)offset);

				if (quicktime_stack_seek(qtreader->qt,NULL,offset) != (int64_t)offset) {
					fprintf(stderr,"   cannot seek to %llu\n",(unsigned long long)offset);
					break;
				}

				/* emit ADTS */
				buffer[0]  = 0xFF;	/* sync */
				buffer[1]  = 0xF8 | 1;	/* sync, id, layer, protection */
				buffer[2]  = 0 << 6;	/* object type */
				buffer[2] |= (ratecode << 2) | (1 << 6); /* 1=LC profile 0=main profile */
				/* NTS: If the profile is wrong (LC AAC when we mark it Main profile) some programs like
				        WinAMP will outright refuse to play the AAC stream) */
				buffer[2] |= channels >> 2;
				buffer[3]  = channels << 6;
				buffer[3] |= (ssize+7) >> 11;
				buffer[4]  = (unsigned char)((ssize+7) >> 3);
				buffer[5]  = (unsigned char)((ssize+7) << 5);
				buffer[5] |= 0x1F;	/* buffer fullness (0x7FF = VBR) */
				buffer[6]  = 0xFC;	/* buffer fullness, and lower 2 bits (0 = one raw ADTS frame) */
				if (write(1,buffer,7) != 7) {
					fprintf(stderr,"Problem writing ADTS header\n");
					abort();
				}

				trd = (int)ssize;
				while (trd >= (int)sizeof(buffer)) {
					if (quicktime_stack_read(qtreader->qt,NULL,buffer,sizeof(buffer)) != (int)sizeof(buffer)) {
						fprintf(stderr,"  cannot fully read %lu bytes\n",(unsigned long)sizeof(buffer));
						break;
					}
					trd -= sizeof(buffer);
					if (write(1,buffer,sizeof(buffer)) != sizeof(buffer)) {
						fprintf(stderr,"Problem writing\n");
						abort();
					}
				}
				if (trd > 0) {
					if (quicktime_stack_read(qtreader->qt,NULL,buffer,trd) != trd) {
						fprintf(stderr,"  cannot fully read %lu bytes\n",(unsigned long)sizeof(buffer));
						break;
					}
					if (write(1,buffer,trd) != trd) {
						fprintf(stderr,"Problem writing\n");
						abort();
					}
				}

				offset += ssize;
				sample++;
			}
		}
	}

	qtreader = quicktime_reader_destroy(qtreader);
	close(fd);
	return 0;
}

