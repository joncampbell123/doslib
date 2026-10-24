#ifndef VHSDROPOUT_PLANE_H
#define VHSDROPOUT_PLANE_H

#include <stddef.h>
#include <stdint.h>

#include <vector>

// one plane of a picture as it is worked on, 16 bits per sample whatever the depth of the video,
// the samples of video of fewer bits moved up to the top bits
struct plane16_t {
    std::vector<uint16_t>       v;
    unsigned int                width = 0;
    unsigned int                height = 0;
    unsigned int                hsub = 0;           // log2 of the horizontal subsampling against luma
    unsigned int                vsub = 0;           // log2 of the vertical subsampling against luma

    void alloc(unsigned int w,unsigned int h) {
        width = w;
        height = h;
        v.assign((size_t)w * h,0);
    }
    uint16_t *line(unsigned int y) {
        return &v[(size_t)y * width];
    }
    const uint16_t *line(unsigned int y) const {
        return &v[(size_t)y * width];
    }
};

// luma of a picture, 0.0 (lowest code) to 255.0 (highest), to measure with
struct image_t {
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
};

// a mask of the pixels of a picture's luma, 0 where the picture is good, nonzero where it is a dropout
typedef std::vector<uint8_t> mask_t;

// the image at the point (x,y), between samples, the edges of the image going on outside it
static inline float sample_bilinear(const image_t &im,float x,float y) {
    if (x < 0.0f) x = 0.0f;
    if (y < 0.0f) y = 0.0f;
    if (x > (float)(im.w - 1)) x = (float)(im.w - 1);
    if (y > (float)(im.h - 1)) y = (float)(im.h - 1);

    const int x0 = (int)x,y0 = (int)y;
    const int x1 = x0 + 1 < im.w ? x0 + 1 : x0;
    const int y1 = y0 + 1 < im.h ? y0 + 1 : y0;
    const float fx = x - (float)x0,fy = y - (float)y0;
    const float *r0 = im.row(y0),*r1 = im.row(y1);
    const float a = r0[x0] + ((r0[x1] - r0[x0]) * fx);
    const float b = r1[x0] + ((r1[x1] - r1[x0]) * fx);

    return a + ((b - a) * fy);
}

// the same for a plane of 16-bit samples
static inline float sample_bilinear(const plane16_t &pl,float x,float y) {
    if (x < 0.0f) x = 0.0f;
    if (y < 0.0f) y = 0.0f;
    if (x > (float)(pl.width - 1u)) x = (float)(pl.width - 1u);
    if (y > (float)(pl.height - 1u)) y = (float)(pl.height - 1u);

    const unsigned int x0 = (unsigned int)x,y0 = (unsigned int)y;
    const unsigned int x1 = x0 + 1u < pl.width ? x0 + 1u : x0;
    const unsigned int y1 = y0 + 1u < pl.height ? y0 + 1u : y0;
    const float fx = x - (float)x0,fy = y - (float)y0;
    const uint16_t *r0 = pl.line(y0),*r1 = pl.line(y1);
    const float a = (float)r0[x0] + (((float)r0[x1] - (float)r0[x0]) * fx);
    const float b = (float)r1[x0] + (((float)r1[x1] - (float)r1[x0]) * fx);

    return a + ((b - a) * fy);
}

#endif //VHSDROPOUT_PLANE_H
