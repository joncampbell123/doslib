/* vhsstabilize: moving the picture on each line left or right, to a fraction of a sample */

#include <math.h>
#include <string.h>

#include "hshift.h"

static inline int clampi(int v,int lo,int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// positions along a line in fixed point, 24 bits of a sample, from 256 samples before the line starts,
// which is more than a line is ever moved, so they are never negative
static const int POS_BITS = 24;
static const int POS_BEFORE = 256;

template <typename T> static void shift_columns_t(const plane_t &dst,const plane_t &src,const hwarp_t warp[],interp_t interp) {
    const int w = (int)src.width;
    const int maxval = (int)src.maxval();
    int wt[256][4];

    // the weights at each 1/256 of the way from one sample to the next
    for (int i=0;i < 256;i++)
        interp_weights(wt[i],(double)i / 256.0,interp);

    for (unsigned int y=0;y < src.height;y++) {
        const T *s = (const T*)src.line(y);
        T *d = (T*)dst.line(y);
        const double a = warp[y].shift,b = warp[y].stretch;

        // a line that has not moved is copied as it is
        if (fabs(a) < (1.0 / 1024.0) && fabs(a + (b * (double)(w - 1))) < (1.0 / 1024.0)) {
            memcpy(d,s,(size_t)w * sizeof(T));
            continue;
        }

        // where in src each sample comes from: x + a + (b * x)
        int64_t pos = (int64_t)floor((a + (double)POS_BEFORE) * (double)(1 << POS_BITS));
        const int64_t step = (int64_t)floor((1.0 + b) * (double)(1 << POS_BITS) + 0.5);

        for (int x=0;x < w;x++,pos += step) {
            // to the nearest 1/256 of a sample
            const int64_t at = (pos + (1 << (POS_BITS - 9))) >> (POS_BITS - 8);
            const int i = (int)(at >> 8) - POS_BEFORE;
            const int f = (int)(at & 255);

            // whole samples are copied as they are, the ends of the line repeating past them
            if (interp == INTERP_NEAREST) {
                d[x] = s[clampi(i + (f >> 7),0,w - 1)];
                continue;
            }
            if (f == 0) {
                d[x] = s[clampi(i,0,w - 1)];
                continue;
            }

            const int *k = wt[f];
            int v;
            if (i >= 1 && (i + 2) < w)
                v = (k[0] * (int)s[i - 1]) + (k[1] * (int)s[i]) + (k[2] * (int)s[i + 1]) + (k[3] * (int)s[i + 2]);
            else
                v = (k[0] * (int)s[clampi(i - 1,0,w - 1)]) + (k[1] * (int)s[clampi(i,0,w - 1)]) +
                    (k[2] * (int)s[clampi(i + 1,0,w - 1)]) + (k[3] * (int)s[clampi(i + 2,0,w - 1)]);

            v = (v < 0) ? 0 : ((v + 8192) >> 14);
            d[x] = (T)(v > maxval ? maxval : v);
        }
    }
}

void shift_columns(const plane_t &dst,const plane_t &src,const hwarp_t warp[],interp_t interp) {
    if (src.bytes == 2)
        shift_columns_t<uint16_t>(dst,src,warp,interp);
    else
        shift_columns_t<uint8_t>(dst,src,warp,interp);
}

void plane_warps(std::vector<hwarp_t> &out,const std::vector<hwarp_t> &frame,unsigned int lines,unsigned int fields,unsigned int hsub,unsigned int vsub) {
    const unsigned int height = (unsigned int)frame.size();
    const double hs = (double)(1u << hsub);

    out.assign(lines,hwarp_t());
    for (unsigned int c=0;c < lines;c++) {
        // line c of the plane is line j of field p, and that is lines j << vsub to ((j + 1) << vsub) - 1 of the field in the frame
        const unsigned int p = c % fields,j = c / fields;
        const unsigned int fl = count_lines(height,p,fields);
        double a = 0,b = 0;
        unsigned int cnt = 0;

        for (unsigned int k=j << vsub;k < ((j + 1u) << vsub) && k < fl;k++) {
            a += frame[p + (k * fields)].shift;
            b += frame[p + (k * fields)].stretch;
            cnt++;
        }

        // the stretch is the same in samples of any width, the shift is not
        if (cnt != 0u) {
            out[c].shift = a / ((double)cnt * hs);
            out[c].stretch = b / (double)cnt;
        }
    }
}
