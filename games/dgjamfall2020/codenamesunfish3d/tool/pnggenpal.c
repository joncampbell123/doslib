
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

    {
        unsigned int y;
        for (y=0;y < png_height;y++)
            memcpy(gen_png_image_rows[y], src_png_image_rows[y], png_width * src_png_bypp);
    }

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

int rgbasortvalue(unsigned char r,unsigned char g,unsigned char b,unsigned char a) {
    return (int)((((r * 30u) + (g * 59u) + (b * 11u)) * a) / 255u);
}

int sortxpixelqsortcommonrgba(unsigned char ra,unsigned char ga,unsigned char ba,unsigned char aa,unsigned char rb,unsigned char gb,unsigned char bb,unsigned char ab) {
    return rgbasortvalue(ra,ga,ba,aa) - rgbasortvalue(rb,gb,bb,ab);
}

int sortxpixelqsort3(const void *a,const void *b) {
    const unsigned char *ba = (const unsigned char*)a;
    const unsigned char *bb = (const unsigned char*)b;
    return sortxpixelqsortcommonrgba(ba[0],ba[1],ba[2],0xFF,bb[0],bb[1],bb[2],0xFF);
}

int sortxpixelqsort4(const void *a,const void *b) {
    const unsigned char *ba = (const unsigned char*)a;
    const unsigned char *bb = (const unsigned char*)b;
    return sortxpixelqsortcommonrgba(ba[0],ba[1],ba[2],ba[3],bb[0],bb[1],bb[2],bb[3]);
}

void sortxpixels(unsigned char *pixels,unsigned int w,unsigned int bypp) {
    if (bypp == 3)
        qsort(pixels,w,bypp,sortxpixelqsort3);
    else if (bypp == 4)
        qsort(pixels,w,bypp,sortxpixelqsort4);
}

static int make_palette() {
	unsigned int x,y,o;
	png_bytep row;

	/* take each row and sort the pixel values in groups */
	for (y=0;y < gen_png_height;y++) {
		row = gen_png_image_rows[y];
		sortxpixels(row,gen_png_width,src_png_bypp);
	}

	return 0;
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
