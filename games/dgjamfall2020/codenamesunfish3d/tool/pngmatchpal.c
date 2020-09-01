
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <png.h>    /* libpng */

static char*            pal_png = NULL;
static char*            in_png = NULL;
static char*            out_png = NULL;

static png_color        pal_png_pal[256] = {0};
static int              pal_png_pal_count = 0;
static png_byte         pal_png_pal_trns[256];
static int              pal_png_pal_trns_count = 0;

static unsigned char    pal_remap[256] = {0}; /* gen_pal -> pal_pal */

static png_color        gen_png_pal[256] = {0};
static int              gen_png_pal_count = 0;

static unsigned char*   gen_png_image = NULL;
static png_bytep*       gen_png_image_rows = NULL;
static png_uint_32      gen_png_width = 0,gen_png_height = 0;
static int              gen_png_bit_depth = 0;
static int              gen_png_color_type = 0;
static int              gen_png_interlace_method = 0;
static int              gen_png_compression_method = 0;
static int              gen_png_filter_method = 0;
static png_byte         gen_png_trns[256];
static int              gen_png_trns_count = 0;

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
    fprintf(stderr,"pngmatchpal -i <input PNG> -o <output PNG> -p <palette PNG>\n");
    fprintf(stderr,"Convert a paletted PNG to another paletted PNG,\n");
    fprintf(stderr,"rearranging the palette to match palette PNG.\n");
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
            else if (!strcmp(a,"p")) {
                if ((pal_png = argv[i++]) == NULL)
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

    if (pal_png == NULL) {
        fprintf(stderr,"Palette -p png required\n");
        return 1;
    }

    return 0;
}

static int load_palette_png(void) {
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

    if (pal_png == NULL)
        return 1;

    fp = fopen(pal_png,"rb");
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

    if (png_color_type != PNG_COLOR_TYPE_PALETTE) {
        fprintf(stderr,"Palette PNG not paletted\n");
        goto fail;
    }

    {
        png_color* pal = NULL;
        int pal_count = 0;

        /* FIXME: libpng makes no reference to freeing this. Do you? */
        if (png_get_PLTE(png_context, png_context_info, &pal, &pal_count) == 0) {
            fprintf(stderr,"Unable to get Palette PNG palette\n");
            goto fail;
        }

        /* I think libpng only points at it's in memory buffers. Copy it. */
        pal_png_pal_count = pal_count;
        if (pal_count != 0 && pal_count <= 256)
            memcpy(pal_png_pal,pal,sizeof(png_color) * pal_count);
    }

    /* we gotta preserve alpha transparency too */
    pal_png_pal_trns_count = 0;
    {
        png_color_16p trans_values = 0; /* throwaway value */
        png_bytep trans = NULL;
        int trans_count = 0;

        pal_png_pal_trns_count = 0;
        if (png_get_tRNS(png_context, png_context_info, &trans, &trans_count, &trans_values) != 0) {
            if (trans_count != 0 && trans_count <= 256) {
                memcpy(pal_png_pal_trns, trans, trans_count);
                pal_png_pal_trns_count = trans_count;
            }
        }
    }

    /* success */
    ret = 0;
fail:
    if (png_context != NULL)
        png_destroy_read_struct(&png_context,&png_context_info,&png_context_end);

    if (ret)
        fprintf(stderr,"Failed to load palette PNG\n");

    fclose(fp);
    return ret;
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

    if (png_color_type != PNG_COLOR_TYPE_PALETTE) {
        fprintf(stderr,"Input PNG not paletted\n");
        goto fail;
    }

    {
        png_color* pal = NULL;
        int pal_count = 0;

        /* FIXME: libpng makes no reference to freeing this. Do you? */
        if (png_get_PLTE(png_context, png_context_info, &pal, &pal_count) == 0) {
            fprintf(stderr,"Unable to get Input PNG palette\n");
            goto fail;
        }

        /* I think libpng only points at it's in memory buffers. Copy it. */
        gen_png_pal_count = pal_count;
        if (pal_count != 0 && pal_count <= 256)
            memcpy(gen_png_pal,pal,sizeof(png_color) * pal_count);
    }

    /* we gotta preserve alpha transparency too */
    gen_png_trns_count = 0;
    {
        png_color_16p trans_values = 0; /* throwaway value */
        png_bytep trans = NULL;
        int trans_count = 0;

        gen_png_trns_count = 0;
        if (png_get_tRNS(png_context, png_context_info, &trans, &trans_count, &trans_values) != 0) {
            if (trans_count != 0 && trans_count <= 256) {
                memcpy(gen_png_trns, trans, trans_count);
                gen_png_trns_count = trans_count;
            }
        }
    }

    if (png_width <= 0 || png_width > 4096 || png_height <= 0 || png_height > 4096 || png_bit_depth != 8)
        goto fail;

    gen_png_image = malloc((png_width * png_height) + 4096);
    if (gen_png_image == NULL)
        goto fail;

    gen_png_image_rows = (png_bytep*)malloc(sizeof(png_bytep) * png_height);
    if (gen_png_image_rows == NULL)
        goto fail;

    {
        unsigned int y;
        for (y=0;y < png_height;y++)
            gen_png_image_rows[y] = gen_png_image + (y * png_width);
    }

    png_read_rows(png_context, gen_png_image_rows, NULL, png_height);

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
    if (gen_png_color_type != PNG_COLOR_TYPE_PALETTE)
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

    png_set_PLTE(png_context, png_context_info, gen_png_pal, gen_png_pal_count);

    /* we gotta preserve alpha transparency too */
    if (gen_png_trns_count != 0) {
        png_set_tRNS(png_context, png_context_info, gen_png_trns, gen_png_trns_count, 0);
    }

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

static int remap_png(void) {
    if (gen_png_image == NULL || gen_png_image_rows == NULL || pal_png_pal_count > 256)
        return 0;

    {
        unsigned int i,o;

        memset(pal_remap,0xFF,256);
        for (o=0;o < pal_png_pal_count;o++) {
            for (i=0;i < gen_png_pal_count;i++) {
                if (gen_png_pal[i].red   == pal_png_pal[o].red &&
                    gen_png_pal[i].green == pal_png_pal[o].green &&
                    gen_png_pal[i].blue  == pal_png_pal[o].blue) {
                    pal_remap[i] = o;
                }
            }
        }

        fprintf(stderr,"Remap from in pal (%u ent) to palette (%u ent): ",gen_png_pal_count,pal_png_pal_count);
        for (i=0;i < gen_png_pal_count;i++) {
            if (pal_remap[i] == 0xFF)
                fprintf(stderr,"X ");
            else
                fprintf(stderr,"%u ",pal_remap[i]);
        }
        fprintf(stderr,"\n");
    }

    return 0;
}

int main(int argc,char **argv) {
    if (parse_argv(argc,argv))
        return 1;

    if (load_palette_png())
        return 1;
    if (load_in_png())
        return 1;
    if (remap_png())
        return 1;
    if (save_out_png())
        return 1;

    free_gen_png();
    return 0;
}
