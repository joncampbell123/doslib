
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include <png.h>    /* libpng */

static char*            in_png = NULL;
static char*            out_png = NULL;

static unsigned int	want_colors = 256;
static unsigned char	color0_black = 1;

static unsigned int	src_png_bypp = 0;
static unsigned char*   src_png_image = NULL;
static png_bytep*       src_png_image_rows = NULL;

static unsigned char*   gen_png_image = NULL;
static png_bytep*       gen_png_image_rows = NULL;
static png_uint_32      gen_png_width = 0,gen_png_height = 0;
static int              gen_png_bit_depth = 0;
static int              gen_png_color_type = 0;
static int              gen_png_interlace_method = 0;
static int              gen_png_compression_method = 0;
static int              gen_png_filter_method = 0;

static png_color        gen_png_pal[256] = {0};
static int              gen_png_pal_count = 0;

static char             pal_vga = 0;

static void free_src_png(void) {
    if (src_png_image) {
        free(src_png_image);
        src_png_image = NULL;
    }
    if (src_png_image_rows) {
        free(src_png_image_rows);
        src_png_image_rows = NULL;
    }
}

static void free_gen_png(void) {
    if (gen_png_image) {
        free(gen_png_image);
        gen_png_image = NULL;
    }
    if (gen_png_image_rows) {
        free(gen_png_image_rows);
        gen_png_image_rows = NULL;
    }
}

static void help(void) {
    fprintf(stderr,"pnggenpal -i <input PNG> -o <output PNG>\n");
    fprintf(stderr,"Read a non-paletted PNG, generate a palette as PNG,\n");
    fprintf(stderr," -vga        Palettize for VGA (6 bits per RGB)\n");
    fprintf(stderr," -nc <n>     Number of colors (default 256)\n");
    fprintf(stderr," -b0 / -nb0  Color #0 is fixed black (default) / not fixed\n");
}

static int parse_argv(int argc,char **argv) {
    int i = 1;
    char *a;

    while (i < argc) {
        a = argv[i++];

        if (*a == '-') {
            do { a++; } while (*a == '-');

            if (!strcmp(a,"h") || !strcmp(a,"help")) {
                help();
                return 1;
            }
	    else if (!strcmp(a,"i")) {
		    if ((in_png = argv[i++]) == NULL)
			    return 1;
            }
            else if (!strcmp(a,"o")) {
                if ((out_png = argv[i++]) == NULL)
                    return 1;
            }
            else if (!strcmp(a,"nc")) {
                if ((a=argv[i++]) == NULL)
                    return 1;

		want_colors = atoi(a);
                if (want_colors == 0 || want_colors > 256)
                    return 1;
            }
            else if (!strcmp(a,"vga")) {
                pal_vga = 1;
            }
            else if (!strcmp(a,"b0")) {
                color0_black = 1;
            }
            else if (!strcmp(a,"nb0")) {
                color0_black = 0;
            }
            else {
                fprintf(stderr,"Unknown switch %s\n",a);
                return 1;
            }
        }
        else {
            fprintf(stderr,"Unexpected arg %s\n",a);
            return 1;
        }
    }

    if (in_png == NULL) {
        fprintf(stderr,"Input -i png required\n");
        return 1;
    }

    if (out_png == NULL) {
        fprintf(stderr,"Output -o png required\n");
        return 1;
    }

    return 0;
}

static int load_in_png(void) {
    png_structp png_context = NULL;
    png_infop png_context_info = NULL;
    png_infop png_context_end = NULL;
    png_uint_32 png_width = 0,png_height = 0;
    int png_bit_depth = 0;
    int png_color_type = 0;
    int png_interlace_method = 0;
    int png_compression_method = 0;
    int png_filter_method = 0;
    FILE *fp = NULL;
    int ret = 1;

    free_src_png();
    free_gen_png();

    if (in_png == NULL)
        return 1;

    fp = fopen(in_png,"rb");
    if (fp == NULL)
        return 1;

    png_context = png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL/*error*/,NULL/*error fn*/,NULL/*warn fn*/);
    if (png_context == NULL) goto fail;

    png_context_info = png_create_info_struct(png_context);
    if (png_context_info == NULL) goto fail;

    png_init_io(png_context, fp);
    png_read_info(png_context, png_context_info);

    if (!png_get_IHDR(png_context, png_context_info, &png_width, &png_height, &png_bit_depth, &png_color_type, &png_interlace_method, &png_compression_method, &png_filter_method))
        goto fail;

    if (!(png_color_type & PNG_COLOR_MASK_COLOR)) { /* RGB or RGBA is fine */
        fprintf(stderr,"Input PNG not truecolor\n");
        goto fail;
    }

    if (png_width <= 0 || png_width > 4096 || png_height <= 0 || png_height > 4096 || png_bit_depth != 8)
        goto fail;

    src_png_bypp = (png_color_type & PNG_COLOR_MASK_ALPHA) ? 4 : 3;

    src_png_image = malloc((png_width * png_height * src_png_bypp) + 4096);
    if (src_png_image == NULL)
        goto fail;

    src_png_image_rows = (png_bytep*)malloc(sizeof(png_bytep) * png_height);
    if (src_png_image_rows == NULL)
        goto fail;

    {
        unsigned int y;
        for (y=0;y < png_height;y++)
            src_png_image_rows[y] = src_png_image + (y * png_width * src_png_bypp);
    }

    gen_png_image = malloc((png_width * src_png_bypp * png_height) + 4096);
    if (gen_png_image == NULL)
        goto fail;

    gen_png_image_rows = (png_bytep*)malloc(sizeof(png_bytep) * png_height);
    if (gen_png_image_rows == NULL)
        goto fail;

    {
        unsigned int y;
        for (y=0;y < png_height;y++)
            gen_png_image_rows[y] = gen_png_image + (y * png_width * src_png_bypp);
    }

    png_read_rows(png_context, src_png_image_rows, NULL, png_height);

    gen_png_width = png_width;
    gen_png_height = png_height;
    gen_png_bit_depth = png_bit_depth;
    gen_png_color_type = png_color_type;
    gen_png_interlace_method = png_interlace_method;
    gen_png_compression_method = png_compression_method;
    gen_png_filter_method = png_filter_method;

    /* success */
    ret = 0;
fail:
    if (png_context != NULL)
        png_destroy_read_struct(&png_context,&png_context_info,&png_context_end);

    if (ret)
        fprintf(stderr,"Failed to load input PNG\n");

    fclose(fp);
    return ret;
}

static inline unsigned char clamp255(int x) {
	return (x > 255) ? 255 : ((x < 0) ? 0 : x);
}

void rgb2yuv(unsigned char *Y,unsigned char *U,unsigned char *V,unsigned char r,unsigned char g,unsigned char b) {
	*Y = clamp255((((r *  66) + (g * 129) + (b *  25) + 128) >> 8) + 16);
	*U = clamp255((((r * -38) + (g * -74) + (b * 112) + 128) >> 8) + 128);
	*V = clamp255((((r * 112) + (g * -94) + (b * -18) + 128) >> 8) + 128);
}

struct color_bucket {
	unsigned int			count;
	unsigned char			R,G,B,A;
	struct color_bucket*		next;
};

void color_buckets_init(struct color_bucket **buckets,unsigned int num) {
	unsigned int x;

	for (x=0;x < num;x++) buckets[x] = NULL;
}

void color_bucket_free(struct color_bucket **b) {
	while (*b != NULL) {
		struct color_bucket *n = (*b)->next;
		free(*b);
		*b = n;
	}
}

void color_buckets_free(struct color_bucket **buckets,unsigned int num) {
	unsigned int x;

	for (x=0;x < num;x++) color_bucket_free(&buckets[x]);
}

/* NTS: It turns out taking the time to check for existing duplicates HERE is faster than sorting through all the
 *      duplicates later. */
void color_bucket_add(struct color_bucket **buckets,unsigned int num,unsigned int idx,struct color_bucket *nb) {
	if (idx < num && nb->next == NULL) {
		if (buckets[idx] != NULL) {
			struct color_bucket *s = buckets[idx];
			while (s != NULL) {
				if (s->R == nb->R && s->G == nb->G && s->B == nb->B && s->A == nb->A) {
					/* we're expected to insert it, if we do not, then we must free it, else a memory leak occurs */
					s->count++;
					free(nb);
					return;
				}

				if (s->next != NULL) {
					s = s->next;
				}
				else {
					/* end of chain, add the new entry to the end */
					nb->count = 1;
					s->next = nb;
					break;
				}
			}
		}
		else {
			buckets[idx] = nb;
			nb->count = 1;
		}
	}
}

unsigned int color_bucket_count(struct color_bucket *b) {
	unsigned int c = 0;

	while (b != NULL) {
		b = b->next;
		c++;
	}

	return c;
}

static inline unsigned int color_bucket_channel_po(unsigned int channel) {
	switch (channel) {
		case 0:		return (unsigned char)offsetof(struct color_bucket,R);
		case 1:		return (unsigned char)offsetof(struct color_bucket,G);
		case 2:		return (unsigned char)offsetof(struct color_bucket,B);
		case 3:		return (unsigned char)offsetof(struct color_bucket,A);
	};

	return 0;
}

static inline unsigned char color_bucket_channel(struct color_bucket *b,unsigned int co) {
	return *((unsigned char*)b + co);
}

static unsigned int color_bucket_lt(struct color_bucket *a,struct color_bucket *b,const unsigned int co[4]) {
	unsigned char ca,cb;
	unsigned int i;

	for (i=0;i < 4;i++) {
		ca = color_bucket_channel(a,co[i]);
		cb = color_bucket_channel(b,co[i]);
		if (ca < cb) return 1;
		if (ca > cb) return 0;
	}

	return 0;
}

// code assumes *d and *s have already been sorted by count
void color_bucket_merge_channel(struct color_bucket **d,struct color_bucket **s,unsigned int channel) {
	const unsigned int co[4] = {
		color_bucket_channel_po( channel       ),
		color_bucket_channel_po((channel+1u)&3u),
		color_bucket_channel_po((channel+2u)&3u),
		color_bucket_channel_po((channel+3u)&3u)
	};

	if (*d == NULL) {
		*d = *s;
		*s = NULL;
	}
	else if (*s != NULL) {
		struct color_bucket **sd = d,*n;

		while (*sd != NULL && *s != NULL) {
			if (color_bucket_lt(*s,*sd,co)) {
				/* remove first from *s */
				n = (*s); *s = (*s)->next;
				/* insert to first in *d */
				n->next = *sd;
				*sd = n;
			}
			else {
				/* advance *sd, or at the end of the list, copy the rest of *s */
				if ((*sd)->next != NULL) {
					sd = &((*sd)->next);
				}
				else {
					(*sd)->next = *s;
					*s = NULL;
					break;
				}
			}
		}
	}
}

void color_bucket_sort_by_channel(struct color_bucket **c,unsigned int channel) {
	const unsigned int co[4] = {
		color_bucket_channel_po( channel       ),
		color_bucket_channel_po((channel+1u)&3u),
		color_bucket_channel_po((channel+2u)&3u),
		color_bucket_channel_po((channel+3u)&3u)
	};
	struct color_bucket *n,*pn;

	while (*c != NULL) {
		pn = *c;
		n = (*c)->next;
		if (n != NULL) {
			while (n != NULL) {
				if (color_bucket_lt(n,*c,co)) {
					/* remove "n" from list */
					pn->next = n->next;
					/* then insert "n" before node *c */
					n->next = *c;
					*c = n;
					break;
				}
				pn = n;
				n = n->next;
				if (n == NULL)
					c = &((*c)->next);
			}
		}
		else {
			c = &((*c)->next);
		}
	}
}

void color_bucket_sort_by_count(struct color_bucket **c) {
	struct color_bucket *n,*pn;

	while (*c != NULL) {
		pn = *c;
		n = (*c)->next;
		if (n != NULL) {
			while (n != NULL) {
				if (n->count > (*c)->count) {
					/* remove "n" from list */
					pn->next = n->next;
					/* then insert "n" before node *c */
					n->next = *c;
					*c = n;
					break;
				}
				pn = n;
				n = n->next;
				if (n == NULL)
					c = &((*c)->next);
			}
		}
		else {
			c = &((*c)->next);
		}
	}
}

static int make_palette() {
#define COLOR_BUCKETS (256u * 8u * 8u)
	unsigned int target_colors = 256;
	struct color_bucket** color_buckets;
	struct color_bucket* nbucket;
	unsigned char minr=0xFF,ming=0xFF,minb=0xFF,mina=0xFF;
	unsigned char maxr=0,maxg=0,maxb=0,maxa=0;
	unsigned int x,y,ci,colors;
	unsigned char selchannel;
	png_bytep row;

	target_colors = want_colors;
	if (color0_black) {
		if (target_colors == 0) return 1;
		target_colors--;
	}

	if (gen_png_color_type == PNG_COLOR_TYPE_PALETTE)
		return 1;
	if (target_colors > 256)
		return 1;

	color_buckets = (struct color_bucket**)malloc(sizeof(struct color_bucket*) * COLOR_BUCKETS);
	if (color_buckets == NULL) return 1;

	color_buckets_init(color_buckets,COLOR_BUCKETS);

	for (y=0;y < gen_png_height;y++) {
		unsigned char Y,U,V;

		memcpy(gen_png_image_rows[y], src_png_image_rows[y], gen_png_width * src_png_bypp);
		row = gen_png_image_rows[y];

		if (pal_vga) {
			for (x=0;x < (gen_png_width * src_png_bypp);x++)
				row[x] &= 0xFC; // strip to 6 bits
		}

		for (x=0;x < (gen_png_width * src_png_bypp);x += src_png_bypp) {
			nbucket = (struct color_bucket*)malloc(sizeof(struct color_bucket));
			nbucket->next = NULL;
			nbucket->count = 0;
			if (src_png_bypp >= 4) {
				nbucket->R = row[x+0];
				nbucket->G = row[x+1];
				nbucket->B = row[x+2];
				nbucket->A = row[x+3];
				rgb2yuv(&Y,&U,&V,nbucket->R,nbucket->G,nbucket->B);
			}
			else { // assume 3
				nbucket->R = row[x+0];
				nbucket->G = row[x+1];
				nbucket->B = row[x+2];
				nbucket->A = 0xFF;
				rgb2yuv(&Y,&U,&V,nbucket->R,nbucket->G,nbucket->B);
			}

			if (minr > nbucket->R) minr = nbucket->R;
			if (ming > nbucket->G) ming = nbucket->G;
			if (minb > nbucket->B) minb = nbucket->B;
			if (mina > nbucket->A) mina = nbucket->A;
			if (maxr < nbucket->R) maxr = nbucket->R;
			if (maxg < nbucket->G) maxg = nbucket->G;
			if (maxb < nbucket->B) maxb = nbucket->B;
			if (maxa < nbucket->A) maxa = nbucket->A;

			ci = (Y * 8u * 8u) + ((U / (256u / 8u)) * 8u) + (V / (256u / 8u));
			color_bucket_add(color_buckets,COLOR_BUCKETS,ci,nbucket); // bucket index by Y (grayscale)
		}
	}

	{
		unsigned char mr = maxr-minr;
		selchannel = 0;

		if (mr < (maxg-ming)) {
			mr = maxg-ming;
			selchannel=1;
		}

		if (mr < (maxb-minb)) {
			mr = maxb-minb;
			selchannel=2;
		}

		if (mr < (maxa-mina)) {
			mr = maxa-mina;
			selchannel=3;
		}
	}

	printf("Min/Max R=%u-%u G=%u-%u B=%u-%u A=%u-%u selchannel=%u\n",minr,maxr,ming,maxg,minb,maxb,mina,maxa,selchannel);

	/* sort from highest to lowest count (occurrence) */
	for (x=0;x < COLOR_BUCKETS;x++)
		color_bucket_sort_by_channel(&color_buckets[x],selchannel);

	for (x=1;x < COLOR_BUCKETS;x++)
		color_bucket_merge_channel(&color_buckets[0],&color_buckets[x],selchannel);

	/* eliminate any color that occurs less than 1 out of (N*N) times of the total picture -- all colors in bucket 0 */
	{
		struct color_bucket **scan;
		unsigned int thr;

		scan = &color_buckets[0];
		colors = color_bucket_count(*scan);
		thr = colors / (target_colors * target_colors);
		while (colors > target_colors) {
			if (thr >= 2) {
				printf("Dropping entries that occurs less than %u times (colors=%u)\n",thr,colors);
				while (*scan != NULL) {
					if ((*scan)->count < thr) {
						/* drop the node */
						struct color_bucket *n = *scan;
						*scan = n->next;
						if (*scan != NULL) (*scan)->count += n->count;
						free(n);

						colors--;
						if (colors <= target_colors) break;
					}
					else {
						scan = &((*scan)->next);
					}
				}
			}

			if (thr < 2)
				thr = 2;
			else
				thr += (thr + 1u) / 2u;

			scan = &color_buckets[0];
		}
	}

	/* median cut, except just cut into N colors in one pass */
	{
		struct color_bucket *scan = color_buckets[0];
		unsigned int c,i,o,ci;

		color_buckets[0] = NULL;

		colors = color_bucket_count(scan);
		for (i=0,o=0,ci=0;i < target_colors && o < target_colors;i++) {
			if (scan == NULL) abort();
			c = (colors * (i+1u)) / target_colors;
			if (ci < c) {
				if (o >= target_colors) abort();
				color_buckets[o++] = scan;
				while (ci < c) {
					if (scan == NULL) abort();
					if ((++ci) == c) {
						struct color_bucket *n = scan->next;
						scan->next = NULL;
						scan = n;
					}
					else {
						scan = scan->next;
					}
				}
			}
		}
		fflush(stdout);
		colors = o;
	}

	/* Median cut, but instead of using an average of the buckets, use the color that's most common in the bucket (highest count).
	 * Using the average produces a picture that is often desaturated and dull compared to the original image. */
	for (x=0;x < colors;x++)
		color_bucket_sort_by_count(&color_buckets[x]);

#if 1//DEBUG
	printf("%u Colors:\n",colors);
	for (y=0;y < COLOR_BUCKETS;y++) {
		nbucket = color_buckets[y];
		if (nbucket != NULL) {
			printf("  Bucket %u\n",y);
			while (nbucket != NULL) {
				printf("    RGB=%03u/%03u/%03u/%03u count=%u\n",
					nbucket->R,
					nbucket->G,
					nbucket->B,
					nbucket->A,
					nbucket->count);

				nbucket = nbucket->next;
			}
		}
	}
#endif

	if (gen_png_image != NULL) {
		free(gen_png_image);
		gen_png_image = NULL;
	}

	if (gen_png_image_rows != NULL) {
		free(gen_png_image_rows);
		gen_png_image_rows = NULL;
	}

	gen_png_width = 16 * 4;
	gen_png_height = 16 * 4;
	gen_png_bit_depth = 8;
	gen_png_color_type = PNG_COLOR_TYPE_PALETTE;

	gen_png_image = malloc((gen_png_width * gen_png_height) + 4096);
	if (gen_png_image == NULL) abort();

	gen_png_image_rows = (png_bytep*)malloc(sizeof(png_bytep) * gen_png_height);
	if (gen_png_image_rows == NULL) abort();

	{
		unsigned int y;
		for (y=0;y < gen_png_height;y++)
			gen_png_image_rows[y] = gen_png_image + (y * gen_png_width);
	}

	{
		unsigned int endcolor = target_colors;
		unsigned int bucket=0;

		gen_png_pal_count = 0;

		if (color0_black) {
			endcolor++;
			if (endcolor > want_colors)
				return 1;

			gen_png_pal[gen_png_pal_count].red = 0;
			gen_png_pal[gen_png_pal_count].green = 0;
			gen_png_pal[gen_png_pal_count].blue = 0;
			gen_png_pal_count++;
		}

		if (endcolor > want_colors || endcolor > 256)
			abort();

		while (gen_png_pal_count < endcolor && bucket < colors) {
			struct color_bucket *s = color_buckets[bucket++];

			if (s != NULL) {
				gen_png_pal[gen_png_pal_count].red = s->R;
				gen_png_pal[gen_png_pal_count].green = s->G;
				gen_png_pal[gen_png_pal_count].blue = s->B;

#if 1//DEBUG
				printf("Palette[%u]: R=%03u G=%03u B=%03u\n",
					gen_png_pal_count,
					gen_png_pal[gen_png_pal_count].red,
					gen_png_pal[gen_png_pal_count].green,
					gen_png_pal[gen_png_pal_count].blue);
#endif

				gen_png_pal_count++;
			}
		}
	}

	for (y=0;y < gen_png_height;y++) {
		row = gen_png_image_rows[y];
		for (x=0;x < gen_png_width;x++) {
			unsigned int i = ((y / 4u) * 16u) + (x / 4u);
			if (i >= gen_png_pal_count) i = 0;
			row[x] = i;
		}
	}

	color_buckets_free(color_buckets,COLOR_BUCKETS);
	free(color_buckets);
	return 0;
#undef COLOR_BUCKETS
}

static int save_out_png(void) {
    png_structp png_context = NULL;
    png_infop png_context_info = NULL;
    png_infop png_context_end = NULL;
    FILE *fp = NULL;
    int ret = 1;

    if (out_png == NULL)
        return 1;

    if (gen_png_width <= 0 || gen_png_width > 4096 || gen_png_height <= 0 || gen_png_height > 4096 || gen_png_bit_depth != 8)
        return 1;
    if (gen_png_image == NULL || gen_png_image_rows == NULL)
        return 1;

    fp = fopen(out_png,"wb");
    if (fp == NULL)
        return 1;

    png_context = png_create_write_struct(PNG_LIBPNG_VER_STRING,NULL/*error*/,NULL/*error fn*/,NULL/*warn fn*/);
    if (png_context == NULL) goto fail;

    png_context_info = png_create_info_struct(png_context);
    if (png_context_info == NULL) goto fail;

    png_init_io(png_context, fp);

    png_set_IHDR(png_context, png_context_info, gen_png_width, gen_png_height, gen_png_bit_depth, gen_png_color_type, gen_png_interlace_method, gen_png_compression_method, gen_png_filter_method);

    if (gen_png_color_type == PNG_COLOR_TYPE_PALETTE)
	    png_set_PLTE(png_context, png_context_info, gen_png_pal, gen_png_pal_count);

    png_write_info(png_context, png_context_info);
    png_write_rows(png_context, gen_png_image_rows, gen_png_height);
    png_write_end(png_context, NULL);

    /* success */
    ret = 0;
fail:
    if (png_context != NULL)
        png_destroy_write_struct(&png_context,&png_context_info);

    if (ret)
        fprintf(stderr,"Failed to save output PNG\n");

    fclose(fp);
    return ret;
}

int main(int argc,char **argv) {
    if (parse_argv(argc,argv))
        return 1;

    if (load_in_png())
        return 1;
    if (make_palette())
        return 1;
    if (save_out_png())
        return 1;

    free_src_png();
    free_gen_png();
    return 0;
}
