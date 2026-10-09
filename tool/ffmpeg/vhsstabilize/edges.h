#ifndef VHSSTABILIZE_EDGES_H
#define VHSSTABILIZE_EDGES_H

#include <vector>

#include "plane.h"
#include "hshift.h"

// where and how to look for the left and right edges of the picture, where it starts and ends
// against the blanking at each side of the frame. Brightness is 0.0 (black) to 1.0.
struct edge_params_t {
    unsigned int                search[2] = {0,0};  // samples in from the left and the right of the frame to look; 0 not to look
    double                      max_shift = 8;      // the most a line is moved left or right, in samples
    double                      min_step = 0.06;    // how much brighter than the blanking the picture has to be at its edge
    double                      blank_max = 0.16;   // the brightest the blanking can be
    unsigned int                top = 0;            // lines at the top and bottom to leave out of where the edges of a frame are
    unsigned int                bottom = 0;
};

// where an edge of the picture is on each line of a frame
struct line_edges_t {
    std::vector<double>         pos;                // x of the line, to a fraction of a sample, NaN where not found
    std::vector<double>         step;               // how much brighter the picture is than the blanking there
};

// where the edge of the picture is on each line of a plane (luma), on the left (side 0) or the right
// (side 1). It is looked for up to max_shift either side of 'expect', or anywhere in the search width
// if expect is NaN.
void find_edges(line_edges_t &e,const plane_t &pl,unsigned int side,double expect,const edge_params_t &ep);

// where an edge is on most of the lines of a frame, between the top and bottom margins,
// or NaN if too few of them have it
double frame_edge(const line_edges_t &e,const edge_params_t &ep);

// where an edge should be in each frame of a video, from where it was found in each frame (NaN where
// it was not): where it is most of the time, smoothed over radius frames, or over all of the video
// if radius is 0. False if it was found in too few frames to go by.
bool edge_reference(std::vector<double> &ref,const std::vector<double> &found,double radius);

// how far the picture moved left or right on each line of a frame, from where its edges are on each
// line (from find_edges, or empty for a side not looked at) and where they should be (NaN for a side
// not looked at). Each field is worked out on its own, as its lines are one sweep of a head across
// the tape. A line where neither edge was found has moved as far as the lines above and below it.
void line_warps(std::vector<hwarp_t> &warp,const line_edges_t edges[2],const double ref[2],unsigned int height,unsigned int fields,const edge_params_t &ep);

#endif //VHSSTABILIZE_EDGES_H
