
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <png.h>    /* libpng */

static char*            in_png = NULL;
static char*            out_png = NULL;

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
            else if (!strcmp(a,"vga")) {
                pal_vga = 1;
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
	unsigned char			Y,U,V;
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

void color_bucket_add(struct color_bucket **buckets,unsigned int num,unsigned int idx,struct color_bucket *nb) {
	if (idx < num && nb->next == NULL) {
		// same as last color? then drop it
		if (buckets[idx] != NULL) {
			if (buckets[idx]->R == nb->R && buckets[idx]->G == nb->G && buckets[idx]->B == nb->B && buckets[idx]->A == nb->A) {
				/* we're expected to insert it, if we do not, then we must free it, else a memory leak occurs */
				buckets[idx]->count++;
				free(nb);
				return;
			}
		}
		// add to head, for performance
		nb->next = buckets[idx];
		nb->count = 1;
		buckets[idx] = nb;
	}
}

int colorrgbcompare(struct color_bucket *a,struct color_bucket *b) {
	if (a->Y > b->Y) return 1;
	if (a->Y < b->Y) return 0;
	if (a->U > b->U) return 1;
	if (a->U < b->U) return 0;
	if (a->V > b->V) return 1;
	if (a->V < b->V) return 0;
	return 0;
}

static int make_palette() {
#define COLOR_BUCKETS (256u * 8u * 8u)
	struct color_bucket** color_buckets;
	struct color_bucket* nbucket;
	unsigned int x,y,ci;
	png_bytep row;

	color_buckets = (struct color_bucket**)malloc(sizeof(struct color_bucket*) * COLOR_BUCKETS);
	if (color_buckets == NULL) return 1;

	color_buckets_init(color_buckets,COLOR_BUCKETS);

	for (y=0;y < gen_png_height;y++) {
		memcpy(gen_png_image_rows[y], src_png_image_rows[y], gen_png_width * src_png_bypp);
		row = gen_png_image_rows[y];

		if (pal_vga) {
			for (x=0;x < (gen_png_width * src_png_bypp);x++)
				row[x] &= 0xFC; // strip to 6 bits
		}

		for (x=0;x < (gen_png_width * src_png_bypp);x += src_png_bypp) {
			nbucket = (struct color_bucket*)malloc(sizeof(struct color_bucket));
			nbucket->count = 0;
			nbucket->next = NULL;
			if (src_png_bypp >= 4) {
				nbucket->R = row[x+0];
				nbucket->G = row[x+1];
				nbucket->B = row[x+2];
				nbucket->A = row[x+3];
				rgb2yuv(&nbucket->Y,&nbucket->U,&nbucket->V,(nbucket->R*nbucket->A)/255u,(nbucket->G*nbucket->A)/255u,(nbucket->B*nbucket->A)/255u);
			}
			else { // assume 3
				nbucket->R = row[x+0];
				nbucket->G = row[x+1];
				nbucket->B = row[x+2];
				nbucket->A = 0xFF;
				rgb2yuv(&nbucket->Y,&nbucket->U,&nbucket->V,nbucket->R,nbucket->G,nbucket->B);
			}

			ci = (nbucket->Y * 8u * 8u) + ((nbucket->U / (256u / 8u)) * 8u) + (nbucket->V / (256u / 8u));
			color_bucket_add(color_buckets,COLOR_BUCKETS,ci,nbucket); // bucket index by Y (grayscale)
		}
	}

	/* sort from least similar to most similar */
	for (x=0;x < COLOR_BUCKETS;x++) {
		struct color_bucket **c,*n,*pn;

		c = &color_buckets[x];
		while (*c != NULL) {
			const int cdx = (int)((*c)->U) - 128;
			const int cdy = (int)((*c)->V) - 128;
			const int cd = cdx * cdx + cdy * cdy;

			pn = *c;
			n = (*c)->next;
			if (n != NULL) {
				while (n != NULL) {
					const int ndx = (int)(n->U) - 128;
					const int ndy = (int)(n->V) - 128;
					const int nd = ndx * ndx + ndy * ndy;
					if (nd > cd || (nd == cd && colorrgbcompare(n,*c) > 0)) {
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

		// remove duplicates
		pn = color_buckets[x];
		if (pn != NULL) {
			n = pn->next;
			while (n != NULL) {
				if (pn->R == n->R && pn->G == n->G && pn->B == n->B && pn->A == n->A) {
					// remove from list and free
					pn->count += n->count;
					pn->next = n->next;
					free(n);
					n = pn->next;
				}
				else {
					pn = n;
					n = n->next;
				}
			}
		}
	}

#if 1//DEBUG
	printf("Colors:\n");
	for (y=0;y < COLOR_BUCKETS;y++) {
		printf("  Bucket %u\n",y);
		nbucket = color_buckets[y];
		while (nbucket != NULL) {
			printf("    RGB=%03u/%03u/%03u/%03u YUV=%03u/%03u/%03u count=%u\n",
				nbucket->R,
				nbucket->G,
				nbucket->B,
				nbucket->A,
				nbucket->Y,
				nbucket->U,
				nbucket->V,
				nbucket->count);

			nbucket = nbucket->next;
		}
	}
#endif

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
