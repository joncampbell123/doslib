#ifndef VHSSTABILIZE_HSHIFT_H
#define VHSSTABILIZE_HSHIFT_H

#include <vector>

#include "plane.h"
#include "interp.h"

// how far the picture on one line has moved to the right: what should be at x of the line
// is at x + shift + (stretch * x) instead, in samples of the plane
struct hwarp_t {
    double                      shift = 0;
    double                      stretch = 0;
};

// move the picture on each line of src back to where it should be, as warp says it moved, into
// the same line of dst. warp has one hwarp_t for each line. dst and src are the same size, but
// not the same memory. Past the ends of a line, its first and last samples repeat.
void shift_columns(const plane_t &dst,const plane_t &src,const hwarp_t warp[],interp_t interp);

// how far the picture moved on each line of a plane whose samples are 1 << hsub samples of the
// frame wide and whose lines are 1 << vsub lines of a field high, with the lines of 'fields'
// fields one after the other as in the frame, from how far it moved on each line of the frame
void plane_warps(std::vector<hwarp_t> &out,const std::vector<hwarp_t> &frame,unsigned int lines,unsigned int fields,unsigned int hsub,unsigned int vsub);

#endif //VHSSTABILIZE_HSHIFT_H
