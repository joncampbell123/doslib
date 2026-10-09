#ifndef VHSSTABILIZE_INTERP_H
#define VHSSTABILIZE_INTERP_H

#include <math.h>

enum interp_t {
    INTERP_NEAREST=0,                               // whole lines or samples only, nothing blurred
    INTERP_LINEAR,
    INTERP_CUBIC                                    // Catmull-Rom
};

// the weights of the samples before, at, after, and after that of a point f (0 < f < 1) of the way
// from one sample to the next, in 14-bit fixed point that add up to exactly 1 << 14
static inline void interp_weights(int w[4],double f,interp_t interp) {
    if (interp == INTERP_LINEAR) {
        w[0] = 0;
        w[1] = (int)floor(((1.0 - f) * 16384.0) + 0.5);
        w[2] = 16384 - w[1];
        w[3] = 0;
    }
    else {
        const double f2 = f * f,f3 = f2 * f;
        w[0] = (int)floor((0.5 * (-f3 + (2.0 * f2) - f) * 16384.0) + 0.5);
        w[2] = (int)floor((0.5 * ((-3.0 * f3) + (4.0 * f2) + f) * 16384.0) + 0.5);
        w[3] = (int)floor((0.5 * (f3 - f2) * 16384.0) + 0.5);
        w[1] = 16384 - w[0] - w[2] - w[3];
    }
}

#endif //VHSSTABILIZE_INTERP_H
