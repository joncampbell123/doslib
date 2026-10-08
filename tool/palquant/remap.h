#ifndef PALQUANT_REMAP_H
#define PALQUANT_REMAP_H

#include "bmpio.h"

// convert an image to the palette: each pixel to the nearest palette entry, or with
// Floyd-Steinberg error diffusion dithering if dither is set
paletted_image_t remap_image(const rgb_image_t &img,const std::vector<rgb_t> &palette,bool dither);

#endif //PALQUANT_REMAP_H
