#include <math.h>

#include <algorithm>
#include <vector>

#include "avg2y.h"

// columns done together, by one thread, each line of them one after another in memory
static const int BLOCK = 32;

// The solver is ADMM, the alternating direction method of multipliers, for
//
//     the least of  sum over y of (out[y] - (x[y] + x[y+1]) / 2)^2 / 2  +  smooth * sum over y of P(x[y+1] - x[y])
//
// where x[0] is the line above the top, which the card had but did not keep, and x[y+1] is line y.
// With d the changes from line to line, d[y] = x[y+1] - x[y], it takes turns at
//   x: the least of the first sum plus rho/2 * |D x - z + u|^2, a tridiagonal system, the same for every column
//   z: each change, shrunk toward 0 as P says
//   u: what D x and z still differ by, added up
// RHO_PER_SMOOTH sets rho for each smooth; with it about 80 turns brings each pixel to within a level of
// where any number more would.
//
// It is solved twice, for two P:
//   |d|, total variation: edges come out as sharp as the card's lines allow, with nothing either side of
//        them, but small changes, noise and fine texture, cost as much for their size as an edge, and go:
//        each column comes out smooth, and not as the columns either side of it, a picture of streaks.
//   Huber: d*d/(2*EDGE) up to EDGE, and |d| - EDGE/2 past it: small changes cost little, as a linear filter
//        that boosts the fine detail the card took out, but not the finest, which it took out altogether,
//        and noise comes out about as it went in; but the edges ring a little, as a linear filter's do.
// and each pixel is taken from the first near an edge, a change of more than EDGE_FAR in the first within
// EDGE_NEAR lines of it, and from the second away from edges, fading from one to the other.
static const float RHO_PER_SMOOTH = 0.05f;
static const float EDGE = 60.0f;
static const float EDGE_LOW = 10.0f;               // a change this big or less is not an edge
static const float EDGE_HIGH = 30.0f;              // and this big or more is
static const int EDGE_NEAR = 3;

void avg2y_undo(float *p,int w,int h,ptrdiff_t stride,const avg2y_params_t &ap) {
    if (w <= 0 || h <= 1)
        return;

    const int n = h + 1;
    const float lambda = std::max(ap.smooth,0.01f);
    const float rho = lambda * RHO_PER_SMOOTH;
    const float t = lambda / rho;

    // the system for x: diagonal b, the same c either side of it, factored once
    const float c = 0.25f - rho;
    std::vector<float> m((size_t)n),cp((size_t)n);
    for (int k=0;k < n;k++) {
        const float ata = (k == 0 || k == n - 1) ? 0.25f : 0.5f;
        const float dtd = (k == 0 || k == n - 1) ? 1.0f : 2.0f;
        const float b = ata + (rho * dtd);
        m[(size_t)k] = k == 0 ? b : b - (c * cp[(size_t)k - 1u]);
        cp[(size_t)k] = c / m[(size_t)k];
    }

    const int blocks = (w + BLOCK - 1) / BLOCK;

#pragma omp parallel for schedule(dynamic,1)
    for (int bi=0;bi < blocks;bi++) {
        const int x0 = bi * BLOCK;
        const int bw = std::min(BLOCK,w - x0);
        std::vector<float> ato((size_t)n * BLOCK),rhs((size_t)n * BLOCK);
        std::vector<float> tv((size_t)n * BLOCK),hub((size_t)n * BLOCK);
        std::vector<float> z((size_t)h * BLOCK),u((size_t)h * BLOCK);
        auto at = [](std::vector<float> &v,int row) -> float* { return &v[(size_t)row * BLOCK]; };

        // what the card made, as it bears on each line: half of each of the two lines it is the mean of
        for (int k=0;k < n;k++) {
            float *a = at(ato,k);
            for (int i=0;i < BLOCK;i++)
                a[i] = 0;
            if (k >= 1) {
                const float *o = p + ((ptrdiff_t)(k - 1) * stride) + x0;
                for (int i=0;i < bw;i++)
                    a[i] += 0.5f * o[i];
            }
            if (k < h) {
                const float *o = p + ((ptrdiff_t)k * stride) + x0;
                for (int i=0;i < bw;i++)
                    a[i] += 0.5f * o[i];
            }
        }

        auto solve = [&](std::vector<float> &x,bool huber) {
            // starting from the card's picture itself
            for (int k=0;k < n;k++) {
                const float *o = p + ((ptrdiff_t)std::max(0,k - 1) * stride) + x0;
                float *xr = at(x,k);
                for (int i=0;i < BLOCK;i++)
                    xr[i] = i < bw ? o[i] : 0.0f;
            }
            for (int k=0;k < h;k++) {
                const float *a = at(x,k),*b = at(x,k + 1);
                float *zr = at(z,k),*ur = at(u,k);
                for (int i=0;i < BLOCK;i++) {
                    zr[i] = b[i] - a[i];
                    ur[i] = 0;
                }
            }

            const float hk = EDGE / (EDGE + t);     // Huber: how much a small change is shrunk
            for (int it=0;it < ap.iterations;it++) {
                // x: the right hand side, ato + rho * D^T (z - u), and the system solved, forward and back
                for (int k=0;k < n;k++) {
                    const float *a = at(ato,k);
                    float *r = at(rhs,k);
                    for (int i=0;i < BLOCK;i++)
                        r[i] = a[i];
                    if (k >= 1) {
                        const float *zr = at(z,k - 1),*ur = at(u,k - 1);
                        for (int i=0;i < BLOCK;i++)
                            r[i] += rho * (zr[i] - ur[i]);
                    }
                    if (k < h) {
                        const float *zr = at(z,k),*ur = at(u,k);
                        for (int i=0;i < BLOCK;i++)
                            r[i] -= rho * (zr[i] - ur[i]);
                    }
                }
                {
                    float *r = at(rhs,0);
                    const float inv = 1.0f / m[0];
                    for (int i=0;i < BLOCK;i++)
                        r[i] *= inv;
                }
                for (int k=1;k < n;k++) {
                    float *r = at(rhs,k);
                    const float *rp = at(rhs,k - 1);
                    const float inv = 1.0f / m[(size_t)k];
                    for (int i=0;i < BLOCK;i++)
                        r[i] = (r[i] - (c * rp[i])) * inv;
                }
                {
                    float *xr = at(x,n - 1);
                    const float *r = at(rhs,n - 1);
                    for (int i=0;i < BLOCK;i++)
                        xr[i] = r[i];
                }
                for (int k=n - 2;k >= 0;k--) {
                    float *xr = at(x,k);
                    const float *r = at(rhs,k),*xn = at(x,k + 1);
                    const float q = cp[(size_t)k];
                    for (int i=0;i < BLOCK;i++)
                        xr[i] = r[i] - (q * xn[i]);
                }

                // z and u
                for (int k=0;k < h;k++) {
                    const float *a = at(x,k),*b = at(x,k + 1);
                    float *zr = at(z,k),*ur = at(u,k);
                    if (huber) {
                        for (int i=0;i < BLOCK;i++) {
                            const float v = (b[i] - a[i]) + ur[i];
                            const float s = v > EDGE + t ? v - t : (v < -(EDGE + t) ? v + t : v * hk);
                            zr[i] = s;
                            ur[i] = v - s;
                        }
                    }
                    else {
                        for (int i=0;i < BLOCK;i++) {
                            const float v = (b[i] - a[i]) + ur[i];
                            const float s = v > t ? v - t : (v < -t ? v + t : 0.0f);
                            zr[i] = s;
                            ur[i] = v - s;
                        }
                    }
                }
            }
        };
        solve(tv,false);
        solve(hub,true);

        // line y is x[y + 1]: from the first near an edge, from the second away from one
        for (int y=0;y < h;y++) {
            const float *a = at(tv,y + 1),*b = at(hub,y + 1);
            float *o = p + ((ptrdiff_t)y * stride) + x0;
            for (int i=0;i < bw;i++) {
                float e = 0;
                for (int k=std::max(0,y + 1 - EDGE_NEAR);k <= std::min(h - 1,y + 1 + EDGE_NEAR);k++)
                    e = std::max(e,fabsf(at(tv,k + 1)[i] - at(tv,k)[i]));
                const float wt = std::min(1.0f,std::max(0.0f,(e - EDGE_LOW) / (EDGE_HIGH - EDGE_LOW)));
                o[i] = (wt * a[i]) + ((1.0f - wt) * b[i]);
            }
        }
    }
}
