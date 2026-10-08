/* palquant: make one optimized palette for one or more 24bpp or 32bpp BMP files,
 * then convert each of them to a paletted BMP with that palette */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "bmpio.h"
#include "quantize.h"
#include "remap.h"

struct job_t {
    std::string                 input;
    std::string                 output;
    rgb_image_t                 image;
};

static void help(void) {
    fprintf(stderr,"palquant [options] <input.bmp> [-o <output.bmp>] [<input.bmp> [-o <output.bmp>] ...]\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"Makes one palette for all of the input files (uncompressed 24bpp or 32bpp BMP),\n");
    fprintf(stderr,"and converts each of them to a paletted BMP with it.\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"  -c <n>       Colors in the palette, 2 to 256 (default 256)\n");
    fprintf(stderr,"  -d           Dither (Floyd-Steinberg error diffusion)\n");
    fprintf(stderr,"  -o <file>    Output file for the input file before it\n");
    fprintf(stderr,"               (default: the input file name with _pq added, foo.bmp -> foo_pq.bmp)\n");
    fprintf(stderr,"  -8           Always write 8 bits per pixel\n");
    fprintf(stderr,"               (default: 1, 4 or 8, the fewest that hold the palette)\n");
    fprintf(stderr,"  -v           Verbose: say what is done, and list the palette\n");
    fprintf(stderr,"  -h           This help\n");
}

// foo.bmp -> foo_pq.bmp, anything else -> anything_pq.bmp
static std::string default_output(const std::string &input) {
    const size_t l = input.size();
    if (l > 4u && input[l-4u] == '.' && strcasecmp(input.c_str() + l - 3u,"bmp") == 0)
        return input.substr(0,l - 4u) + "_pq" + input.substr(l - 4u);

    return input + "_pq.bmp";
}

// true if the two names are, or would be, the same file
static bool same_file(const std::string &a,const std::string &b) {
    struct stat sa,sb;

    if (a == b)
        return true;
    if (stat(a.c_str(),&sa) != 0 || stat(b.c_str(),&sb) != 0)
        return false;

    return sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
}

int main(int argc,char **argv) {
    std::vector<job_t> jobs;
    unsigned int colors = 256;
    bool verbose = false;
    bool dither = false;
    bool force8 = false;

    for (int i=1;i < argc;i++) {
        const char *a = argv[i];

        if (a[0] == '-' && a[1] != 0) {
            const std::string opt = a + 1;

            if (opt == "c") {
                char *end = NULL;
                if (++i >= argc) {
                    fprintf(stderr,"-c needs the number of colors\n");
                    return 1;
                }

                const long v = strtol(argv[i],&end,10);
                if (end == argv[i] || *end != 0 || v < 2 || v > 256) {
                    fprintf(stderr,"-c %s: the number of colors must be 2 to 256\n",argv[i]);
                    return 1;
                }
                colors = (unsigned int)v;
            }
            else if (opt == "o") {
                if (++i >= argc) {
                    fprintf(stderr,"-o needs a file name\n");
                    return 1;
                }
                if (jobs.empty() || !jobs.back().output.empty()) {
                    fprintf(stderr,"-o %s: -o goes after the input file it is the output of\n",argv[i]);
                    return 1;
                }
                jobs.back().output = argv[i];
            }
            else if (opt == "d") {
                dither = true;
            }
            else if (opt == "8") {
                force8 = true;
            }
            else if (opt == "v") {
                verbose = true;
            }
            else if (opt == "h" || opt == "-help") {
                help();
                return 0;
            }
            else {
                fprintf(stderr,"Unknown option %s\n",a);
                help();
                return 1;
            }
        }
        else {
            job_t j;
            j.input = a;
            jobs.push_back(j);
        }
    }

    if (jobs.empty()) {
        help();
        return 1;
    }

    // never write over an input file, or write two outputs to one file
    for (size_t i=0;i < jobs.size();i++) {
        if (jobs[i].output.empty())
            jobs[i].output = default_output(jobs[i].input);
    }
    for (size_t i=0;i < jobs.size();i++) {
        for (size_t j=0;j < jobs.size();j++) {
            if (same_file(jobs[i].output,jobs[j].input)) {
                fprintf(stderr,"%s: output would write over the input file %s\n",jobs[i].output.c_str(),jobs[j].input.c_str());
                return 1;
            }
            if (j > i && same_file(jobs[i].output,jobs[j].output)) {
                fprintf(stderr,"%s: two input files would be written to the same output file\n",jobs[i].output.c_str());
                return 1;
            }
        }
    }

    // read them all, and count their colors together so that the palette suits all of them
    color_histogram_t hist;
    for (size_t i=0;i < jobs.size();i++) {
        std::string err;

        if (!read_bmp_truecolor(jobs[i].input,jobs[i].image,err)) {
            fprintf(stderr,"%s: %s\n",jobs[i].input.c_str(),err.c_str());
            return 1;
        }

        hist.add(jobs[i].image);
        if (verbose)
            printf("%s: %ux%u, %u bits per pixel\n",jobs[i].input.c_str(),jobs[i].image.width,jobs[i].image.height,jobs[i].image.bpp);
    }

    const std::vector<rgb_t> palette = make_palette(hist,colors);

    if (verbose) {
        if (hist.distinct_colors() > color_histogram_t::MAX_EXACT)
            printf("Palette: %u color%s, for more than %u distinct colors\n",(unsigned int)palette.size(),palette.size() == 1u ? "" : "s",(unsigned int)color_histogram_t::MAX_EXACT);
        else
            printf("Palette: %u color%s, for %u distinct color%s\n",(unsigned int)palette.size(),palette.size() == 1u ? "" : "s",
                (unsigned int)hist.distinct_colors(),hist.distinct_colors() == 1u ? "" : "s");

        for (size_t i=0;i < palette.size();i++)
            printf("    %3u: #%02x%02x%02x\n",(unsigned int)i,palette[i].r,palette[i].g,palette[i].b);
    }

    for (size_t i=0;i < jobs.size();i++) {
        const paletted_image_t out = remap_image(jobs[i].image,palette,dither);
        const unsigned int bpp = force8 ? 8u : bpp_for_colors(palette.size());
        std::string err;

        if (!write_bmp_paletted(jobs[i].output,out,bpp,err)) {
            fprintf(stderr,"%s: %s\n",jobs[i].output.c_str(),err.c_str());
            return 1;
        }

        if (verbose)
            printf("%s -> %s, %u bits per pixel%s\n",jobs[i].input.c_str(),jobs[i].output.c_str(),bpp,dither ? ", dithered" : "");

        // the truecolor image is not needed any more
        jobs[i].image.pixels = std::vector<rgb_t>();
    }

    return 0;
}

