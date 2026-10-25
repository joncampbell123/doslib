#ifndef VHSDROPOUT_MOTION_H
#define VHSDROPOUT_MOTION_H

#include <stdint.h>

#include <vector>

#include "plane.h"

// an image of 8-bit samples, to match quickly
struct image8_t {
    std::vector<uint8_t>        v;
    int                         w = 0;
    int                         h = 0;

    void alloc(int width,int height) {
        w = width;
        h = height;
        v.assign((size_t)width * height,0);
    }
    uint8_t *row(int y) {
        return &v[(size_t)y * w];
    }
    const uint8_t *row(int y) const {
        return &v[(size_t)y * w];
    }
};

// an image at full size, half, quarter, ..., each level half the size of the one before,
// and the full size image half a pixel to the right, half a line down, and both
struct pyramid_t {
    std::vector<image8_t>       level;
    image8_t                    half[3];            // x + 0.5, y + 0.5, both

    void clear(void) {
        std::vector<image8_t>().swap(level);
        for (int i=0;i < 3;i++)
            std::vector<uint8_t>().swap(half[i].v);
    }
    bool empty(void) const {
        return level.empty();
    }
};

void build_pyramid(pyramid_t &pyr,const image_t &full,unsigned int levels);

struct me_params_t {
    int                         block = 16;         // pixels, the size of the blocks the picture is matched in
    int                         range_x = 32;       // pixels the picture can move left or right, at most
    int                         range_y = 16;       // pixels the picture can move up or down, at most
    int                         trunc = 24;         // a difference counts for at most this much, so that
                                                    // dropouts and other things that are not in the other picture
                                                    // do not throw the match off
    float                       lambda = 0.05f;     // cost of each pixel the picture moves, so that where it could be
                                                    // anywhere, such as the sky, it stays where it is
    float                       smooth = 0.5f;      // cost of each pixel a block moves differently from the median of
                                                    // the blocks around it, since the picture moves as a whole
    bool                        halfpel = true;     // to half a pixel, not only whole pixels
};

// how the picture moved from one picture to another, block by block: pixel (x,y) of one picture
// is pixel (x + vx,y + vy) of the other. Between the middles of the blocks, the vectors are blended.
struct mvfield_t {
    int                         bw = 0;             // blocks across and down
    int                         bh = 0;
    int                         block = 16;
    std::vector<float>          vx,vy;
    std::vector<float>          cost;               // how badly the block matches, the mean difference per pixel, 0 to trunc

    void alloc(int width,int height,int blk) {
        block = blk;
        bw = (width + blk - 1) / blk;
        bh = (height + blk - 1) / blk;
        vx.assign((size_t)bw * bh,0.0f);
        vy.assign((size_t)bw * bh,0.0f);
        cost.assign((size_t)bw * bh,0.0f);
    }

    // the vector at a pixel of luma, and how badly the blocks around it match
    void at(float x,float y,float &dx,float &dy,float *c=NULL) const;

    // the other way: from the other picture to this one, near enough where the picture moves smoothly
    void reverse(mvfield_t &r) const {
        r = *this;
        for (size_t i=0;i < vx.size();i++) {
            r.vx[i] = -vx[i];
            r.vy[i] = -vy[i];
        }
    }
};

// how the picture moved from cur to ref
void estimate_motion(mvfield_t &mv,const pyramid_t &cur,const pyramid_t &ref,const me_params_t &mp);

// how badly each block matches where its vector puts it, to the half pixel, after the vectors were changed:
// the median difference of its pixels
void block_costs(mvfield_t &mv,const pyramid_t &cur,const pyramid_t &ref,const me_params_t &mp);

// the vectors again, of the blocks near the dropouts of cur, from the vector of each block and the blocks
// next to it, matching only the pixels that are not dropouts in either picture, and for blocks that are
// almost all dropouts, the vectors of the blocks around them
void refine_masked(mvfield_t &mv,const image_t &cur,const image_t &ref,const mask_t &cmask,const mask_t &rmask,const me_params_t &mp);

// ref moved to where cur is, the prediction of cur from ref
void predict(image_t &out,const image_t &ref,const mvfield_t &mv);

#endif //VHSDROPOUT_MOTION_H
