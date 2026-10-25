#ifndef MCM_CAPTURE_H
#define MCM_CAPTURE_H

#include <stdint.h>

#include <string>
#include <vector>

#include "match.h"

// the bands of the spectrum of the audio, of each slice of it as long as a frame of the video
static const int AUDIO_BANDS = 32;

// how many slices of the audio, each a frame long, a thumbnail of the audio of a frame is: THUMB_H of them,
// THUMB_W bands each (the bands are read THUMB_W at a time, so AUDIO_BANDS must be THUMB_W)
static_assert(AUDIO_BANDS == THUMB_W,"a row of a thumbnail of the audio is the bands of a slice of it");

// a capture of a tape, and what is known of each frame of it from reading it once
struct capture_t {
    std::string                 path;
    unsigned int                number = 1;         // the first file given is 1

    int                         width = 0,height = 0;
    int                         sar_num = 1,sar_den = 1;    // the shape of a pixel
    int                         rate_num = 0,rate_den = 1;  // frames a second
    int                         audio_rate = 0;     // samples a second, 0 if it has no audio
    int                         audio_channels = 0;

    thumbs_t                    thumbs;             // of each frame, and its time from the start of the file
    std::vector<uint16_t>       damage;             // of each frame, how many of its cells look like dropouts
    std::vector<float>          noise;              // of each frame, how noisy it is, in levels
    unsigned int                cells = 0;          // cells of each frame: lines times parts across

    // the spectrum of the audio, AUDIO_BANDS of each slice, slice k at k frames from the start of the file,
    // or empty if it has no audio
    std::vector<uint8_t>        audio;

    size_t frames(void) const {
        return thumbs.count();
    }
    double frame_rate(void) const {
        return rate_den > 0 ? (double)rate_num / (double)rate_den : 0;
    }
    // where frame i is in the file, in frames from the start, as an editor counts them
    int64_t position(size_t i) const {
        return (int64_t)llround(thumbs.time[i] * frame_rate());
    }
};

// Read the whole of each capture once, all at once, jobs at a time, for its thumbnails, how damaged and noisy
// each frame is, and the spectrum of its audio. What is read is kept in cache_dir, if it is not "", so that
// each file is read only once. False if any cannot be read, or CTRL+C stopped it.
bool index_captures(std::vector<capture_t> &caps,const std::string &cache_dir,unsigned int jobs,bool quiet);

// a thumbnail of the audio of frame i of a capture: the spectrum of THUMB_H frames of the audio
// around it, a frame of it in each row
void audio_thumb(uint8_t *t,const capture_t &c,size_t i);

// the thumbnails of the audio of each frame of a capture
void audio_thumbs(thumbs_t &out,const capture_t &c);

#endif //MCM_CAPTURE_H
