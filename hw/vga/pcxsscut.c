
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef O_BINARY
#define O_BINARY (0)
#endif

#pragma pack(push,1)
struct vrl_header {
	uint8_t			vrl_sig[4];		// +0x00  "VRL1"
	uint8_t			fmt_sig[4];		// +0x04  "VGAX"
	uint16_t		height;			// +0x08  Sprite height
	uint16_t		width;			// +0x0A  Sprite width
	int16_t			hotspot_x;		// +0x0C  Hotspot offset (X) for programmer's reference
	int16_t			hotspot_y;		// +0x0E  Hotspot offset (Y) for programmer's reference
};							// =0x10
#pragma pack(pop)

#pragma pack(push,1)
struct pcx_header {
	uint8_t			manufacturer;		// +0x00  always 0x0A
	uint8_t			version;		// +0x01  0, 2, 3, or 5
	uint8_t			encoding;		// +0x02  always 0x01 for RLE
	uint8_t			bitsPerPlane;		// +0x03  bits per pixel in each color plane (1, 2, 4, 8, 24)
	uint16_t		Xmin,Ymin,Xmax,Ymax;	// +0x04  window (image dimensions). Pixel count in each dimension is Xmin <= x <= Xmax, Ymin <= y <= Ymax i.e. INCLUSIVE
	uint16_t		VertDPI,HorzDPI;	// +0x0C  vertical/horizontal resolution in DPI
	uint8_t			palette[48];		// +0x10  16-color or less color palette
	uint8_t			reserved;		// +0x40  reserved, set to zero
	uint8_t			colorPlanes;		// +0x41  number of color planes
	uint16_t		bytesPerPlaneLine;	// +0x42  number of bytes to read for a single plane's scanline (uncompressed, apparently)
	uint16_t		palType;		// +0x44  palette type (1 = color)
	uint16_t		hScrSize,vScrSize;	// +0x46  scrolling?
	uint8_t			pad[54];		// +0x4A  padding
};							// =0x80
#pragma pack(pop)

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

struct pcx_cutregion_t {
	uint16_t		x,y,w,h;
	uint16_t		sprite_id;
	char			sprite_name[9];		// 8 chars + NUL
};

#define MAX_CUTREGIONS		1024
struct pcx_cutregion_t		cutregion[MAX_CUTREGIONS];
int				cutregions = 0;

static void chomp(char *line) {
	char *s = line+strlen(line)-1;
	while (s >= line && (*s == '\n' || *s == '\r')) *s-- = 0;
}

static void take_item(struct pcx_cutregion_t *item) {
	unsigned int i;

	if (cutregions < MAX_CUTREGIONS) {
		if (item->sprite_id == 0)
			item->sprite_id = MAX_CUTREGIONS + cutregions; // auto-assign
		if (item->sprite_name[0] == 0)
			sprintf(item->sprite_name,"____%04u",item->sprite_id); // auto-assign

		// warn if sprite ID or sprite name already taken!
		for (i=0;i < cutregions;i++) {
			if (cutregion[i].sprite_id == item->sprite_id)
				fprintf(stderr,"WARNING for %s: sprite ID %u already taken by %s\n",
					item->sprite_name,item->sprite_id,cutregion[i].sprite_name);
			if (!strcasecmp(cutregion[i].sprite_name,item->sprite_name))
				fprintf(stderr,"WARNING for %s: sprite name already taken\n",
					item->sprite_name);
		}

		if (item->w != 0 && item->h != 0)
			cutregion[cutregions++] = *item;
	}

	memset(item,0,sizeof(*item));
}

static int parse_script_file(const char *path) {
	unsigned char start_item = 0;
	unsigned char in_section = 0;
	struct pcx_cutregion_t item;
	unsigned char OK = 1;
	char line[512],*s,*d;
	FILE *fp;

	fp = fopen(path,"r");
	if (!fp) return 0;

	memset(&item,0,sizeof(item));
	while (!feof(fp) && !ferror(fp)) {
		if (fgets(line,sizeof(line)-1,fp) == NULL) break;

		/* eat the newline at the end. "chomp" refers to the function of the same name in Perl */
		chomp(line);

		/* begin scanning */
		s = line;
		if (*s == 0/*end of string*/ || *s == '#'/*comment*/) continue;
		while (*s == ' ' || *s == '\t') s++;

		if (*s == '*') { // beginning of section
			if (start_item) {
				take_item(&item);
				start_item = 0;
			}

			s++;
			if (!strcmp(s,"spritesheet")) {
				in_section = 1; // we're in the spritesheet section. this is the part we care about!
			}
			else {
				in_section = 0; // ignore the rest.
			}
		}
		else if (*s == '+') { // starting another item
			s++;
			if (in_section) {
				if (start_item)
					take_item(&item);

				if (cutregions >= MAX_CUTREGIONS) {
					fprintf(stderr,"Too many cut regions!\n");
					OK = 0;
					break;
				}

				// New sprite (no ID or name)
				// +
				//
				// New sprite with name (8 char max)
				// +PRUSSIA
				//
				// New sprite with ID
				// +@400
				//
				// New sprite with name and ID
				// +PRUSSIA@400
				//
				// ID part can be octal or hexadecimal according to strtoul() rules
				// +PRUSSIA@0x158
				// +PRUSSIA@0777
				start_item = 1;
				while (*s == ' ') s++;
				d = item.sprite_name;

				// sprite name (optional)
				while (*s != 0 && *s != '@') {
					if (d < (item.sprite_name+8) && (isdigit(*s) || isalpha(*s) || *s == '_' || *s == '+'))
						*d++ = *s++;
					else
						s++;
				}
				*d = 0;//ASCIIZ snip
				// @sprite ID
				if (*s == '@') {
					s++;
					item.sprite_id = (uint16_t)strtoul(s,&s,0); // whatever base you want, and set 's' to point after number
				}
			}
		}
		else {
			if (start_item) {
				// item params are name=value
				char *name = s;
				char *value = strchr(s,'=');
				if (value != NULL) {
					*value++ = 0; // snip

					if (!strcmp(name,"xy")) {
						// xy=x,y
						item.x = (uint16_t)strtoul(value,&value,0);
						if (*value == ',') {
							value++;
							item.y = (uint16_t)strtoul(value,&value,0);
						}
					}
					else if (!strcmp(name,"wh")) {
						// wh=w,h
						item.w = (uint16_t)strtoul(value,&value,0);
						if (*value == ',') {
							value++;
							item.h = (uint16_t)strtoul(value,&value,0);
						}
					}
					else if (!strcmp(name,"x")) {
						// x=x
						item.x = (uint16_t)strtoul(value,&value,0);
					}
					else if (!strcmp(name,"y")) {
						// y=y
						item.y = (uint16_t)strtoul(value,&value,0);
					}
					else if (!strcmp(name,"w")) {
						// w=w
						item.w = (uint16_t)strtoul(value,&value,0);
					}
					else if (!strcmp(name,"h")) {
						// h=h
						item.h = (uint16_t)strtoul(value,&value,0);
					}
				}
			}
		}
	}

	if (start_item) {
		take_item(&item);
		start_item = 0;
	}

	fclose(fp);
	return OK;
}

static void help() {
	fprintf(stderr,"PCXSSCUT VGA Mode X sprite sheet splitter (C) 2016 Jonathan Campbell\n");
	fprintf(stderr,"PCX file must be 256-color format with VGA palette.\n");
	fprintf(stderr,"Program will emit multiple VRL files as directed by sprite sheet file\n");
	fprintf(stderr,"to the CURRENT WORKING DIRECTORY. Unless -y is specified, program will\n");
	fprintf(stderr,"exit with error if the VRL file already exists.\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"pcxsscut [options]\n");
	fprintf(stderr,"  -s <filename>                File on how to cut the sprite sheet\n");
	fprintf(stderr,"  -i <filename>                Read image from PCX file\n");
	fprintf(stderr,"  -tc <index>                  Specify transparency color\n");
	fprintf(stderr,"  -p <filename>                Write PCX palette to file\n");
	fprintf(stderr,"  -y                           Always overwrite (careful!)\n");
}

int main(int argc,char **argv) {
	const char *src_file = NULL,*scr_file = NULL,*pal_file = NULL;
	unsigned int x,y,runcount,skipcount,cut;
	struct pcx_cutregion_t *cutreg;
	unsigned char y_overwrite = 0;
	unsigned char *s,*d,*dfence;
	struct vrl_header hdr;
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

