#ifndef VMI_INTERP_H
#define VMI_INTERP_H

#include <vector>

#include "flow.h"

// a frame of the input that is a real frame, not a repeat, as it is worked on
struct frame_t {
    long                        index = 0;          // of the input
    double                      time = 0;           // seconds, when it was really captured
    fimage_t                    luma;               // at the input's size, to match with
    pyr_t                       pyr;
    std::vector<fimage_t>       planes;             // its color components, upscaled, all the same size
};

// what is known of the motion around the two frames a new frame goes between
struct between_t {
    const frame_t*              a = NULL;
    const frame_t*              b = NULL;
    const field_t*              ab = NULL;          // how the picture moved from a to b
    const field_t*              ba = NULL;          // and from b to a
    const field_t*              a_prev = NULL;      // from a to the frame before it, or NULL if that is not used
    const field_t*              b_next = NULL;      // from b to the frame after it, or NULL
    double                      dt = 0;             // seconds from a to b
    double                      dt_prev = 0;        // from the frame before a to a
    double                      dt_next = 0;        // from b to the frame after it
    float                       scale = 1;          // the planes are this many times the size of the luma
};

// the frame a fraction t of the way from a to b, each pixel moved along its path from a and from b
void interpolate(std::vector<fimage_t> &out,const between_t &bt,float t);

#endif //VMI_INTERP_H
