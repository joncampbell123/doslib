#include <math.h>
#include <string.h>

#include <algorithm>
#include <vector>

#include "detect.h"

// the mean of in over box pixels across centered on each pixel, the ends of the line going on outside it
static void box_row(float *out,const float *in,int w,int box) {
    const int r = box / 2;
    float sum = 0;

    for (int x=-r;x <= r;x++)
        sum += in[std::min(std::max(x,0),w - 1)];
    for (int x=0;x < w;x++) {
        out[x] = sum / (float)((r * 2) + 1);
        sum += in[std::min(x + r + 1,w - 1)] - in[std::max(x - r,0)];
    }
}

// |a - b| along a line, boxed
static void absdiff_row(float *out,float *tmp,const float *a,const float *b,int w,int box) {
    for (int x=0;x < w;x++)
        tmp[x] = fabsf(a[x] - b[x]);
    box_row(out,tmp,w,box);
}

// how far each pixel is from halfway between the pixels left and right of it, boxed: the fine detail across a line
static void hdetail_row(float *out,float *tmp,const float *a,int w,int box) {
    for (int x=0;x < w;x++) {
        const float l = a[x > 0 ? x - 1 : 0],r = a[x + 1 < w ? x + 1 : w - 1];
        tmp[x] = fabsf(a[x] - ((l + r) * 0.5f));
    }
    box_row(out,tmp,w,box);
}

// keep only the runs of each line at least min_run long, after filling in the gaps of up to gap pixels between them
static void keep_runs(mask_t &mask,int w,int h,int min_run,int gap) {
    #pragma omp parallel for schedule(static)
    for (int y=0;y < h;y++) {
        uint8_t *m = &mask[(size_t)y * w];
        int x = 0;

        // fill in the gaps
        int last_end = -1;
        while (x < w) {
            if (!m[x]) {
                x++;
                continue;
            }
            if (last_end >= 0 && x - last_end <= gap)
                memset(m + last_end,1,(size_t)(x - last_end));
            while (x < w && m[x]) x++;
            last_end = x;
        }

        // and take out what is too short
        x = 0;
        while (x < w) {
            if (!m[x]) {
                x++;
                continue;
            }
            const int s = x;
            while (x < w && m[x]) x++;
            if (x - s < min_run)
                memset(m + s,0,(size_t)(x - s));
        }
    }
}

void grow_mask(mask_t &mask,int w,int h,int gx,int gy) {
    if (gx > 0) {
        // a line of it for each thread, as allocating one for each line keeps the threads waiting for each other
        #pragma omp parallel
        {
            std::vector<uint8_t> o((size_t)w);
            #pragma omp for schedule(static)
            for (int y=0;y < h;y++) {
                uint8_t *m = &mask[(size_t)y * w];
                memcpy(&o[0],m,(size_t)w);
                for (int x=0;x < w;x++) {
                    if (!m[x])
                        continue;
                    for (int i=std::max(0,x - gx);i <= std::min(w - 1,x + gx);i++)
                        o[i] = 1;
                }
                memcpy(m,&o[0],(size_t)w);
            }
        }
    }
    if (gy > 0) {
        const mask_t o = mask;

        #pragma omp parallel for schedule(static)
        for (int y=0;y < h;y++) {
            uint8_t *m = &mask[(size_t)y * w];
            for (int j=std::max(0,y - gy);j <= std::min(h - 1,y + gy);j++) {
                const uint8_t *s = &o[(size_t)j * w];
                for (int x=0;x < w;x++)
                    m[x] |= s[x];
            }
        }
    }
}

void detect_dropouts(mask_t &mask,std::vector<float> &E,const image_t &cur,const std::vector<const image_t*> &pred,
    const std::vector<const std::vector<float>*> &prevE,const detect_params_t &dp,detect_stats_t &st) {
    const int w = cur.w,h = cur.h;
    const size_t n = (size_t)w * h;
    const int np = (int)pred.size();

    st = detect_stats_t();
    mask.assign(n,0);
    E.assign(n,0.0f);
    if (np == 0 || w < 8 || h < 8)
        return;

    const int top = std::min(std::max(dp.margin[0],1),h - 1);
    const int bottom = std::max(top,h - std::max(dp.margin[1],1));
    const int left = std::min(std::max(dp.margin[2],0),w);
    const int right = std::max(left,w - std::max(dp.margin[3],0));

    // How much each pixel differs from each prediction, along the line, and how much from the
    // prediction it differs least from but one: a dropout differs from all of them, but one of them
    // may be a dropout in the same place. With only one or two predictions, the least.
    std::vector<uint8_t> best(n,0);

    #pragma omp parallel
    {
    std::vector<float> d((size_t)w),tmp((size_t)w),m1((size_t)w),m2((size_t)w);
    #pragma omp for schedule(static)
    for (int y=0;y < h;y++) {
        std::fill(m1.begin(),m1.end(),1e30f);
        std::fill(m2.begin(),m2.end(),1e30f);
        uint8_t *bi = &best[(size_t)y * w];

        for (int k=0;k < np;k++) {
            absdiff_row(&d[0],&tmp[0],cur.row(y),pred[k]->row(y),w,dp.box);
            for (int x=0;x < w;x++) {
                if (d[x] < m1[x]) {
                    m2[x] = m1[x];
                    m1[x] = d[x];
                    bi[x] = (uint8_t)k;
                }
                else if (d[x] < m2[x]) {
                    m2[x] = d[x];
                }
            }
        }

        float *e = &E[(size_t)y * w];
        for (int x=0;x < w;x++)
            e[x] = (np >= 3 && !dp.least_of) ? m2[x] : m1[x];
    }
    }

    // the typical difference, from the median of every 4th pixel of every 4th line
    {
        std::vector<float> s;
        for (int y=top;y < bottom;y += 4) {
            for (int x=left;x < right;x += 4)
                s.push_back(E[((size_t)y * w) + x]);
        }
        if (!s.empty()) {
            std::nth_element(s.begin(),s.begin() + (ptrdiff_t)(s.size() / 2u),s.end());
            st.noise = s[s.size() / 2u];
        }
    }

    const float thr = std::max(dp.threshold,dp.noise * st.noise);
    st.threshold = thr;

    // How much the picture differs from the pictures around it as a rule, near each pixel: water, leaves
    // and crowds differ a lot all over, and a dropout differs a lot more than the lines above and below it.
    // It is the lower quarter of the differences of strips 16 pixels across, of the lines above and below.
    std::vector<float> T(n,thr);
    if (dp.local > 0.0f) {
        const int bs = 16,nbx = (w + bs - 1) / bs;
        const int vr = std::max(1,dp.local_lines);
        std::vector<float> eb((size_t)nbx * h),bg((size_t)nbx * h),pb((size_t)nbx * h,0.0f);

        // and in the same place in the pictures just before, the least of them, over a few lines
        if (!prevE.empty()) {
            std::vector<float> pr((size_t)nbx * h);

            #pragma omp parallel for schedule(static)
            for (int y=0;y < h;y++) {
                for (int bx=0;bx < nbx;bx++) {
                    const int x0 = bx * bs,x1 = std::min(w,x0 + bs);
                    float least = 1e30f;
                    for (const std::vector<float> *pe : prevE) {
                        if (pe->size() != n)
                            continue;
                        const float *r = &(*pe)[(size_t)y * w];
                        float sum = 0;
                        for (int x=x0;x < x1;x++)
                            sum += r[x];
                        least = std::min(least,sum / (float)(x1 - x0));
                    }
                    pr[((size_t)y * nbx) + bx] = least < 1e29f ? least : 0.0f;
                }
            }

            #pragma omp parallel for schedule(static)
            for (int y=0;y < h;y++) {
                for (int bx=0;bx < nbx;bx++) {
                    float most = 0;
                    for (int j=std::max(0,y - 2);j <= std::min(h - 1,y + 2);j++)
                        most = std::max(most,pr[((size_t)j * nbx) + bx]);
                    pb[((size_t)y * nbx) + bx] = most;
                }
            }
        }

        #pragma omp parallel for schedule(static)
        for (int y=0;y < h;y++) {
            const float *e = &E[(size_t)y * w];
            for (int bx=0;bx < nbx;bx++) {
                const int x0 = bx * bs,x1 = std::min(w,x0 + bs);
                float sum = 0;
                for (int x=x0;x < x1;x++)
                    sum += e[x];
                eb[((size_t)y * nbx) + bx] = sum / (float)(x1 - x0);
            }
        }

        // the lower quarter, of every 4th line, from about 25 lines of those above and below it, which is
        // plenty to tell how much they differ as a rule, and quick even for many lines either way;
        // the lines between are blended from them
        const int ystep = 4;
        const int stride = std::max(1,((vr * 2) + 1) / 25);
        const int nrows = ((h - 1) / ystep) + 2;
        std::vector<float> bq((size_t)nrows * nbx,0.0f);

        #pragma omp parallel for schedule(static)
        for (int r=0;r < nrows;r++) {
            const int y = std::min(h - 1,r * ystep);
            std::vector<float> v;
            v.reserve((size_t)(((vr * 2) / stride) + 2) * 3u);
            for (int bx=0;bx < nbx;bx++) {
                v.clear();
                for (int j=std::max(top,y - vr);j <= std::min(bottom - 1,y + vr);j += stride) {
                    for (int k=std::max(0,bx - 1);k <= std::min(nbx - 1,bx + 1);k++)
                        v.push_back(eb[((size_t)j * nbx) + k]);
                }
                if (v.empty())
                    continue;
                std::nth_element(v.begin(),v.begin() + (ptrdiff_t)(v.size() / 4u),v.end());
                bq[((size_t)r * nbx) + bx] = v[v.size() / 4u];
            }
        }

        #pragma omp parallel for schedule(static)
        for (int y=0;y < h;y++) {
            const int r0 = y / ystep,r1 = std::min(nrows - 1,r0 + 1);
            const float a = (float)(y - (r0 * ystep)) / (float)ystep;
            for (int bx=0;bx < nbx;bx++) {
                const float q = (bq[((size_t)r0 * nbx) + bx] * (1.0f - a)) + (bq[((size_t)r1 * nbx) + bx] * a);
                bg[((size_t)y * nbx) + bx] = std::max(q,dp.prev * pb[((size_t)y * nbx) + bx]);
            }

            // between the middles of the strips, blended
            float *t = &T[(size_t)y * w];
            const float *b = &bg[(size_t)y * nbx];
            for (int x=0;x < w;x++) {
                float f = ((float)x - (float)(bs / 2)) / (float)bs;
                if (f < 0.0f) f = 0.0f;
                if (f > (float)(nbx - 1)) f = (float)(nbx - 1);
                const int i0 = (int)f,i1 = std::min(i0 + 1,nbx - 1);
                const float a = f - (float)i0;
                t[x] = std::max(thr,dp.local * ((b[i0] * (1.0f - a)) + (b[i1] * a)));
            }
        }
    }

    // Where it differs enough, whether it looks like a dropout, against the picture it differs least from:
    //   the same as the lines above it where that picture is not, the line repeated by the dropout
    //     compensator of the VCR (1 line above, or 2, which is the line above in the same field when
    //     each frame is a field made whole),
    //   or noisy across the line where that picture is not, and more than the lines a few above and
    //     below, a line of static,
    //   or differing much more than that, and much more than the lines a few above and below, a line
    //     that is not there at all.
    // Water, leaves and things that move some other way differ too, but are like that in the lines
    // above and below as well, and are not copies of the line above.
    mask_t seed(n,0),weak(n,0);
    const int far = std::max(3,dp.local_lines / 4);

    #pragma omp parallel
    {
    std::vector<float> tmp((size_t)w),c1((size_t)w),c2((size_t)w),ch((size_t)w),cha((size_t)w),chb((size_t)w),hg((size_t)w);
    std::vector<float> p1((size_t)w),p2((size_t)w),ph((size_t)w);
    #pragma omp for schedule(dynamic,8)
    for (int y=top;y < bottom;y++) {
        const float *e = &E[(size_t)y * w];
        const float *ea = &E[(size_t)std::max(0,y - far) * w];
        const float *eb = &E[(size_t)std::min(h - 1,y + far) * w];
        const float *t = &T[(size_t)y * w];
        const uint8_t *bi = &best[(size_t)y * w];
        uint8_t *sd = &seed[(size_t)y * w];
        uint8_t *wk = &weak[(size_t)y * w];
        const int y2 = std::max(0,y - 2);
        bool any = false;

        for (int x=left;x < right;x++) {
            if (e[x] >= t[x] * dp.low) {
                wk[x] = 1;
                any = true;
            }
        }
        if (!any)
            continue;

        absdiff_row(&c1[0],&tmp[0],cur.row(y),cur.row(y - 1),w,dp.box);
        absdiff_row(&c2[0],&tmp[0],cur.row(y),cur.row(y2),w,dp.box);

        // the same as the lines above it, where there is enough across the line that a picture would
        // not be, a line repeated whatever the pictures around it are like, as long as it differs from them
        {
            const float *r = cur.row(y);
            for (int x=0;x < w;x++)
                tmp[x] = fabsf(r[x + 1 < w ? x + 1 : x] - r[x > 0 ? x - 1 : x]) * 0.5f;
            box_row(&hg[0],&tmp[0],w,dp.box + 6);
        }
        for (int x=left;x < right;x++) {
            if (dp.same_as_above && wk[x] && hg[x] >= dp.repeat_detail && std::max(c1[x],c2[x]) < dp.repeat_ratio * hg[x])
                sd[x] = 1;
        }
        hdetail_row(&ch[0],&tmp[0],cur.row(y),w,dp.box);
        hdetail_row(&cha[0],&tmp[0],cur.row(std::max(0,y - far)),w,dp.box);
        hdetail_row(&chb[0],&tmp[0],cur.row(std::min(h - 1,y + far)),w,dp.box);

        for (int k=0;k < np;k++) {
            bool used = false;
            const float least = dp.repeated_low ? dp.low : 1.0f;
            for (int x=left;x < right && !used;x++)
                used = (e[x] >= t[x] * least && bi[x] == k);
            if (!used)
                continue;

            const image_t &p = *pred[k];
            absdiff_row(&p1[0],&tmp[0],p.row(y),p.row(y - 1),w,dp.box);
            absdiff_row(&p2[0],&tmp[0],p.row(y),p.row(y2),w,dp.box);
            hdetail_row(&ph[0],&tmp[0],p.row(y),w,dp.box);
            if (dp.pred_detail != 1.0f) {
                for (int x=0;x < w;x++) {
                    p1[x] *= dp.pred_detail;
                    p2[x] *= dp.pred_detail;
                    ph[x] *= dp.pred_detail;
                }
            }

            for (int x=left;x < right;x++) {
                if (e[x] < t[x] * least || bi[x] != k)
                    continue;

                const bool thin = std::max(ea[x],eb[x]) < (0.5f * e[x]);
                const bool repeated = (std::max(c1[x],c2[x]) + 0.3f) < (0.5f * std::max(p1[x],p2[x]));
                const bool noisy = ch[x] > ((1.5f * ph[x]) + 2.0f) && ch[x] > ((1.5f * std::max(cha[x],chb[x])) + 1.0f);
                const bool strong = (dp.strong_lines && thin && e[x] >= t[x] * dp.strong) ||
                                    (dp.strong_bands && e[x] >= t[x] * dp.strong);

                if (repeated || (e[x] >= t[x] && (noisy || strong)))
                    sd[x] = 1;
            }
        }
    }
    }

    // a dropout is long across a line, not a few pixels, which is a part of the picture that moved
    // some other way than the pictures around it
    keep_runs(seed,w,h,std::max(1,dp.min_run / 2),dp.gap);

    // and goes on along the line into whatever around it differs less, but still a lot.
    // Not up and down: each line of a dropout is a dropout of its own.
    #pragma omp parallel for schedule(static)
    for (int y=0;y < h;y++) {
        const uint8_t *sd = &seed[(size_t)y * w];
        const uint8_t *wk = &weak[(size_t)y * w];
        uint8_t *m = &mask[(size_t)y * w];
        int x = 0;

        while (x < w) {
            if (!wk[x] && !sd[x]) {
                x++;
                continue;
            }
            const int s = x;
            bool any = false;
            while (x < w && (wk[x] || sd[x])) {
                any = any || sd[x];
                x++;
            }
            if (any)
                memset(m + s,1,(size_t)(x - s));
        }
    }

    keep_runs(mask,w,h,dp.min_run,dp.gap);

    // and on as many lines one after another as it has to be: each run of a line is kept only if it
    // is part of runs that touch from line to line, down that many lines
    if (dp.min_lines > 1) {
        // how many lines of runs touching each other each pixel is in, going down, and going up
        std::vector<uint16_t> dn(n,0),up(n,0);
        for (int y=0;y < h;y++) {
            for (int x=0;x < w;x++) {
                const size_t i = ((size_t)y * w) + x;
                if (mask[i])
                    up[i] = (uint16_t)std::min(65535,1 + (y > 0 ? (int)up[i - (size_t)w] : 0));
            }
        }
        for (int y=h - 1;y >= 0;y--) {
            for (int x=0;x < w;x++) {
                const size_t i = ((size_t)y * w) + x;
                if (mask[i])
                    dn[i] = (uint16_t)std::min(65535,1 + (y + 1 < h ? (int)dn[i + (size_t)w] : 0));
            }
        }
        mask_t keep(n,0);
        for (int y=0;y < h;y++) {
            const size_t r = (size_t)y * w;
            int x = 0;
            while (x < w) {
                if (!mask[r + x]) {
                    x++;
                    continue;
                }
                const int s0 = x;
                int most = 0;
                while (x < w && mask[r + x]) {
                    most = std::max(most,(int)up[r + x] + (int)dn[r + x] - 1);
                    x++;
                }
                if (most >= dp.min_lines)
                    memset(&keep[r + s0],1,(size_t)(x - s0));
            }
        }
        mask.swap(keep);
    }

    size_t count = 0;
    for (size_t i=0;i < n;i++)
        count += mask[i];
    if (count > (size_t)((double)dp.max_area * (double)(right - left) * (double)(bottom - top))) {
        st.too_much = true;
        mask.assign(n,0);
        return;
    }

    grow_mask(mask,w,h,dp.grow_x,dp.grow_y);

    for (int y=0;y < h;y++) {
        const uint8_t *m = &mask[(size_t)y * w];
        size_t c = 0;
        for (int x=0;x < w;x++)
            c += m[x];
        st.pixels += c;
        if (c != 0)
            st.lines++;
    }
}
