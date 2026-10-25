#ifndef CCDEFUZZ_AVG2Y_H
#define CCDEFUZZ_AVG2Y_H

#include <stddef.h>

// avg2y: a capture card that made each line of a field the mean of the line and the line above it,
//
//     out[y] = (in[y-1] + in[y]) / 2
//
// as one did capturing 320x240 video from one field. That takes out the finest detail from line to line
// altogether: a pattern of lines alternating light and dark comes out flat. So it cannot be undone as it
// was done, in[y] = 2 * out[y] - in[y-1], which makes any noise, or the MPEG compression of it, into
// stripes that go on down the picture. Instead each column is the picture that, made the mean of each two
// lines, is most like what the card made, and has the fewest, smallest changes from line to line, counted
// two ways (see avg2y.cpp): one that makes edges sharp, without the ripples (ringing) either side of them
// that a linear filter that boosted the finest detail would put there; and one that keeps the noise and
// texture as they were, which the first smooths away, column by column, into streaks. Each pixel is taken
// from the first near an edge, and from the second elsewhere.

struct avg2y_params_t {
    float                       smooth = 3.0f;      // what each level of change from line to line costs, against
                                                    // each level of difference from the card's picture: more boosts
                                                    // the finest detail less, and the noise with it
    int                         iterations = 80;    // of the solver: more is nearer the best picture
};

// Undo it down each column of a plane of w by h samples of 0 to 255 (it may go outside that), each line
// stride floats after the one before, in place. The lines are of one field, as the card made them.
// The picture comes out half a line lower than the card's, where the lines of the field were.
void avg2y_undo(float *p,int w,int h,ptrdiff_t stride,const avg2y_params_t &ap);

#endif //CCDEFUZZ_AVG2Y_H
