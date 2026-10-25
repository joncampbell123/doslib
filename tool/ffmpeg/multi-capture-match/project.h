#ifndef MCM_PROJECT_H
#define MCM_PROJECT_H

#include <string>
#include <utility>
#include <vector>

#include "capture.h"
#include "timeline.h"

// a track of the sequence, and its clips
struct track_t {
    std::string                 name;
    std::vector<clip_t>         clips;
};

// what is written: the captures, and a sequence of tracks of clips of them
struct project_t {
    std::string                 name;               // of the sequence
    const std::vector<capture_t>* caps = NULL;
    int                         width = 0,height = 0;   // of the sequence, square pixels
    int                         rate_num = 60000,rate_den = 1001;
    int64_t                     frames = 0;         // how long the sequence is
    std::vector<track_t>        video;              // bottom to top: the top one is what is seen
    std::vector<track_t>        audio;              // clips of the audio of the captures, stereo
    bool                        nest = false;       // each capture in a sequence of its own, and the clips of those

    // where the editor finds the files: each path with the first prefix that is the start of it changed
    // to what goes with it, as /media/x to D: for an editor on Windows that has the disk as D:
    std::vector<std::pair<std::string,std::string> > path_map;
};

// a Premiere Pro project (.prproj): XML, gzipped
bool write_prproj(const std::string &path,const project_t &p);

// a Final Cut Pro 7 XML file (xmeml), which Premiere Pro, DaVinci Resolve and others import
bool write_fcpxml(const std::string &path,const project_t &p);

#endif //MCM_PROJECT_H
