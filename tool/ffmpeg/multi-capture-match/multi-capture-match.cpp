/* multi-capture-match: line up several captures of the same video tape, frame by frame, take each frame
 * from the capture that has it best, and write the result as a sequence of clips of the captures, for
 * an editor: a Premiere Pro project, or Final Cut Pro 7 XML.
 *
 * Each capture is read once, for a thumbnail of each frame, how many of its lines look like dropouts
 * against the frames around it, how noisy it is, and the spectrum of its sound. That is kept in a cache,
 * so that each file is read only once.
 *
 * The frames are matched as vhs-dropout-filter matches the frames of a -donor: by the pattern of how the
 * picture changes from frame to frame, and then by the thumbnails themselves, frame by frame. -match audio
 * matches thumbnails of the spectrum of the sound in the same way, for parts with no picture to go by.
 *
 * The first capture, or the longest, is the timeline, and each of the others is matched to it in turn.
 * What a capture has that the timeline does not, as where another capture stopped or skipped, is put in
 * the timeline where it goes. Frames a capture dropped are not in it; frames too damaged to match are put
 * where the frames either side of them say they go.
 *
 * Each frame of the timeline is then taken from the capture with the least damage and noise, going to
 * another capture only where that is worth the cut. A frame that a capture card repeated for one it dropped
 * is not taken where another capture has the frame, and is cut out where none does. The frames are written
 * as clips: a clip for each run of frames of a capture, one after the other. The top track of the sequence
 * is that, and under it each capture lined up with it, all of it, for going to another by hand. */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <algorithm>
#include <string>
#include <vector>

#include "capture.h"
#include "timeline.h"
#include "project.h"

volatile sig_atomic_t interrupted = 0;

static void on_interrupt(int sig) {
    (void)sig;
    interrupted = 1;
}

// CTRL+C stops reading the files. It is only caught once: CTRL+C again quits at once.
static void catch_interrupt(void) {
    struct sigaction sa;

    memset(&sa,0,sizeof(sa));
    sa.sa_handler = on_interrupt;
    sa.sa_flags = SA_RESETHAND;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,&sa,NULL);
}

struct options_t {
    std::vector<std::string>    inputs;
    std::string                 output;
    std::string                 name;               // of the sequence
    std::string                 log;
    std::string                 cache_dir;
    int                         ref = -1;           // the capture the timeline starts from, -1 the longest
    int                         width = 0,height = 0;
    bool                        all_tracks = true;  // each capture lined up on a track of its own as well
    bool                        all_audio = false;  // and its audio
    bool                        overwrite = false;
    bool                        quiet = false;
    unsigned int                jobs = 4;
    timeline_params_t           tp;
    pick_params_t               pp;
    std::vector<std::pair<std::string,std::string> > path_map;
};

static void help(void) {
    fprintf(stderr,"multi-capture-match [options] <capture> <capture> [...] -o <output>\n");
    fprintf(stderr,"Line up captures of the same video tape, frame by frame, take each frame from the capture that\n");
    fprintf(stderr,"has it best, and write that as a sequence of clips of the captures for an editor.\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"  -o <file>      What to write: a Premiere Pro project (.prproj), or Final Cut Pro 7 XML (.xml),\n");
    fprintf(stderr,"                 which Premiere Pro, DaVinci Resolve and others import\n");
    fprintf(stderr,"  -y             Write over the output if it is there\n");
    fprintf(stderr,"  -match <by>    How to match the captures: video (the pictures, the default), audio (the\n");
    fprintf(stderr,"                 sound), or both (the pictures, and the sound where they do not match)\n");
    fprintf(stderr,"  -ref <n>       The capture the timeline starts from, 1 for the first given (default the longest)\n");
    fprintf(stderr,"  -score <n>     How alike, 0 to 1, two frames have to be to be the same frame (default 0.9)\n");
    fprintf(stderr,"  -switch <n>    What going to another capture costs, against the damage and noise it saves:\n");
    fprintf(stderr,"                 more for fewer cuts (default 3; a line of a frame that is a dropout is 0.2 a\n");
    fprintf(stderr,"                 sixteenth of it, a level of noise 0.25 a frame)\n");
    fprintf(stderr,"  -cut <percent> Cut out frames with more than this much of them dropouts in every capture\n");
    fprintf(stderr,"                 (default: never)\n");
    fprintf(stderr,"  -insert <n>    Put in the timeline a run of a capture this many frames longer than what the\n");
    fprintf(stderr,"                 timeline has there (default 8)\n");
    fprintf(stderr,"  -keep-blank    Keep frames that are blank in every capture (no tape playing)\n");
    fprintf(stderr,"  -keep-repeats  Keep frames a capture card repeated for frames it dropped, where no capture\n");
    fprintf(stderr,"                 has the frame it dropped (they are cut out, as the frames they are not)\n");
    fprintf(stderr,"  -tracks <t>    best: only the best of the captures; all: and each capture lined up with it,\n");
    fprintf(stderr,"                 all of it, on the tracks under it (the default)\n");
    fprintf(stderr,"  -audio <t>     best: only the audio of the best (the default); all: and of each capture\n");
    fprintf(stderr,"  -name <name>   The name of the sequence (default the name of the output)\n");
    fprintf(stderr,"  -size <WxH>    The size of the sequence, square pixels (default as the capture the timeline\n");
    fprintf(stderr,"                 starts from is shown)\n");
    fprintf(stderr,"  -path-map <from>=<to>  Where the editor finds the files: each path starting with <from> has\n");
    fprintf(stderr,"                 it changed to <to>, as /media/disk=D: for an editor on Windows. More than one\n");
    fprintf(stderr,"                 can be given; the first that fits is used.\n");
    fprintf(stderr,"  -log <file>    Write a CSV file of each frame of the timeline, and the frame of each capture\n");
    fprintf(stderr,"                 that is it, and which was taken\n");
    fprintf(stderr,"  -jobs <n>      How many files to read at once (default 4)\n");
    fprintf(stderr,"  -cache <dir>   Where to keep what is read of each file (default ~/.cache/multi-capture-match)\n");
    fprintf(stderr,"  -nocache       Do not keep it\n");
    fprintf(stderr,"  -q             Quiet\n");
}

static bool file_exists(const std::string &a) {
    struct stat s;
    return stat(a.c_str(),&s) == 0;
}

static bool ends_with(const std::string &s,const char *e) {
    const size_t n = strlen(e);
    if (s.size() < n)
        return false;
    return strcasecmp(s.c_str() + s.size() - n,e) == 0;
}

static bool parse_double(const char *s,double &v) {
    char *e;
    v = strtod(s,&e);
    return e != s && *e == 0;
}

static bool write_log(const std::string &path,const std::vector<slot_t> &tl,const std::vector<int> &choice,const std::vector<capture_t> &caps) {
    FILE *fp = fopen(path.c_str(),"w");
    if (fp == NULL) {
        fprintf(stderr,"%s: cannot write it\n",path.c_str());
        return false;
    }
    fprintf(fp,"slot,sequence_frame,capture,capture_frame,damage,noise");
    for (const capture_t &c : caps)
        fprintf(fp,",capture%u_frame,capture%u_match",c.number,c.number);
    fprintf(fp,"\n");

    int64_t pos = 0;
    for (size_t s=0;s < tl.size();s++) {
        if (choice[s] >= 0) {
            const cand_t &k = tl[s].c[(size_t)choice[s]];
            const capture_t &c = caps[k.cap];
            fprintf(fp,"%zu,%lld,%u,%lld,%u,%.2f",s,(long long)pos++,c.number,(long long)k.frame,c.damage[(size_t)k.frame],c.noise[(size_t)k.frame]);
        }
        else {
            fprintf(fp,"%zu,,,,,",s);
        }
        for (unsigned int c=0;c < caps.size();c++) {
            int best = -1;
            for (size_t i=0;i < tl[s].c.size();i++) {
                if (tl[s].c[i].cap == c && (best < 0 || tl[s].c[i].score > tl[s].c[(size_t)best].score))
                    best = (int)i;
            }
            if (best >= 0) {
                const cand_t &k = tl[s].c[(size_t)best];
                fprintf(fp,",%lld,%s",(long long)k.frame,k.matched ? std::to_string(k.score).c_str() : "between");
            }
            else {
                fprintf(fp,",,");
            }
        }
        fprintf(fp,"\n");
    }
    return fclose(fp) == 0;
}

static bool run(options_t &opt) {
    std::vector<capture_t> caps(opt.inputs.size());
    for (size_t i=0;i < caps.size();i++) {
        caps[i].path = opt.inputs[i];
        caps[i].number = (unsigned int)i + 1u;
    }

    catch_interrupt();
    if (!index_captures(caps,opt.cache_dir,opt.jobs,opt.quiet))
        return false;
    signal(SIGINT,SIG_DFL);

    // the timeline starts from the longest, unless told otherwise
    unsigned int ref = 0;
    if (opt.ref >= 0) {
        ref = (unsigned int)opt.ref;
    }
    else {
        for (unsigned int i=1;i < caps.size();i++) {
            if (caps[i].frames() > caps[ref].frames())
                ref = i;
        }
    }

    // they can only be matched frame for frame at the same frame rate
    std::vector<capture_t> use;
    const unsigned int ref_num = caps[ref].rate_num,ref_den = caps[ref].rate_den;
    const std::string ref_path = caps[ref].path;
    for (unsigned int i=0;i < caps.size();i++) {
        const capture_t &c = caps[i];
        if (!opt.quiet)
            fprintf(stderr,"%u: %s: %dx%d, %.3f frames a second, %zu frames%s%s\n",c.number,c.path.c_str(),c.width,c.height,c.frame_rate(),
                c.frames(),c.audio_rate > 0 ? "" : ", no audio",i == ref ? " (the timeline starts from it)" : "");
        if ((int64_t)c.rate_num * ref_den != (int64_t)ref_num * c.rate_den) {
            fprintf(stderr,"%s is not at the frame rate of %s, and is left out\n",c.path.c_str(),ref_path.c_str());
            continue;
        }
        if (i == ref)
            ref = (unsigned int)use.size();
        use.push_back(std::move(caps[i]));
    }
    caps.swap(use);

    std::vector<slot_t> tl;
    if (!build_timeline(tl,caps,ref,opt.tp))
        return false;

    std::vector<int> choice;
    pick_best(choice,tl,caps,opt.pp);

    project_t p;
    p.caps = &caps;
    p.name = opt.name;
    p.rate_num = caps[ref].rate_num;
    p.rate_den = caps[ref].rate_den;
    p.path_map = opt.path_map;
    if (opt.width > 0) {
        p.width = opt.width;
        p.height = opt.height;
    }
    else {
        p.width = ((int)lround((double)caps[ref].width * caps[ref].sar_num / caps[ref].sar_den) + 1) & ~1;
        p.height = caps[ref].height;
    }

    track_t best;
    best.name = "best";
    best_clips(best.clips,tl,choice,caps);
    p.frames = best.clips.empty() ? 0 : best.clips.back().start + best.clips.back().length;

    if (opt.all_tracks) {
        for (unsigned int c=0;c < caps.size();c++) {
            track_t t;
            t.name = caps[c].path;
            capture_clips(t.clips,c,tl,choice,caps);
            if (!t.clips.empty())
                p.video.push_back(t);
        }
    }
    p.video.push_back(best);
    p.audio.push_back(best);
    if (opt.all_audio) {
        for (unsigned int c=0;c < caps.size();c++) {
            track_t t;
            capture_clips(t.clips,c,tl,choice,caps);
            if (!t.clips.empty() && caps[c].audio_rate > 0)
                p.audio.push_back(t);
        }
    }

    // what was taken from each
    if (!opt.quiet) {
        std::vector<int64_t> frames(caps.size(),0);
        std::vector<size_t> clips(caps.size(),0);
        for (const clip_t &c : best.clips) {
            frames[c.cap] += c.length;
            clips[c.cap]++;
        }
        size_t cut = 0;
        for (int c : choice)
            cut += c < 0 ? 1u : 0u;
        fprintf(stderr,"The timeline is %zu frames; the sequence is %lld frames (%.1f seconds) in %zu clips",tl.size(),
            (long long)p.frames,(double)p.frames * p.rate_den / p.rate_num,best.clips.size());
        if (cut != 0)
            fprintf(stderr,", with %zu frames cut out",cut);
        fprintf(stderr,"\n");
        for (unsigned int c=0;c < caps.size();c++)
            fprintf(stderr,"  %u: %lld frames (%.1f%%) in %zu clips from %s\n",caps[c].number,(long long)frames[c],
                p.frames ? (100.0 * (double)frames[c]) / (double)p.frames : 0.0,clips[c],caps[c].path.c_str());
    }

    if (!opt.log.empty() && !write_log(opt.log,tl,choice,caps))
        return false;

    const bool xml = ends_with(opt.output,".xml");
    if (!(xml ? write_fcpxml(opt.output,p) : write_prproj(opt.output,p)))
        return false;
    if (!opt.quiet)
        fprintf(stderr,"Wrote %s\n",opt.output.c_str());
    return true;
}

int main(int argc,char **argv) {
    options_t opt;
    bool cache_given = false;

    for (int i=1;i < argc;i++) {
        const char *a = argv[i];

        if (*a != '-' || a[1] == 0) {
            opt.inputs.push_back(a);
            continue;
        }

        const std::string o = a + 1;
        const bool takes = (o == "o" || o == "match" || o == "ref" || o == "score" || o == "switch" || o == "cut" || o == "insert" ||
                            o == "tracks" || o == "audio" || o == "name" || o == "size" || o == "path-map" || o == "log" ||
                            o == "jobs" || o == "cache");
        const char *v = NULL;
        if (takes) {
            if (i + 1 >= argc) {
                fprintf(stderr,"-%s needs a value\n",o.c_str());
                return 1;
            }
            v = argv[++i];
        }
        double d;

        if (o == "h" || o == "help" || o == "-help") {
            help();
            return 0;
        }
        else if (o == "o") {
            opt.output = v;
        }
        else if (o == "y") {
            opt.overwrite = true;
        }
        else if (o == "q") {
            opt.quiet = true;
        }
        else if (o == "keep-blank") {
            opt.tp.keep_blank = true;
        }
        else if (o == "keep-repeats") {
            opt.pp.keep_repeats = true;
        }
        else if (o == "nocache") {
            opt.cache_dir.clear();
            cache_given = true;
        }
        else if (o == "match") {
            if (!strcmp(v,"video")) opt.tp.by = MATCH_VIDEO;
            else if (!strcmp(v,"audio")) opt.tp.by = MATCH_AUDIO;
            else if (!strcmp(v,"both")) opt.tp.by = MATCH_BOTH;
            else {
                fprintf(stderr,"-match %s: must be video, audio or both\n",v);
                return 1;
            }
        }
        else if (o == "ref") {
            if (!parse_double(v,d) || d < 1 || d != floor(d)) {
                fprintf(stderr,"-ref %s: must be the number of a capture, 1 for the first\n",v);
                return 1;
            }
            opt.ref = (int)d - 1;
        }
        else if (o == "score") {
            if (!parse_double(v,d) || d <= 0 || d >= 1) {
                fprintf(stderr,"-score %s: must be between 0 and 1\n",v);
                return 1;
            }
            opt.tp.min_score = d;
        }
        else if (o == "switch") {
            if (!parse_double(v,d) || d < 0) {
                fprintf(stderr,"-switch %s: must be 0 or more\n",v);
                return 1;
            }
            opt.pp.switch_cost = d;
        }
        else if (o == "cut") {
            if (!parse_double(v,d) || d < 0 || d > 100) {
                fprintf(stderr,"-cut %s: must be a percentage\n",v);
                return 1;
            }
            opt.pp.cut = d / 100.0;
        }
        else if (o == "insert") {
            if (!parse_double(v,d) || d < 1 || d != floor(d)) {
                fprintf(stderr,"-insert %s: must be 1 or more frames\n",v);
                return 1;
            }
            opt.tp.insert_min = (int)d;
        }
        else if (o == "tracks" || o == "audio") {
            bool all;
            if (!strcmp(v,"best")) all = false;
            else if (!strcmp(v,"all")) all = true;
            else {
                fprintf(stderr,"-%s %s: must be best or all\n",o.c_str(),v);
                return 1;
            }
            (o == "tracks" ? opt.all_tracks : opt.all_audio) = all;
        }
        else if (o == "name") {
            opt.name = v;
        }
        else if (o == "size") {
            if (sscanf(v,"%dx%d",&opt.width,&opt.height) != 2 || opt.width < 16 || opt.height < 16) {
                fprintf(stderr,"-size %s: must be WIDTHxHEIGHT\n",v);
                return 1;
            }
        }
        else if (o == "path-map") {
            const char *eq = strchr(v,'=');
            if (eq == NULL || eq == v) {
                fprintf(stderr,"-path-map %s: must be <from>=<to>\n",v);
                return 1;
            }
            opt.path_map.push_back(std::make_pair(std::string(v,(size_t)(eq - v)),std::string(eq + 1)));
        }
        else if (o == "log") {
            opt.log = v;
        }
        else if (o == "jobs") {
            if (!parse_double(v,d) || d < 1 || d != floor(d)) {
                fprintf(stderr,"-jobs %s: must be 1 or more\n",v);
                return 1;
            }
            opt.jobs = (unsigned int)d;
        }
        else if (o == "cache") {
            opt.cache_dir = v;
            cache_given = true;
        }
        else {
            fprintf(stderr,"Unknown option %s\n",a);
            return 1;
        }
    }

    if (opt.inputs.empty() || opt.output.empty()) {
        help();
        return 1;
    }
    if (opt.ref >= (int)opt.inputs.size()) {
        fprintf(stderr,"-ref %d: there are only %zu captures\n",opt.ref + 1,opt.inputs.size());
        return 1;
    }
    if (!ends_with(opt.output,".prproj") && !ends_with(opt.output,".xml")) {
        fprintf(stderr,"%s: the output must be a .prproj or .xml file\n",opt.output.c_str());
        return 1;
    }
    if (!opt.overwrite && file_exists(opt.output)) {
        fprintf(stderr,"%s is there already (-y to write over it)\n",opt.output.c_str());
        return 1;
    }
    for (const std::string &in : opt.inputs) {
        if (in == opt.output) {
            fprintf(stderr,"%s: the output would write over a capture\n",in.c_str());
            return 1;
        }
    }
    if (opt.name.empty()) {
        const size_t s = opt.output.rfind('/');
        opt.name = s == std::string::npos ? opt.output : opt.output.substr(s + 1u);
        const size_t dot = opt.name.rfind('.');
        if (dot != std::string::npos && dot > 0)
            opt.name.resize(dot);
    }
    if (!cache_given) {
        const char *x = getenv("XDG_CACHE_HOME"),*home = getenv("HOME");
        if (x != NULL && *x)
            opt.cache_dir = std::string(x) + "/multi-capture-match";
        else if (home != NULL && *home)
            opt.cache_dir = std::string(home) + "/.cache/multi-capture-match";
    }
    opt.tp.quiet = opt.quiet;

    return run(opt) ? 0 : 1;
}
