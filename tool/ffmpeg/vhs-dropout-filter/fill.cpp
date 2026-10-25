#include <math.h>
#include <string.h>

#include <algorithm>
#include <vector>

#include "fill.h"
#include "detect.h"

void plane_mask(mask_t &pm,const mask_t &mask,int lw,int lh,const plane16_t &pl) {
    if (pl.hsub == 0u && pl.vsub == 0u) {
        pm = mask;
        return;
    }

    pm.assign((size_t)pl.width * pl.height,0);
    for (int y=0;y < lh;y++) {
        const uint8_t *m = &mask[(size_t)y * lw];
        const unsigned int py = (unsigned int)y >> pl.vsub;
        if (py >= pl.height)
            continue;
        uint8_t *d = &pm[(size_t)py * pl.width];
        for (int x=0;x < lw;x++) {
            const unsigned int px = (unsigned int)x >> pl.hsub;
            if (m[x] && px < pl.width)
                d[px] = 1;
        }
    }
}

// what is still not filled in (done[i] == 0), from the samples above and below it in the same column,
// blended from one to the other, or the one there is if there is only one
static size_t fill_columns(plane16_t &out,const mask_t &pm,const mask_t &done) {
    const int w = (int)out.width,h = (int)out.height;
    size_t count = 0;

    #pragma omp parallel for schedule(static) reduction(+:count)
    for (int x=0;x < w;x++) {
        int y = 0;
        while (y < h) {
            const size_t i = ((size_t)y * w) + x;
            if (!pm[i] || done[i]) {
                y++;
                continue;
            }

            const int s = y;
            while (y < h && pm[((size_t)y * w) + x] && !done[((size_t)y * w) + x]) y++;

            const bool above = s > 0,below = y < h;
            const float a = above ? (float)out.line((unsigned int)(s - 1))[x] : 0.0f;
            const float b = below ? (float)out.line((unsigned int)y)[x] : 0.0f;
            for (int j=s;j < y;j++) {
                float v;
                if (above && below)
                    v = a + ((b - a) * (float)(j - s + 1) / (float)(y - s + 1));
                else if (above)
                    v = a;
                else if (below)
                    v = b;
                else
                    continue;
                out.line((unsigned int)j)[x] = (uint16_t)std::min(65535.0f,std::max(0.0f,v + 0.5f));
                count++;
            }
        }
    }

    return count;
}

// the sums of v and of c over each box of the plane, from tables of the sums from the top left corner
struct box_sums_t {
    int                         w = 0,h = 0;
    std::vector<double>         sv,sc;

    void build(const std::vector<float> &v,const std::vector<float> &c,int width,int height) {
        w = width;
        h = height;
        sv.assign((size_t)(w + 1) * (h + 1),0.0);
        sc.assign((size_t)(w + 1) * (h + 1),0.0);
        for (int y=0;y < h;y++) {
            double rv = 0,rc = 0;
            for (int x=0;x < w;x++) {
                rv += v[((size_t)y * w) + x];
                rc += c[((size_t)y * w) + x];
                sv[((size_t)(y + 1) * (w + 1)) + x + 1] = sv[((size_t)y * (w + 1)) + x + 1] + rv;
                sc[((size_t)(y + 1) * (w + 1)) + x + 1] = sc[((size_t)y * (w + 1)) + x + 1] + rc;
            }
        }
    }

    void sum(int x0,int y0,int x1,int y1,double &v,double &c) const {
        x0 = std::max(0,x0); y0 = std::max(0,y0);
        x1 = std::min(w,x1); y1 = std::min(h,y1);
        const size_t a = ((size_t)y0 * (w + 1)) + x0,b = ((size_t)y0 * (w + 1)) + x1;
        const size_t d = ((size_t)y1 * (w + 1)) + x0,e = ((size_t)y1 * (w + 1)) + x1;
        v = sv[e] - sv[b] - sv[d] + sv[a];
        c = sc[e] - sc[b] - sc[d] + sc[a];
    }
};

void fill_motion(std::vector<plane16_t> &out,const std::vector<plane16_t> &cur,const mask_t &mask,int lw,int lh,
    const std::vector<fill_source_t> &src,const fill_params_t &fp,fill_stats_t &fs,
    const std::vector<plane16_t> *fallback) {
    out = cur;

    for (size_t p=0;p < cur.size();p++) {
        const plane16_t &cp = cur[p];
        plane16_t &op = out[p];
        const int w = (int)cp.width,h = (int)cp.height;
        const size_t n = (size_t)w * h;
        const float sx = (float)(1u << cp.hsub),sy = (float)(1u << cp.vsub);
        const int mx = std::max(1,fp.match_x >> cp.hsub),my = std::max(1,fp.match_y >> cp.vsub);
        mask_t pm,near;
        std::vector<mask_t> own(src.size());
        std::vector<const mask_t*> spm(src.size());

        plane_mask(pm,mask,lw,lh,cp);
        for (size_t s=0;s < src.size();s++) {
            if (src[s].pmasks != NULL && p < src[s].pmasks->size()) {
                spm[s] = &(*src[s].pmasks)[p];
            }
            else {
                plane_mask(own[s],*src[s].mask,lw,lh,(*src[s].pl)[p]);
                spm[s] = &own[s];
            }
        }

        // the pixels to make a prediction of: the dropouts, and around them, to match the brightness to
        near = pm;
        if (fp.match)
            grow_mask(near,w,h,mx,my);

        std::vector<float> pred(n,0.0f),diff(n,0.0f),have(n,0.0f);
        mask_t done(n,0),from_donor(n,0);

        #pragma omp parallel for schedule(dynamic,4)
        for (int y=0;y < h;y++) {
            for (int x=0;x < w;x++) {
                const size_t i = ((size_t)y * w) + x;
                if (!near[i])
                    continue;

                // where the pixel is in luma
                const float lx = ((float)x * sx) + ((sx - 1.0f) * 0.5f);
                const float ly = ((float)y * sy) + ((sy - 1.0f) * 0.5f);
                float sw = 0,sv = 0;
                int top = -1;
                float cv[16],cw[16];                // what each source of the top priority says, and how much it counts
                int nc = 0;

                for (size_t s=0;s < src.size();s++) {
                    if (src[s].priority < top)
                        continue;
                    const plane16_t &rp = (*src[s].pl)[p];
                    float dx,dy,c;

                    src[s].mv->at(lx,ly,dx,dy,&c);
                    if (c > src[s].max_cost)
                        continue;
                    const float rx = (float)x + (dx / sx),ry = (float)y + (dy / sy);
                    const int ix = (int)floorf(rx),iy = (int)floorf(ry);

                    // none of the samples it is made from can be a dropout
                    bool bad = false;
                    for (int j=iy;j <= iy + 1 && !bad;j++) {
                        const int cj = std::min(std::max(j,0),h - 1);
                        for (int k=ix;k <= ix + 1 && !bad;k++) {
                            const int ck = std::min(std::max(k,0),w - 1);
                            if ((*spm[s])[((size_t)cj * w) + ck])
                                bad = true;
                        }
                    }
                    if (bad)
                        continue;

                    // the first of a higher priority puts aside what came before
                    if (src[s].priority > top) {
                        top = src[s].priority;
                        sw = sv = 0;
                        nc = 0;
                    }
                    const float wt = expf(-c / fp.cost_scale) / (float)src[s].distance;
                    const float v = sample_bilinear(rp,rx,ry);
                    sv += wt * v;
                    sw += wt;
                    if (nc < 16) {
                        cv[nc] = v;
                        cw[nc] = wt;
                        nc++;
                    }
                }

                // Where other copies of the tape do not agree, in a dropout, one that is much the same as
                // the dropout is likely to have the same dropout, as another playback of a tape with a
                // crease in it does, and is left out, where another is not like the dropout at all.
                if (pm[i] && top > 0 && nc >= 2) {
                    const float here = (float)cp.v[i];
                    float most = 0;
                    for (int k=0;k < nc;k++)
                        most = std::max(most,fabsf(cv[k] - here));
                    if (most > 12.0f * 256.0f) {
                        sv = sw = 0;
                        for (int k=0;k < nc;k++) {
                            if (fabsf(cv[k] - here) >= 0.4f * most) {
                                sv += cw[k] * cv[k];
                                sw += cw[k];
                            }
                        }
                    }
                }

                if (sw > 0.0f) {
                    pred[i] = sv / sw;
                    if (pm[i]) {
                        done[i] = 1;
                        from_donor[i] = top > 0 ? 1 : 0;
                    }
                    else {
                        // not where they differ a lot, which is more likely a dropout that was not found,
                        // or something that moved, than how much brighter one is than the other
                        const float d = (float)cp.v[i] - pred[i];
                        if (fabsf(d) <= 16.0f * 256.0f) {
                            diff[i] = d;
                            have[i] = 1.0f;
                        }
                    }
                }
            }
        }

        // what is filled in is moved up or down to the brightness of the picture around it,
        // the mean of how much brighter the picture is than the prediction, around it
        box_sums_t bs;
        if (fp.match)
            bs.build(diff,have,w,h);

        size_t count = 0,dcount = 0;
        #pragma omp parallel for schedule(static) reduction(+:count,dcount)
        for (int y=0;y < h;y++) {
            uint16_t *o = op.line((unsigned int)y);
            for (int x=0;x < w;x++) {
                const size_t i = ((size_t)y * w) + x;
                if (!done[i])
                    continue;

                float v = pred[i];
                if (fp.match) {
                    double ds,cs;
                    bs.sum(x - mx,y - my,x + mx + 1,y + my + 1,ds,cs);
                    if (cs >= 1.0)
                        v += (float)(ds / cs);
                }
                o[x] = (uint16_t)std::min(65535.0f,std::max(0.0f,v + 0.5f));
                count++;
                dcount += from_donor[i];
            }
        }

        // what nothing filled in, from what was held, or the lines above and below
        size_t left = 0,held = 0;
        if (fallback != NULL) {
            const plane16_t &fb = (*fallback)[p];
            for (size_t i=0;i < n;i++) {
                if (pm[i] && !done[i]) {
                    op.v[i] = fb.v[i];
                    held++;
                }
            }
        }
        else {
            left = fill_columns(op,pm,done);
        }
        if (p == 0) {
            fs.temporal += count + held;
            fs.donor += dcount;
            fs.spatial += left;
        }
    }
}

void fill_hold(std::vector<plane16_t> &out,const std::vector<plane16_t> &cur,const mask_t &mask,int lw,int lh,
    const std::vector<plane16_t> *held,fill_stats_t &fs) {
    out = cur;

    for (size_t p=0;p < cur.size();p++) {
        plane16_t &op = out[p];
        const size_t n = (size_t)op.width * op.height;
        mask_t pm;

        plane_mask(pm,mask,lw,lh,op);
        if (held != NULL) {
            const plane16_t &hp = (*held)[p];
            size_t count = 0;
            for (size_t i=0;i < n;i++) {
                if (pm[i]) {
                    op.v[i] = hp.v[i];
                    count++;
                }
            }
            if (p == 0)
                fs.temporal += count;
        }
        else {
            const size_t count = fill_columns(op,pm,mask_t(n,0));
            if (p == 0)
                fs.spatial += count;
        }
    }
}
