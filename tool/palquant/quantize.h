#ifndef PALQUANT_QUANTIZE_H
#define PALQUANT_QUANTIZE_H

#include "bmpio.h"

// how much the red, green and blue differences count in the distance between two colors.
// The eye is most sensitive to green, then red, then blue.
#define PQ_WEIGHT_R                 3u
#define PQ_WEIGHT_G                 4u
#define PQ_WEIGHT_B                 2u

static inline uint32_t color_distance(const rgb_t &a,const rgb_t &b) {
    const int dr = (int)a.r - (int)b.r;
    const int dg = (int)a.g - (int)b.g;
    const int db = (int)a.b - (int)b.b;
    return (PQ_WEIGHT_R * (uint32_t)(dr * dr)) + (PQ_WEIGHT_G * (uint32_t)(dg * dg)) + (PQ_WEIGHT_B * (uint32_t)(db * db));
}

// how many pixels of each color there are, over all of the images given to it
class color_histogram_t {
public:
    // colors are counted in cells of 6 bits per channel, keeping the sum of the colors in each cell
    // so that its average color is exact
    struct cell_t {
        uint64_t                count = 0;
        uint64_t                r = 0,g = 0,b = 0;  // sums
    };

    color_histogram_t();

    void add(const rgb_image_t &img);

    // how many distinct colors there are, or MAX_EXACT+1 if there are more than MAX_EXACT
    size_t distinct_colors() const { return distinct; }

    // the distinct colors, if there are no more than MAX_EXACT of them
    const std::vector<rgb_t> &exact_colors() const { return exact; }

    const std::vector<cell_t> &cells() const { return cell; }

    static const size_t MAX_EXACT = 256;

private:
    std::vector<uint64_t>       seen;               // one bit for each 24-bit color
    std::vector<rgb_t>          exact;
    size_t                      distinct = 0;
    std::vector<cell_t>         cell;
};

// make a palette of at most `colors` entries (2 to 256) for the colors in the histogram, darkest first.
// If there are no more distinct colors than that, the palette is those colors.
std::vector<rgb_t> make_palette(const color_histogram_t &hist,unsigned int colors);

// finds the palette entry nearest to a color
class nearest_color_t {
public:
    explicit nearest_color_t(const std::vector<rgb_t> &palette);

    // index of the nearest palette entry
    unsigned int find(const rgb_t &c) const;

private:
    struct entry_t {
        rgb_t                   c;
        unsigned int            index;
    };

    std::vector<entry_t>        by_green;           // the palette, sorted by green
};

#endif //PALQUANT_QUANTIZE_H
