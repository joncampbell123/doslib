#ifndef VMI_FLOW_H
#define VMI_FLOW_H

#include <stddef.h>

#include <vector>

// a plane of float samples, 0 to 255
struct fimage_t {
    std::vector<float>          v;
    int                         w = 0;
    int                         h = 0;

    void alloc(int width,int height) {
        w = width;
        h = height;
        v.assign((size_t)width * height,0.0f);
    }
    float *row(int y) {
        return &v[(size_t)y * w];
    }
    const float *row(int y) const {
        return &v[(size_t)y * w];
    }

    // the image at (x,y), between samples, the edges going on outside it
    float at(float x,float y) const {
        if (x < 0.0f) x = 0.0f;
        if (y < 0.0f) y = 0.0f;
        if (x > (float)(w - 1)) x = (float)(w - 1);
        if (y > (float)(h - 1)) y = (float)(h - 1);
        const int x0 = (int)x,y0 = (int)y;
        const int x1 = x0 + 1 < w ? x0 + 1 : x0,y1 = y0 + 1 < h ? y0 + 1 : y0;
        const float fx = x - (float)x0,fy = y - (float)y0;
        const float *r0 = row(y0),*r1 = row(y1);
        const float a = r0[x0] + ((r0[x1] - r0[x0]) * fx);
        const float b = r1[x0] + ((r1[x1] - r1[x0]) * fx);
        return a + ((b - a) * fy);
    }
};

// an image at full size, half, quarter, ..., each half the size of the one before
struct pyr_t {
    std::vector<fimage_t>       lv;
};

void build_pyr(pyr_t &p,const fimage_t &full);

// How the picture moved from one frame to another, block by block: pixel (x,y) of the one is pixel
// (x + vx,y + vy) of the other. Between the middles of the blocks, the vectors are blended.
struct field_t {
    int                         bw = 0,bh = 0;      // blocks across and down
    int                         block = 8;
    std::vector<float>          vx,vy;
    std::vector<float>          cost;               // how badly each block matches, the mean difference of its pixels
                                                    // once its brightness is made the same
    std::vector<float>          dc;                 // how much brighter each block is where it moved to

    void alloc(int width,int height,int blk) {
        block = blk;
        bw = (width + blk - 1) / blk;
        bh = (height + blk - 1) / blk;
        vx.assign((size_t)bw * bh,0.0f);
        vy.assign((size_t)bw * bh,0.0f);
        cost.assign((size_t)bw * bh,0.0f);
        dc.assign((size_t)bw * bh,0.0f);
    }
    bool empty(void) const {
        return vx.empty();
    }

    // the vector at a pixel
    void at(float x,float y,float &dx,float &dy) const {
        float bx = (x + 0.5f) / (float)block - 0.5f,by = (y + 0.5f) / (float)block - 0.5f;
        if (bx < 0.0f) bx = 0.0f;
        if (by < 0.0f) by = 0.0f;
        if (bx > (float)(bw - 1)) bx = (float)(bw - 1);
        if (by > (float)(bh - 1)) by = (float)(bh - 1);
        const int x0 = (int)bx,y0 = (int)by;
        const int x1 = x0 + 1 < bw ? x0 + 1 : x0,y1 = y0 + 1 < bh ? y0 + 1 : y0;
        const float fx = bx - (float)x0,fy = by - (float)y0;
        const size_t i00 = ((size_t)y0 * bw) + x0,i01 = ((size_t)y0 * bw) + x1,i10 = ((size_t)y1 * bw) + x0,i11 = ((size_t)y1 * bw) + x1;
        const float ax = vx[i00] + ((vx[i01] - vx[i00]) * fx),bxv = vx[i10] + ((vx[i11] - vx[i10]) * fx);
        const float ay = vy[i00] + ((vy[i01] - vy[i00]) * fx),byv = vy[i10] + ((vy[i11] - vy[i10]) * fx);
        dx = ax + ((bxv - ax) * fy);
        dy = ay + ((byv - ay) * fy);
    }

    // how much brighter the picture is where it moved to, at a pixel
    float dc_at(float x,float y) const {
        float bx = (x + 0.5f) / (float)block - 0.5f,by = (y + 0.5f) / (float)block - 0.5f;
        if (bx < 0.0f) bx = 0.0f;
        if (by < 0.0f) by = 0.0f;
        if (bx > (float)(bw - 1)) bx = (float)(bw - 1);
        if (by > (float)(bh - 1)) by = (float)(bh - 1);
        const int x0 = (int)bx,y0 = (int)by;
        const int x1 = x0 + 1 < bw ? x0 + 1 : x0,y1 = y0 + 1 < bh ? y0 + 1 : y0;
        const float fx = bx - (float)x0,fy = by - (float)y0;
        const float a = dc[((size_t)y0 * bw) + x0] + ((dc[((size_t)y0 * bw) + x1] - dc[((size_t)y0 * bw) + x0]) * fx);
        const float b = dc[((size_t)y1 * bw) + x0] + ((dc[((size_t)y1 * bw) + x1] - dc[((size_t)y1 * bw) + x0]) * fx);
        return a + ((b - a) * fy);
    }

    // how badly the blocks around a pixel match
    float cost_at(float x,float y) const {
        int bx = (int)(x / (float)block),by = (int)(y / (float)block);
        if (bx < 0) bx = 0;
        if (by < 0) by = 0;
        if (bx >= bw) bx = bw - 1;
        if (by >= bh) by = bh - 1;
        return cost[((size_t)by * bw) + bx];
    }
};

struct flow_params_t {
    int                         block = 8;          // pixels, of the blocks matched
    int                         range = 64;         // pixels the picture can move, at most, either way
    float                       smooth = 1.0f;      // cost of each pixel a block moves differently from the blocks
                                                    // around it, in levels of difference per pixel: the picture moves
                                                    // in pieces, not pixel by pixel
    float                       predict = 0.5f;     // cost of each pixel a block moves differently from the prediction
                                                    // of the frames before, if there is one
    bool                        subpel = true;      // to a quarter of a pixel, not only whole pixels
};

// How the picture moved from a to b. If pred is not NULL, it is a prediction of the vectors, as from the
// frames before going on as they moved, which the blocks that match as well either way take.
void estimate_flow(field_t &f,const pyr_t &a,const pyr_t &b,const flow_params_t &fp,const field_t *pred);

#endif //VMI_FLOW_H
