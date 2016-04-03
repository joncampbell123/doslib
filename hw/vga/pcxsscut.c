
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vrl.h"
#include "pcxfmt.h"
#include "comshtps.h"

#ifndef O_BINARY
#define O_BINARY (0)
#endif

static unsigned char		transparent_color = 0;

static unsigned char*		src_pcx = NULL;
static unsigned char		*src_pcx_start,*src_pcx_end;
static unsigned int		src_pcx_size = 0;
static unsigned int		src_pcx_stride = 0;
static unsigned int		src_pcx_width = 0;
static unsigned int		src_pcx_height = 0;

static unsigned char		out_strip[(256*3)+16];
static unsigned int		out_strip_height = 0;
static unsigned int		out_strips = 0;

static void help() {
	fprintf(stderr,"PCXSSCUT VGA Mode X sprite sheet splitter (C) 2016 Jonathan Campbell\n");
	fprintf(stderr,"PCX file must be 256-color format with VGA palette.\n");
	fprintf(stderr,"Program will emit multiple VRL files as directed by sprite sheet file\n");
	fprintf(stderr,"to the CURRENT WORKING DIRECTORY. Unless -y is specified, program will\n");
	fprintf(stderr,"exit with error if the VRL file already exists.\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"pcxsscut [options]\n");
	fprintf(stderr,"  -hp <string>                 With -hc, prefix for sprite defines\n");
	fprintf(stderr,"  -hc <filename>               Emit sprite names and IDs to C header\n");
	fprintf(stderr,"  -s <filename>                File on how to cut the sprite sheet\n");
	fprintf(stderr,"  -i <filename>                Read image from PCX file\n");
	fprintf(stderr,"  -tc <index>                  Specify transparency color\n");
	fprintf(stderr,"  -p <filename>                Write PCX palette to file\n");
	fprintf(stderr,"  -y                           Always overwrite (careful!)\n");
}

int main(int argc,char **argv) {
	const char *src_file = NULL,*scr_file = NULL,*pal_file = NULL,*hdr_file = NULL,*hdr_prefix = NULL;
	unsigned int x,y,runcount,skipcount,cut;
	struct vrl_spritesheetentry_t *cutreg;
	unsigned char y_overwrite = 0;
	unsigned char *s,*d,*dfence;
	struct vrl1_vgax_header hdr;
	char tmpname[14];
	const char *a;
	int i,fd;

	for (i=1;i < argc;) {
		a = argv[i++];
		if (*a == '-') {
			do { a++; } while (*a == '-');

			if (!strcmp(a,"h") || !strcmp(a,"help")) {
				help();
				return 1;
			}
			else if (!strcmp(a,"hp")) {
				hdr_prefix = argv[i++];
			}
			else if (!strcmp(a,"hc")) {
				hdr_file = argv[i++];
			}
			else if (!strcmp(a,"y")) {
				y_overwrite = 1;
			}
			else if (!strcmp(a,"i")) {
				src_file = argv[i++];
			}
			else if (!strcmp(a,"s")) {
				scr_file = argv[i++];
			}
			else if (!strcmp(a,"p")) {
				pal_file = argv[i++];
			}
			else if (!strcmp(a,"tc")) {
				transparent_color = (unsigned char)strtoul(argv[i++],NULL,0);
			}
			else {
				fprintf(stderr,"Unknown switch '%s'. Use --help\n",a);
				return 1;
			}
		}
		else {
			fprintf(stderr,"Unknown param %s\n",a);
			return 1;
		}
	}

	if (src_file == NULL || scr_file == NULL) {
		help();
		return 1;
	}

	/* load the PCX into memory */
	/* WARNING: 16-bit DOS builds of this code cannot load a PCX that decodes to more than 64K - 800 bytes */
	fd = open(src_file,O_RDONLY|O_BINARY);
	if (fd < 0) {
		fprintf(stderr,"Cannot open source file '%s', %s\n",src_file,strerror(errno));
		return 1;
	}
	{
		unsigned long sz = lseek(fd,0,SEEK_END);
		if (sz < (128+769)) {
			fprintf(stderr,"File is too small to be PCX\n");
			return 1;
		}
		if (sizeof(unsigned int) == 2 && sz > 65530UL) {
			fprintf(stderr,"File is too large to load into memory\n");
			return 1;
		}

		src_pcx_size = (unsigned int)sz;
		src_pcx = malloc(src_pcx_size);
		if (src_pcx == NULL) {
			fprintf(stderr,"Cannot malloc for source PCX\n");
			return 1;
		}
	}
	lseek(fd,0,SEEK_SET);
	if ((unsigned int)read(fd,src_pcx,src_pcx_size) != src_pcx_size) {
		fprintf(stderr,"Cannot read PCX\n");
		return 1;
	}
	close(fd);
	src_pcx_start = src_pcx + 128;
	src_pcx_end = src_pcx + src_pcx_size;

	/* parse header */
	{
		struct pcx_header *hdr = (struct pcx_header*)src_pcx;

		if (hdr->manufacturer != 0xA || hdr->encoding != 1 || hdr->bitsPerPlane != 8 ||
			hdr->colorPlanes != 1 || hdr->Xmin >= hdr->Xmax || hdr->Ymin >= hdr->Ymax) {
			fprintf(stderr,"PCX format not supported\n");
			return 1;
		}
		src_pcx_stride = hdr->bytesPerPlaneLine;
		src_pcx_width = hdr->Xmax + 1 - hdr->Xmin;
		src_pcx_height = hdr->Ymax + 1 - hdr->Ymin;
		if (src_pcx_width >= 4096 || src_pcx_height >= 4096) {
			fprintf(stderr,"PCX too big\n");
			return 1;
		}
		if (src_pcx_stride < src_pcx_width) {
			fprintf(stderr,"PCX stride < width\n");
			return 1;
		}
	}

	/* identify and load palette */
	if (src_pcx_size > 769) {
		s = src_pcx + src_pcx_size - 769;
		if (*s == 0x0C) {
			src_pcx_end = s++;
			if (pal_file != NULL) {
				fd = open(pal_file,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0644);
				if (fd < 0) {
					fprintf(stderr,"Cannot create file '%s', %s\n",pal_file,strerror(errno));
					return 1;
				}
				write(fd,s,768);
				close(fd);
			}
		}
	}

	/* decode the PCX */
	{
		unsigned char b,run;
		unsigned char *tmp = malloc(src_pcx_stride * src_pcx_height);
		if (tmp == NULL) {
			fprintf(stderr,"Cannot allocate decode buffer\n");
			return 1;
		}

		d = tmp;
		dfence = tmp + (src_pcx_stride * src_pcx_height);
		s = src_pcx_start;
		while (s < src_pcx_end && d < dfence) {
			b = *s++;
			if ((b & 0xC0) == 0xC0) {
				run = b & 0x3F;
				if (s >= src_pcx_end) break;
				b = *s++;
				while (run > 0) {
					*d++ = b;
					run--;
					if (d >= dfence) break;
				}
			}
			else {
				*d++ = b;
			}
		}

		/* discard source PCX data */
		free(src_pcx);
		src_pcx = tmp;
	}

	/* read the script file */
	if (!parse_script_file(scr_file)) {
		fprintf(stderr,"Script file (-s) parse error\n");
		return 1;
	}

	if (hdr_file != NULL) {
		FILE *fp;

		fp = fopen(hdr_file,"w");
		if (fp == NULL) {
			fprintf(stderr,"Failed to open -hc file\n");
			return 1;
		}

		fprintf(fp,"// header file for sprite sheet. AUTO GENERATED, do not edit\n");
		fprintf(fp,"// \n");
		fprintf(fp,"// source PCX: %s\n",src_file);
		fprintf(fp,"// sheet script: %s\n",scr_file);
		if (pal_file != NULL) fprintf(fp,"// palette file: %s\n",pal_file);
		fprintf(fp,"\n");

		for (cut=0;cut < cutregions;cut++) {
			cutreg = cutregion+cut;

			fprintf(fp,"#define %s%s %uU\n",
				hdr_prefix != NULL ? hdr_prefix : "",
				cutreg->sprite_name,
				cutreg->sprite_id);
		}

		fprintf(fp,"\n");
		fprintf(fp,"// end list\n");
		fclose(fp);
	}

	if (cutregions == 0) {
		fprintf(stderr,"No sprite regions to cut\n");
		return 0; // not an error, just sayin'
	}

	for (cut=0;cut < cutregions;cut++) {
		cutreg = cutregion+cut;

		printf("Processing spritesheet cut %s@%u at (%u,%u) dim (%u,%u) out of PCX (%u,%u)\n",
			cutreg->sprite_name,
			cutreg->sprite_id,
			cutreg->x,
			cutreg->y,
			cutreg->w,
			cutreg->h,
			src_pcx_width,
			src_pcx_height);

		if (cutreg->w == 0 || cutreg->h == 0) {
			fprintf(stderr,"cut region is NULL size\n");
			return 1;
		}
		if (cutreg->sprite_name[0] == 0) {
			fprintf(stderr,"cut region has NULL name\n");
			return 1;
		}

		if (cutreg->x >= src_pcx_width || cutreg->y >= src_pcx_height) {
			fprintf(stderr,"cut region x,y out of range (beyond PCX width/height)\n");
			return 1;
		}
		if ((cutreg->x+cutreg->w) > src_pcx_width || (cutreg->y+cutreg->h) > src_pcx_height) {
			fprintf(stderr,"cut region w,h out of range ((x+w) > PCX width or (y+h) > PCX height)\n");
			return 1;
		}

		sprintf(tmpname,"%s.VRL",cutreg->sprite_name);
		fd = open(tmpname,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY|(y_overwrite?0:O_EXCL),0644);
		if (fd < 0) {
			fprintf(stderr,"Cannot create file '%s', %s\n",tmpname,strerror(errno));
			return 1;
		}

		out_strips = cutreg->w;
		out_strip_height = cutreg->h;

		memset(&hdr,0,sizeof(hdr));
		memcpy(hdr.vrl_sig,"VRL1",4); // Vertical Run Length v1
		memcpy(hdr.fmt_sig,"VGAX",4); // VGA mode X
		hdr.height = out_strip_height;
		hdr.width = out_strips;
		write(fd,&hdr,sizeof(hdr));

		for (x=0;x < out_strips;x++) {
			y = 0;
			d = out_strip;
			s = src_pcx + x + cutreg->x + (cutreg->y * src_pcx_stride);
			dfence = out_strip + sizeof(out_strip);
			while (y < out_strip_height) {
				unsigned char *stripstart = d;
				unsigned char color_run = 0;

				d += 2; // patch bytes later
				runcount = 0;
				skipcount = 0;
				while (y < out_strip_height && *s == transparent_color) {
					y++;
					s += src_pcx_stride;
					if ((++skipcount) == 254) break;
				}

				// check: can we do a run length of one color?
				if (y < out_strip_height && *s != transparent_color) {
					unsigned char first_color = *s;
					unsigned char *scan_s = s;
					unsigned int scan_y = y;

					color_run = 1;
					scan_s += src_pcx_stride;
					scan_y++;
					while (scan_y < out_strip_height) {
						if (*scan_s != first_color) break;
						scan_y++;
						scan_s += src_pcx_stride;
						if ((++color_run) == 126) break;
					}

					if (color_run < 3) color_run = 0;

					if (color_run == 0) {
						unsigned char ppixel = transparent_color,same_count = 0;

						scan_s = s;
						scan_y = y;
						while (scan_y < out_strip_height && *scan_s != transparent_color) {
							if (*scan_s == ppixel) {
								if (same_count >= 4) {
									d -= same_count;
									scan_y -= same_count;
									scan_s -= same_count * src_pcx_stride;
									runcount -= same_count;
									break;
								}
								same_count++;
							}
							else {
								same_count=0;
							}

							scan_y++;
							*d++ = ppixel = *scan_s;
							scan_s += src_pcx_stride;
							if ((++runcount) == 126) break;
						}
					}
					else {
						*d++ = first_color;
						runcount = color_run;
					}

					y = scan_y;
					s = scan_s;
				}

				if (runcount == 0 && skipcount == 0) {
					d = stripstart;
				}
				else {
					// overwrite the first byte with run + skip count
					if (color_run != 0) {
						stripstart[0] = runcount + 0x80; // it's a run of one color
						d = stripstart + 3; // it becomes <runcount+0x80> <skipcount> <color to repeat>
					}
					else {
						stripstart[0] = runcount; // <runcount> <skipcount> [run]
					}
					stripstart[1] = skipcount;
				}
			}

			// final byte
			*d++ = 0xFF;
			assert(d <= dfence);
			write(fd,out_strip,(int)(d - out_strip));
		}

		close(fd);
	}

	return 0;
}

