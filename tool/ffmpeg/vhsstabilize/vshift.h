#ifndef VHSSTABILIZE_VSHIFT_H
#define VHSSTABILIZE_VSHIFT_H

#include "plane.h"
#include "interp.h"

// move the picture in lines first, first+step, first+(2*step), ... of src down by 'shift'
// of those lines (up if negative), into the same lines of dst. dst and src are the same size,
// but not the same memory. The lines the picture moved away from become 'fill'.
void shift_lines(const plane_t &dst,const plane_t &src,unsigned int first,unsigned int step,double shift,interp_t interp,unsigned int fill);

#endif //VHSSTABILIZE_VSHIFT_H
