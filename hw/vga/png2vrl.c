/* WARNING: For host systems with libpng only, not (yet) for DOS */

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vrl.h"

#include <png.h>            /* libpng */

#ifndef O_BINARY
#define O_BINARY (0)
#endif

static unsigned char		transparent_color = 0xFF;

static unsigned char*		src_pcx = NULL;
static unsigned int		src_pcx_stride = 0;
static unsigned int		src_pcx_width = 0;
static unsigned int		src_pcx_height = 0;

static unsigned char        tmp[1024];

static unsigned char		out_strip[(256*3)+16];
static unsigned int		out_strip_height = 0;
static unsigned int		out_strips = 0;

static void help() {
	fprintf(stderr,"PNG2VRL VGA Mode X sprite compiler (C) 2020 Jonathan Campbell\n");
	fprintf(stderr,"PNG file must be 256-color paletted with VGA palette.\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"png2vrl [options]\n");
	fprintf(stderr,"  -o <filename>                Write VRL sprite to file\n");
	fprintf(stderr,"  -i <filename>                Read image from PNG file\n");
	fprintf(stderr,"  -tc <index>                  Specify transparency color\n");
	fprintf(stderr,"  -p <filename>                Write PNG palette to file\n");
}

int main(int argc,char **argv) {
	const char *src_file = NULL,*dst_file = NULL,*pal_file = NULL;
    png_structp png_context = NULL;
    png_infop png_context_info = NULL;
    png_infop png_context_end = NULL;
    png_bytep* src_pcx_rows = NULL;
    unsigned int x,y,runcount,skipcount;
	unsigned char *s,*d,*dfence;
	const char *a;
    FILE *fp;
	int i,fd;

    png_uint_32 png_width = 0,png_height = 0;
    int png_bit_depth = 0;
    int png_color_type = 0;
    int png_interlace_method = 0;
    int png_compression_method = 0;
    int png_filter_method = 0;

	for (i=1;i < argc;) {
		a = argv[i++];
		if (*a == '-') {
			do { a++; } while (*a == '-');

			if (!strcmp(a,"h") || !strcmp(a,"help")) {
				help();
				return 1;
			}
			else if (!strcmp(a,"i")) {
				src_file = argv[i++];
			}
			else if (!strcmp(a,"o")) {
				dst_file = argv[i++];
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

	if (src_file == NULL || dst_file == NULL) {
		help();
		return 1;
	}

	/* load the PNG into memory */
    fp = fopen(src_file,"rb");
    if (fp == NULL) return 1;

    png_context = png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL/*error*/,NULL/*error fn*/,NULL/*warn fn*/);
    if (png_context == NULL) return 1;

    png_context_info = png_create_info_struct(png_context);
    if (png_context_info == NULL) return 1;

    png_init_io(png_context, fp);
    png_read_info(png_context, png_context_info);

    if (!png_get_IHDR(png_context, png_context_info, &png_width, &png_height, &png_bit_depth, &png_color_type, &png_interlace_method, &png_compression_method, &png_filter_method))
        return 1;

    if (png_color_type != PNG_COLOR_TYPE_PALETTE) {
        fprintf(stderr,"Input PNG not paletted\n");
        return 1;
    }

    {
        png_color* pal = NULL;
        int pal_count = 0;

        /* FIXME: libpng makes no reference to freeing this. Do you? */
        if (png_get_PLTE(png_context, png_context_info, &pal, &pal_count) == 0) {
            fprintf(stderr,"Unable to get Input PNG palette\n");
            return 1;
        }

        if (pal_file != NULL) {
            unsigned int i;

            for (i=0;i < pal_count;i++) {
                tmp[(i*3)+0] = pal[i].red;
                tmp[(i*3)+1] = pal[i].green;
                tmp[(i*3)+2] = pal[i].blue;
            }
            for (;i < 256;i++) {
                tmp[(i*3)+0] = 0;
                tmp[(i*3)+1] = 0;
                tmp[(i*3)+2] = 0;
            }

            fd = open(pal_file,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0644);
            if (fd < 0) {
                fprintf(stderr,"Cannot create file '%s', %s\n",pal_file,strerror(errno));
                return 1;
            }
            write(fd,tmp,768);
            close(fd);
        }
    }

    /* we gotta preserve alpha transparency too */
    {
        png_color_16p trans_values = 0; /* throwaway value */
        png_bytep trans = NULL;
        int trans_count = 0;

        if (png_get_tRNS(png_context, png_context_info, &trans, &trans_count, &trans_values) != 0) {
            int i;

            for (i=0;i < trans_count;i++) {
                if (trans[i] == 0) {
                    transparent_color = i;
                    break;
                }
            }
        }
    }

    src_pcx_stride = png_width;
    src_pcx_width = png_width;
    src_pcx_height = png_height;
    if (src_pcx_width >= 512 || src_pcx_height > 256) {
        fprintf(stderr,"PNG too big\n");
        return 1;
    }
    if (src_pcx_stride < src_pcx_width) {
        fprintf(stderr,"PNG stride < width\n");
        return 1;
    }

    if (src_pcx_height > 255)
        out_strip_height = 255;
    else
        out_strip_height = src_pcx_height;

    out_strips = src_pcx_width;

    src_pcx = malloc((png_width * png_height) + 4096);
    if (src_pcx == NULL)
        return 1;

    src_pcx_rows = (png_bytep*)malloc(sizeof(png_bytep) * png_height);
    if (src_pcx_rows == NULL)
        return 1;

    {
        unsigned int y;
        for (y=0;y < png_height;y++)
            src_pcx_rows[y] = src_pcx + (y * png_width);
    }

    png_read_rows(png_context, src_pcx_rows, NULL, png_height);

    png_destroy_read_struct(&png_context,&png_context_info,&png_context_end);

    free(src_pcx_rows);
    src_pcx_rows = NULL;

	/* write out */
	fd = open(dst_file,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0644);
	if (fd < 0) {
		fprintf(stderr,"Cannot create file '%s', %s\n",dst_file,strerror(errno));
		return 1;
	}
	{
		struct vrl1_vgax_header hdr;
		memset(&hdr,0,sizeof(hdr));
		memcpy(hdr.vrl_sig,"VRL1",4); // Vertical Run Length v1
		memcpy(hdr.fmt_sig,"VGAX",4); // VGA mode X
		hdr.height = out_strip_height;
		hdr.width = out_strips;
		write(fd,&hdr,sizeof(hdr));

		for (x=0;x < out_strips;x++) {
			y = 0;
			d = out_strip;
			s = src_pcx + x;
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

                if (runcount == 0 && y >= out_strip_height) {
                    /* avoid encoding strips with zero length just to skip to end of column */
                    d = stripstart;
                    break;
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
	}
	close(fd);
	return 0;
}

