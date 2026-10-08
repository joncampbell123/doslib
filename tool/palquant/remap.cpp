
#include <math.h>

#include <algorithm>

#include "quantize.h"
#include "remap.h"

// the color cache has 2^CACHE_BITS entries. An empty one has a key that is not a 24-bit color.
#define CACHE_BITS                  18u
#define CACHE_EMPTY                 0xFFFFFFFFu

// The nearest palette entry of each color, remembered for the colors seen recently.
// Images reuse the same colors a lot, so this saves most of the searching.
class color_cache_t {
public:
    explicit color_cache_t(const std::vector<rgb_t> &palette) : nc(palette), key(1u << CACHE_BITS,CACHE_EMPTY), value(1u << CACHE_BITS,0) { }

    uint8_t find(const rgb_t &c) {
        const uint32_t k = ((uint32_t)c.r << 16u) | ((uint32_t)c.g << 8u) | (uint32_t)c.b;
        const uint32_t slot = (uint32_t)((k * 2654435761u) >> (32u - CACHE_BITS));

        if (key[slot] != k) {
            key[slot] = k;
            value[slot] = (uint8_t)nc.find(c);
        }

        return value[slot];
    }

private:
    nearest_color_t             nc;
    std::vector<uint32_t>       key;
    std::vector<uint8_t>        value;
};

// an error in 16ths, divided by 16 and rounded
static inline int div16(const int e) {
    return (e >= 0) ? ((e + 8) / 16) : -((8 - e) / 16);
}

static inline int clamp255(const int v) {
    return (v < 0) ? 0 : ((v > 255) ? 255 : v);
}

paletted_image_t remap_image(const rgb_image_t &img,const std::vector<rgb_t> &palette,bool dither) {
    paletted_image_t out;
    color_cache_t cache(palette);

    out.width = img.width;
    out.height = img.height;
    out.palette = palette;
    out.pixels.resize(img.pixels.size());

    if (!dither) {
        for (size_t i=0;i < img.pixels.size();i++)
            out.pixels[i] = cache.find(img.pixels[i]);

        return out;
    }

    // Floyd-Steinberg error diffusion. Each pixel's error goes 7/16 to the next pixel, and 3/16, 5/16 and
    // 1/16 to the three below it. Rows alternate direction so that the error does not always drift
    // the same way. The errors are kept in 16ths, with a pixel of margin on both sides of the row.
    // The corrected color is clamped to 0-255 before it is matched.
    //
    // A color that no mix of the palette can make (a green with little blue, when every greenish
    // entry has a lot of blue) leaves an error that dithering can never pay back. It builds up,
    // skews the colors around it, and bleeds into what follows. When the image's color can be
    // made, the error a pixel passes on is no larger than the distance from its palette entry to
    // the nearest other entry, so each pixel passes on no more than that.
    const int w = (int)img.width;
    std::vector<int> cur((size_t)(w + 2) * 3u,0),next((size_t)(w + 2) * 3u,0);
    std::vector<int> max_err(palette.size(),0);

    for (size_t i=0;i < palette.size();i++) {
        double nearest = -1;

        for (size_t j=0;j < palette.size();j++) {
            if (i != j) {
                const double dr = (double)palette[i].r - (double)palette[j].r;
                const double dg = (double)palette[i].g - (double)palette[j].g;
                const double db = (double)palette[i].b - (double)palette[j].b;
                const double d = sqrt((dr * dr) + (dg * dg) + (db * db));

                if (nearest < 0 || d < nearest)
                    nearest = d;
            }
        }

        if (nearest > 0)
            max_err[i] = (int)(nearest + 0.5);
    }

    for (unsigned int y=0;y < img.height;y++) {
        const bool backwards = (y & 1u) != 0;
        const int dir = backwards ? -1 : 1;

        std::fill(next.begin(),next.end(),0);
        for (int i=0;i < w;i++) {
            const int x = backwards ? (w - 1 - i) : i;
            const size_t pi = ((size_t)y * img.width) + (size_t)x;
            const rgb_t &p = img.pixels[pi];
            const int e = (x + 1) * 3;
            int v[3];
            rgb_t want;

            v[0] = clamp255((int)p.r + div16(cur[e + 0]));
            v[1] = clamp255((int)p.g + div16(cur[e + 1]));
            v[2] = clamp255((int)p.b + div16(cur[e + 2]));
            want.r = (uint8_t)v[0];
            want.g = (uint8_t)v[1];
            want.b = (uint8_t)v[2];

            const uint8_t idx = cache.find(want);
            const rgb_t &got = palette[idx];
            const int err[3] = { v[0] - (int)got.r,v[1] - (int)got.g,v[2] - (int)got.b };
            out.pixels[pi] = idx;

            for (unsigned int k=0;k < 3;k++) {
                const int pass = std::max(-max_err[idx],std::min(max_err[idx],err[k]));

                cur[e + (dir * 3) + (int)k] += pass * 7;
                next[e - (dir * 3) + (int)k] += pass * 3;
                next[e + (int)k] += pass * 5;
                next[e + (dir * 3) + (int)k] += pass;
            }
        }

        cur.swap(next);
    }

    return out;
}

