#ifndef MSV1_CRAM_H
#define MSV1_CRAM_H

#include <stddef.h>
#include <stdint.h>

#include <vector>

// How each 4x4 block of a Microsoft Video 1 (CRAM) frame was coded, from its packet. The colors are not
// kept: they are read from the frame as decoded, which has them, whether from a palette or 15-bit RGB.
enum cram_mode_t {
    CRAM_UNKNOWN=0,                                 // not coded yet, as before the first keyframe
    CRAM_SOLID,                                     // one color, the mean of the block
    CRAM_TWO,                                       // two colors, a bit of each pixel saying which
    CRAM_EIGHT                                      // two colors to each 2x2 quarter, a bit of each pixel saying which
};

struct cram_block_t {
    uint8_t                     mode = CRAM_UNKNOWN;
    uint16_t                    flags = 0;          // the bit of each pixel, as coded: bit 0 is the bottom left pixel,
                                                    // then the rest of the bottom row, then the rows above it
};

// the blocks of a frame, left to right, top to bottom
struct cram_map_t {
    int                         bw = 0,bh = 0;      // blocks across and down
    std::vector<cram_block_t>   b;
    std::vector<uint8_t>        coded;              // 1 for the blocks coded in this frame, 0 for those it skips

    void alloc(int width,int height) {
        bw = width / 4;
        bh = height / 4;
        b.assign((size_t)bw * bh,cram_block_t());
        coded.assign((size_t)bw * bh,0);
    }
    cram_block_t &at(int bx,int by) {
        return b[((size_t)by * bw) + bx];
    }
    const cram_block_t &at(int bx,int by) const {
        return b[((size_t)by * bw) + bx];
    }

    // The packet of a frame, as FFmpeg's decoder reads it, onto the blocks of the frame before: the blocks it
    // skips stay as they were. eight_bit is for 8-bit (paletted) video, as the stream's bits per sample say.
    // False if the packet is too small to be a frame, which the decoder leaves out too.
    bool parse(const uint8_t *buf,size_t size,bool eight_bit);
};

// Which pixels of the block of pixel (x,y) of a frame are one set, coded as one color: the group (the block, or
// its quarter for an eight color block), and the bit of the pixel in it. For a block of one color, the bit is 0.
static inline void cram_pixel_set(const cram_block_t &blk,int px,int py,int &quarter,int &bit) {
    // the bits go from the bottom row up
    const int ry = 3 - py;
    const int i = (ry * 4) + px;
    bit = (blk.mode == CRAM_TWO || blk.mode == CRAM_EIGHT) ? ((blk.flags >> i) & 1) : 0;
    quarter = blk.mode == CRAM_EIGHT ? (((ry >> 1) * 2) + (px >> 1)) : 0;
}

struct cram_params_t {
    int                         iterations = 20;    // turns of smoothing, each followed by putting each pixel back within
                                                    // what its block's code says it can be
    float                       edge = 2.0f;        // levels of change per pixel above which the picture is smoothed only
                                                    // along its edges, not across them
    float                       scale = 2.0f;       // pixels around each pixel that the way of its edge is found from: more
                                                    // for the edge of each block of two colors, which goes in steps, to
                                                    // be seen as one straight edge
    float                       flat = 10.0f;       // how far a pixel of a block of one color may be from that color, in
                                                    // levels of 0 to 255: the encoder makes a block one color only if it
                                                    // is near enough to it all over
    float                       noise = 24.0f;      // two colors of a block or quarter nearer than this, in levels, are taken
                                                    // as one color with noise
    float                       tolerance = 3.0f;   // how far the mean of each set of pixels may be from its color, which
                                                    // was rounded to a palette or 15 bits
    float                       spread = 0.6f;      // how far beyond its color each pixel of a two color set may be, as
                                                    // a fraction of the way from one color to the other
    float                       rate = 0.2f;        // how much the pixels of two and eight color blocks are smoothed each
                                                    // turn, against those of blocks of one color: less, their bits having
                                                    // the detail that blocks of one color have lost
    bool                        solid = true;       // interpolate the blocks of one color, not only the two color blocks
};

// What the frame most likely was, before the encoder took it down to a color or two in each block: smoothed,
// along the edges where there are edges, as much as the colors and bits of each block allow. in and out are
// 3 planes of w x h, R, G and B, 0 to 255.
void cram_restore(const float *in,float *out,int w,int h,const cram_map_t &map,const cram_params_t &p);

#endif //MSV1_CRAM_H
