
#include <stdio.h>
#include <string.h>

static char*        pal_png = NULL;
static char*        in_png = NULL;
static char*        out_png = NULL;

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

int main(int argc,char **argv) {
    if (parse_argv(argc,argv))
        return 1;

    return 0;
}
