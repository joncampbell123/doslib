#ifndef PALQUANT_BMPIO_H
#define PALQUANT_BMPIO_H

#include <stdint.h>

#include <string>
#include <vector>

// one pixel, 8 bits per channel
struct rgb_t {
    uint8_t                     r,g,b;
};

// a truecolor image, top row first
struct rgb_image_t {
    unsigned int                width = 0;
    unsigned int                height = 0;
    unsigned int                bpp = 0;            // bits per pixel of the file it was read from
    std::vector<rgb_t>          pixels;             // width * height
};

// a paletted image, top row first
struct paletted_image_t {
    unsigned int                width = 0;
    unsigned int                height = 0;
    std::vector<uint8_t>        pixels;             // palette index of each pixel, width * height
    std::vector<rgb_t>          palette;            // 1 to 256 entries
};

// read an uncompressed 24bpp or 32bpp BMP file. Alpha, if any, is ignored.
// returns false and says why in err if it cannot.
bool read_bmp_truecolor(const std::string &path,rgb_image_t &img,std::string &err);

// write a paletted BMP file with 1, 4, or 8 bits per pixel. bpp 0 picks the fewest that hold the palette.
// returns false and says why in err if it cannot.
bool write_bmp_paletted(const std::string &path,const paletted_image_t &img,unsigned int bpp,std::string &err);

// the fewest bits per pixel (1, 4, or 8) that hold a palette of this many colors
unsigned int bpp_for_colors(size_t colors);

#endif //PALQUANT_BMPIO_H
