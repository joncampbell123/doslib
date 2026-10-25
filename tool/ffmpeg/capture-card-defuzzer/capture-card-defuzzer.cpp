/* capture-card-defuzzer: undo what an old video capture card did to the picture, to make it less fuzzy.
 *
 * -filter avg2y: a card that captured 320x240 from one field of the video, and made each line the mean of
 *                it and the line above it: out[y] = (in[y-1] + in[y]) / 2. Each line is solved for again,
 *                without ringing (see avg2y.h).
 *
 * The cards' video was most often MPEG, with blocks that undoing them would make sharper too, so first, by
 * default for MPEG, FFmpeg's spp post-processing filter takes the blocks out, as much as the quantizer of
 * each block in the stream says they need.
 *
 * The video is read once, each frame filtered and encoded again. The audio is copied as it is. */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/avutil.h>
#include <libavutil/avconfig.h>
#include <libavutil/opt.h>
#include <libavutil/parseutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include <omp.h>

#include "avg2y.h"

// the corrections -filter can make
enum correction_t {
    CORR_AVG2Y=0                                    // each line the mean of it and the line above
};

struct correction_info_t {
    const char*                 name;
    const char*                 what;
};

static const correction_info_t corrections[] = {
    { "avg2y",  "each line of a field the mean of it and the line above, (in[y-1] + in[y]) / 2,\n"
                "                   as a card capturing 320x240 from one field made them" }
};

struct options_t {
    std::string                 input;
    std::string                 output;
    std::string                 codec;              // video encoder name
    std::string                 codec_opts;         // key=value:key=value
    std::string                 deblock = "auto";   // the FFmpeg filter graph that takes out MPEG's blocks first, "auto" or "none"
    std::vector<int>            filters;            // the corrections, in order
    int64_t                     bit_rate = 0;
    int64_t                     start_us = 0;       // -ss: how far into the input to start, in microseconds
    int64_t                     length_us = -1;     // -t: how much of it to do, -1 for all of it
    int                         threads = 0;        // to work on the pictures with, 0 for half the CPUs
    int                         fields = -1;        // -1 to decide from the file, 0 progressive, 1 interlaced
    bool                        chroma = false;     // correct the color too, not only the brightness
    avg2y_params_t              avg2y;
    bool                        overwrite = false;
    bool                        quiet = false;
};

static void help(void) {
    fprintf(stderr,"capture-card-defuzzer [options] <input> <output>\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"Undoes what an old video capture card did to the picture. The video is encoded again,\n");
    fprintf(stderr,"the audio is copied as it is.\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"  -filter <name>[,<name>...]  The corrections to make, in order (default avg2y):\n");
    for (const correction_info_t &c : corrections)
        fprintf(stderr,"    %-14s %s\n",c.name,c.what);
    fprintf(stderr,"  -smooth <n>    avg2y: what each level of change from line to line costs, against each\n");
    fprintf(stderr,"                 level the picture differs from what the card made. More boosts the finest\n");
    fprintf(stderr,"                 detail less, and the noise with it; less boosts both more (default 3)\n");
    fprintf(stderr,"  -iterations <n>  avg2y: turns of the solver, more for nearer the best picture (default 80)\n");
    fprintf(stderr,"  -chroma        Correct the color as well as the brightness. Not for MPEG of 4:2:0 color, which\n");
    fprintf(stderr,"                 has half the lines of color, each made the mean of two lines again.\n");
    fprintf(stderr,"  -deblock <f>   Take out MPEG's blocks first, before the corrections make them sharper, with\n");
    fprintf(stderr,"                 this FFmpeg filter graph, such as spp, spp=5, pp7, fspp or deblock (the\n");
    fprintf(stderr,"                 post-processing filters use the quantizer of each block in the stream);\n");
    fprintf(stderr,"                 auto (the default) for spp if the video is MPEG-1, MPEG-2, MPEG-4 part 2,\n");
    fprintf(stderr,"                 H.263 or MJPEG, and none otherwise; none not to\n");
    fprintf(stderr,"  -I             Interlaced: each field is corrected by itself\n");
    fprintf(stderr,"                 (default, if the file says the video is interlaced)\n");
    fprintf(stderr,"  -P             Progressive: each frame is corrected as a whole, as the card's\n");
    fprintf(stderr,"                 one field 320x240 frames are\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"Output:\n");
    fprintf(stderr,"  -c <encoder>   Video encoder, such as ffv1, prores_ks or libx264 (default: ffv1 for .mkv,\n");
    fprintf(stderr,"                 .avi and .nut, prores_ks for .mov, libx264 -crf 14 for .mp4, otherwise the\n");
    fprintf(stderr,"                 input's codec)\n");
    fprintf(stderr,"  -b <rate>      Video bit rate, such as 50M\n");
    fprintf(stderr,"  -x <k=v:k=v>   Video encoder options, such as -x crf=18\n");
    fprintf(stderr,"  -ss <time>     Start this far into the input, as FFmpeg's -ss: seconds, such as 90.5,\n");
    fprintf(stderr,"                 or [HH:]MM:SS[.m...], such as 1:30.5. The output starts at 0.\n");
    fprintf(stderr,"  -t <time>      Do only this much of the input, as FFmpeg's -t\n");
    fprintf(stderr,"  -y             Write over the output file if it exists\n");
    fprintf(stderr,"  -threads <n>   Threads to work on the pictures with (default: half the CPUs, leaving\n");
    fprintf(stderr,"                 the rest to decode and encode the video)\n");
    fprintf(stderr,"  -q             Quiet\n");
    fprintf(stderr,"  -h             This help\n");
}

static std::string averr(int e) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(e,buf,sizeof(buf));
    return buf;
}

// true if the two names are, or would be, the same file
static bool same_file(const std::string &a,const std::string &b) {
    struct stat sa,sb;

    if (a == b)
        return true;
    if (stat(a.c_str(),&sa) != 0 || stat(b.c_str(),&sb) != 0)
        return false;

    return sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
}

static bool file_exists(const std::string &a) {
    struct stat s;
    return stat(a.c_str(),&s) == 0;
}

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC,&ts);
    return (double)ts.tv_sec + ((double)ts.tv_nsec / 1e9);
}

/////////////////////////////////////////////////////////////////////////////
// pixel formats

// true if each component is in its own plane, 8 bits in a byte or 9 to 16 bits in two bytes of host byte order.
// Those are the formats measured and moved: anything else is converted to one of them.
static bool pixfmt_workable(enum AVPixelFormat fmt) {
    const AVPixFmtDescriptor *d = av_pix_fmt_desc_get(fmt);

    if (d == NULL || d->nb_components == 0)
        return false;
    if (d->flags & (AV_PIX_FMT_FLAG_HWACCEL | AV_PIX_FMT_FLAG_PAL | AV_PIX_FMT_FLAG_BITSTREAM | AV_PIX_FMT_FLAG_FLOAT))
        return false;

    for (unsigned int i=0;i < d->nb_components;i++) {
        const AVComponentDescriptor &c = d->comp[i];
        const int bytes = c.depth > 8 ? 2 : 1;

        for (unsigned int j=0;j < i;j++) {
            if (d->comp[j].plane == c.plane)
                return false;
        }
        if (c.depth > 16 || c.shift != 0 || c.offset != 0 || c.step != bytes)
            return false;
        if (bytes == 2 && (!!(d->flags & AV_PIX_FMT_FLAG_BE)) != (!!AV_HAVE_BIGENDIAN))
            return false;
    }

    return true;
}

// the old full range YUV formats
static bool pixfmt_is_yuvj(enum AVPixelFormat fmt) {
    const char *n = av_get_pix_fmt_name(fmt);
    return n != NULL && strncmp(n,"yuvj",4) == 0;
}

static bool pixfmt_has_alpha(enum AVPixelFormat fmt) {
    const AVPixFmtDescriptor *d = av_pix_fmt_desc_get(fmt);
    return d != NULL && (d->flags & AV_PIX_FMT_FLAG_ALPHA);
}

// the workable format to convert a format that is not workable to, losing the least
static enum AVPixelFormat work_format_for(enum AVPixelFormat fmt) {
    static const enum AVPixelFormat candidates[] = {
        AV_PIX_FMT_YUV420P,     AV_PIX_FMT_YUV422P,     AV_PIX_FMT_YUV444P,     AV_PIX_FMT_YUV411P,     AV_PIX_FMT_YUV440P,
        AV_PIX_FMT_YUV420P10,   AV_PIX_FMT_YUV422P10,   AV_PIX_FMT_YUV444P10,
        AV_PIX_FMT_YUV420P12,   AV_PIX_FMT_YUV422P12,   AV_PIX_FMT_YUV444P12,
        AV_PIX_FMT_YUV420P16,   AV_PIX_FMT_YUV422P16,   AV_PIX_FMT_YUV444P16,
        AV_PIX_FMT_YUVA420P,    AV_PIX_FMT_YUVA422P,    AV_PIX_FMT_YUVA444P,
        AV_PIX_FMT_YUVA422P10,  AV_PIX_FMT_YUVA444P10,  AV_PIX_FMT_YUVA444P16,
        AV_PIX_FMT_GRAY8,       AV_PIX_FMT_GRAY10,      AV_PIX_FMT_GRAY12,      AV_PIX_FMT_GRAY16,
        AV_PIX_FMT_GBRP,        AV_PIX_FMT_GBRP10,      AV_PIX_FMT_GBRP12,      AV_PIX_FMT_GBRP16,
        AV_PIX_FMT_GBRAP,       AV_PIX_FMT_GBRAP16,
        AV_PIX_FMT_NONE
    };

    if (pixfmt_workable(fmt))
        return fmt;

    return avcodec_find_best_pix_fmt_of_list(candidates,fmt,pixfmt_has_alpha(fmt) ? 1 : 0,NULL);
}

static const enum AVPixelFormat *encoder_pix_fmts(const AVCodec *c) {
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61,13,100)
    const void *list = NULL;
    int count = 0;

    if (avcodec_get_supported_config(NULL,c,AV_CODEC_CONFIG_PIX_FORMAT,0,&list,&count) < 0)
        return NULL;

    return (const enum AVPixelFormat*)list;
#else
    return c->pix_fmts;
#endif
}

static bool frame_is_interlaced(const AVFrame *f) {
#ifdef AV_FRAME_FLAG_INTERLACED
    return !!(f->flags & AV_FRAME_FLAG_INTERLACED);
#else
    return !!f->interlaced_frame;
#endif
}

static bool frame_is_tff(const AVFrame *f) {
#ifdef AV_FRAME_FLAG_TOP_FIELD_FIRST
    return !!(f->flags & AV_FRAME_FLAG_TOP_FIELD_FIRST);
#else
    return !!f->top_field_first;
#endif
}

static void frame_set_interlaced(AVFrame *f,bool tff) {
#ifdef AV_FRAME_FLAG_INTERLACED
    f->flags |= AV_FRAME_FLAG_INTERLACED;
    if (tff)
        f->flags |= AV_FRAME_FLAG_TOP_FIELD_FIRST;
    else
        f->flags &= ~AV_FRAME_FLAG_TOP_FIELD_FIRST;
#else
    f->interlaced_frame = 1;
    f->top_field_first = tff ? 1 : 0;
#endif
}

static bool field_order_interlaced(enum AVFieldOrder fo) {
    return fo == AV_FIELD_TT || fo == AV_FIELD_BB || fo == AV_FIELD_TB || fo == AV_FIELD_BT;
}

static const char *field_order_name(enum AVFieldOrder fo) {
    switch (fo) {
        case AV_FIELD_PROGRESSIVE:  return "progressive";
        case AV_FIELD_TT:           return "interlaced, top field first";
        case AV_FIELD_BB:           return "interlaced, bottom field first";
        case AV_FIELD_TB:           return "interlaced, top field coded first, bottom field shown first";
        case AV_FIELD_BT:           return "interlaced, bottom field coded first, top field shown first";
        default:                    break;
    }
    return "field order not given";
}

/////////////////////////////////////////////////////////////////////////////
// converting frames from one pixel format to another

class converter_t {
public:
    ~converter_t() {
        if (sws) sws_freeContext(sws);
    }

    // convert src into dst, which has its format and size set and its buffers allocated.
    // Levels stay as they are, full range or not, unless dst is a yuvj format, which is always full range.
    bool convert(AVFrame *dst,const AVFrame *src) {
        const enum AVPixelFormat sf = (enum AVPixelFormat)src->format;
        const enum AVPixelFormat df = (enum AVPixelFormat)dst->format;

        sws = sws_getCachedContext(sws,src->width,src->height,sf,dst->width,dst->height,df,
            SWS_BICUBIC | SWS_ACCURATE_RND | SWS_FULL_CHR_H_INT | SWS_FULL_CHR_H_INP,NULL,NULL,NULL);
        if (sws == NULL)
            return false;

        const int src_full = (src->color_range == AVCOL_RANGE_JPEG || pixfmt_is_yuvj(sf)) ? 1 : 0;
        const int dst_full = (src_full || pixfmt_is_yuvj(df)) ? 1 : 0;
        const int *coefs = sws_getCoefficients(src->colorspace == AVCOL_SPC_UNSPECIFIED ? SWS_CS_DEFAULT : (int)src->colorspace);
        sws_setColorspaceDetails(sws,coefs,src_full,coefs,dst_full,0,1 << 16,1 << 16);

        return sws_scale(sws,src->data,src->linesize,0,src->height,dst->data,dst->linesize) > 0;
    }
private:
    struct SwsContext*          sws = NULL;
};

// a frame of that format and size, with its buffers
static AVFrame *alloc_frame(enum AVPixelFormat fmt,int width,int height) {
    AVFrame *f = av_frame_alloc();

    if (f == NULL)
        return NULL;

    f->format = fmt;
    f->width = width;
    f->height = height;
    if (av_frame_get_buffer(f,0) < 0) {
        av_frame_free(&f);
        return NULL;
    }

    return f;
}
/////////////////////////////////////////////////////////////////////////////
// reading and decoding the input file

// set by CTRL+C: stop reading the input, and finish the output file with what there is
static volatile sig_atomic_t interrupted = 0;

static void on_interrupt(int sig) {
    (void)sig;
    interrupted = 1;
}

// CTRL+C stops the reading, so that what is written so far is finished and playable.
// It is only caught once: CTRL+C again quits at once.
static void catch_interrupt(void) {
    struct sigaction sa;

    memset(&sa,0,sizeof(sa));
    sa.sa_handler = on_interrupt;
    sa.sa_flags = SA_RESETHAND;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,&sa,NULL);
}

// a time as FFmpeg takes it for -ss and -t: seconds, such as 90 or 1.5, or [HH:]MM:SS[.m...],
// in microseconds, or false if it is not one or is less than 0
static bool parse_time(const char *s,int64_t &us) {
    return av_parse_time(&us,s,1) >= 0 && us >= 0;
}

class input_t {
public:
    AVFormatContext*            fmt = NULL;
    AVCodecContext*             dec = NULL;
    int                         vindex = -1;
    int64_t                     start_us = 0;       // how far into the input to start, as FFmpeg's -ss
    int64_t                     length_us = -1;     // how much of it, as FFmpeg's -t, -1 for the rest of it
    const char*                 stop_message = NULL;    // said when CTRL+C stops the reading
    bool                        export_qp = false;  // the decoder gives the quantizer of each block with each frame,
                                                    // for the post-processing filters that use it
    bool                        stopped = false;    // CTRL+C stopped the reading before the end

    ~input_t() {
        close();
    }

    void close(void) {
        avcodec_free_context(&dec);
        avformat_close_input(&fmt);
        vindex = -1;
    }

    AVStream *vstream(void) const {
        return fmt->streams[vindex];
    }

    // open the file and the decoder for its video. If video_only, the other streams are not read.
    bool open(const std::string &path,bool video_only) {
        int r;

        if ((r=avformat_open_input(&fmt,path.c_str(),NULL,NULL)) < 0) {
            fprintf(stderr,"%s: %s\n",path.c_str(),averr(r).c_str());
            return false;
        }
        if ((r=avformat_find_stream_info(fmt,NULL)) < 0) {
            fprintf(stderr,"%s: cannot read the streams: %s\n",path.c_str(),averr(r).c_str());
            return false;
        }
        if ((vindex=av_find_best_stream(fmt,AVMEDIA_TYPE_VIDEO,-1,-1,NULL,0)) < 0) {
            fprintf(stderr,"%s: no video stream\n",path.c_str());
            return false;
        }

        const AVStream *st = fmt->streams[vindex];
        const AVCodec *codec = avcodec_find_decoder(st->codecpar->codec_id);
        if (codec == NULL) {
            fprintf(stderr,"%s: no decoder for %s video\n",path.c_str(),avcodec_get_name(st->codecpar->codec_id));
            return false;
        }

        if ((dec=avcodec_alloc_context3(codec)) == NULL)
            return false;
        if ((r=avcodec_parameters_to_context(dec,st->codecpar)) < 0) {
            fprintf(stderr,"%s: %s\n",path.c_str(),averr(r).c_str());
            return false;
        }
        dec->pkt_timebase = st->time_base;
        dec->thread_count = 0;
        if (export_qp)
            dec->export_side_data |= AV_CODEC_EXPORT_DATA_VIDEO_ENC_PARAMS;
        if ((r=avcodec_open2(dec,codec,NULL)) < 0) {
            fprintf(stderr,"%s: cannot open the %s decoder: %s\n",path.c_str(),codec->name,averr(r).c_str());
            return false;
        }

        if (video_only) {
            for (unsigned int i=0;i < fmt->nb_streams;i++) {
                if ((int)i != vindex)
                    fmt->streams[i]->discard = AVDISCARD_ALL;
            }
        }

        // the start of the part to read, as FFmpeg's -ss: the keyframe at or before it, from which
        // the frames before it are decoded and left out
        t0 = fmt->start_time != AV_NOPTS_VALUE ? fmt->start_time : 0;
        const AVRational fr = av_guess_frame_rate(fmt,fmt->streams[vindex],NULL);
        half_frame_us = (fr.num > 0 && fr.den > 0) ? av_rescale_q(1,av_inv_q(fr),av_make_q(1,AV_TIME_BASE)) / 2 : 0;
        if (start_us > 0) {
            const int64_t ts = t0 + start_us;
            if ((r=avformat_seek_file(fmt,-1,INT64_MIN,ts,ts,0)) < 0) {
                fprintf(stderr,"%s: cannot go to %.3f seconds in: %s\n",path.c_str(),(double)start_us / AV_TIME_BASE,averr(r).c_str());
                return false;
            }
        }

        return true;
    }

    // what to take off the time of everything written, in microseconds, so that with -ss
    // the output starts at 0, as FFmpeg's does
    int64_t offset_us(void) const {
        return start_us > 0 ? t0 + start_us : 0;
    }

    // how far a frame or packet at ts in time base tb is into the input, from its start, in microseconds
    int64_t time_us(int64_t ts,AVRational tb) const {
        return av_rescale_q(ts,tb,av_make_q(1,AV_TIME_BASE)) - t0;
    }

    // read the file, or the part of it from start_us for length_us: decode each frame of the video
    // and give it to on_frame, and give each packet of the other streams to on_packet. Either can
    // stop it by returning false, which is an error. CTRL+C stops it too, but not as an error.
    bool run(const std::function<bool(AVFrame*)> &on_frame,const std::function<bool(AVPacket*)> &on_packet) {
        AVPacket *pkt = av_packet_alloc();
        AVFrame *frame = av_frame_alloc();
        const AVRational vtb = fmt->streams[vindex]->time_base;
        const int64_t end_us = length_us >= 0 ? start_us + length_us : INT64_MAX;
        unsigned int errors = 0;
        bool ok = (pkt != NULL && frame != NULL);
        bool ended = false;                         // past the end of the part to read
        int r;

        // give on_frame each frame the decoder has ready, of the part to read: the frame nearest
        // the start, and the frames before the end
        auto drain = [&](void) -> bool {
            for (;;) {
                r = avcodec_receive_frame(dec,frame);
                if (r == AVERROR(EAGAIN) || r == AVERROR_EOF)
                    return true;
                if (r < 0) {
                    if (errors++ < 10u) fprintf(stderr,"\nVideo decoding error: %s\n",averr(r).c_str());
                    return true;
                }

                if (frame->best_effort_timestamp != AV_NOPTS_VALUE) {
                    const int64_t t = time_us(frame->best_effort_timestamp,vtb) + half_frame_us;
                    if (t < start_us) {
                        av_frame_unref(frame);
                        continue;
                    }
                    if (t >= end_us) {
                        av_frame_unref(frame);
                        ended = true;
                        return false;
                    }
                }

                const bool keep = on_frame(frame);
                av_frame_unref(frame);
                if (!keep)
                    return false;
            }
        };

        while (ok) {
            if (interrupted) {
                stopped = true;
                if (stop_message != NULL)
                    fprintf(stderr,"\n%s\n",stop_message);
                break;
            }

            if ((r=av_read_frame(fmt,pkt)) < 0) {
                if (r != AVERROR_EOF)
                    fprintf(stderr,"\nError reading the input, stopping there: %s\n",averr(r).c_str());
                break;
            }

            if (pkt->stream_index == vindex) {
                for (;;) {
                    r = avcodec_send_packet(dec,pkt);
                    if (r == AVERROR(EAGAIN)) {
                        if (!(ok=drain())) break;
                        continue;
                    }
                    if (r < 0 && errors++ < 10u)
                        fprintf(stderr,"\nVideo decoding error: %s\n",averr(r).c_str());
                    break;
                }
                if (ok)
                    ok = drain();
            }
            else if (on_packet) {
                // of the part to read
                const AVStream *s = fmt->streams[pkt->stream_index];
                const int64_t t = pkt->pts != AV_NOPTS_VALUE ? time_us(pkt->pts,s->time_base) : start_us;
                if (t >= start_us && t < end_us)
                    ok = on_packet(pkt);
            }

            av_packet_unref(pkt);
        }

        // and the frames the decoder still has, unless stopped before the end, when some of them
        // may be missing frames they need, and are left out, which would put the others out of step
        if (ended || stopped) {
            ok = ok || ended;
        }
        else if (ok) {
            avcodec_send_packet(dec,NULL);
            ok = drain();
            if (ended)
                ok = true;
        }

        av_packet_free(&pkt);
        av_frame_free(&frame);
        return ok;
    }
private:
    int64_t                     t0 = 0;             // the time the input starts at, in microseconds
    int64_t                     half_frame_us = 0;
};

class progress_t {
public:
    progress_t(const char *what,int64_t total,bool quiet) : what(what), total(total), quiet(quiet) { }

    void frame(unsigned long n) {
        const double t = now_seconds();
        if (!quiet && (t - last) >= 0.5) {
            show(n);
            last = t;
        }
    }

    void done(unsigned long n) {
        if (!quiet) {
            show(n);
            fprintf(stderr,"\n");
        }
    }
private:
    void show(unsigned long n) {
        if (total > 0)
            fprintf(stderr,"\r%s: frame %lu of %lld (%.1f%%)  ",what,n,(long long)total,(n * 100.0) / (double)total);
        else
            fprintf(stderr,"\r%s: frame %lu  ",what,n);
    }

    const char*                 what;
    int64_t                     total;
    bool                        quiet;
    double                      last = 0;
};

// how many frames the video stream has, or about how many, or 0 if not known,
// of the part of it that is read
static int64_t frame_count(const input_t &in) {
    const AVStream *st = in.vstream();
    const AVRational fr = av_guess_frame_rate(in.fmt,(AVStream*)st,NULL);
    const bool rate = fr.num > 0 && fr.den > 0;
    int64_t n = 0;

    if (st->nb_frames > 0)
        n = st->nb_frames;
    else if (in.fmt->duration > 0 && rate)
        n = av_rescale_q(in.fmt->duration,av_make_q(1,AV_TIME_BASE),av_inv_q(fr));

    if (rate && in.length_us >= 0)
        return av_rescale_q(in.length_us,av_make_q(1,AV_TIME_BASE),av_inv_q(fr));
    if (rate && in.start_us > 0 && n > 0)
        return std::max((int64_t)0,n - av_rescale_q(in.start_us,av_make_q(1,AV_TIME_BASE),av_inv_q(fr)));

    return n;
}
/////////////////////////////////////////////////////////////////////////////
// writing the output file

class output_t {
public:
    AVFormatContext*            oc = NULL;
    AVCodecContext*             enc = NULL;
    AVPacket*                   pkt = NULL;
    std::vector<int>            map;                // output stream of each input stream, or -1
    int                         vindex = -1;        // output video stream
    int64_t                     frame_duration = 0; // in the encoder time base
    int64_t                     last_pts = AV_NOPTS_VALUE;

    ~output_t() {
        if (oc != NULL) {
            if (!(oc->oformat->flags & AVFMT_NOFILE))
                avio_closep(&oc->pb);
            avformat_free_context(oc);
        }
        avcodec_free_context(&enc);
        av_packet_free(&pkt);
    }

    // write the packets the encoder has ready
    bool write_encoded(void) {
        for (;;) {
            int r = avcodec_receive_packet(enc,pkt);
            if (r == AVERROR(EAGAIN) || r == AVERROR_EOF)
                return true;
            if (r < 0) {
                fprintf(stderr,"\nVideo encoding error: %s\n",averr(r).c_str());
                return false;
            }

            if (pkt->duration <= 0)
                pkt->duration = frame_duration;
            av_packet_rescale_ts(pkt,enc->time_base,oc->streams[vindex]->time_base);
            pkt->stream_index = vindex;
            if ((r=av_interleaved_write_frame(oc,pkt)) < 0) {
                fprintf(stderr,"\nError writing video: %s\n",averr(r).c_str());
                return false;
            }
        }
    }
};

// The video encoder: -c, or else one for the kind of file the output is, as the input was most likely a
// capture card's MPEG, too little to encode again as it was: lossless FFV1 in Matroska, AVI and NUT, ProRes
// in QuickTime, H.264 in MP4. Otherwise the input's codec.
static const AVCodec *choose_encoder(const options_t &opt,enum AVCodecID id,const AVOutputFormat *ofmt) {
    const char *name = ofmt->name;
    const char *pick = NULL;
    const AVCodec *c;

    if (!opt.codec.empty())
        return avcodec_find_encoder_by_name(opt.codec.c_str());

    if (strstr(name,"matroska") != NULL || strcmp(name,"avi") == 0 || strcmp(name,"nut") == 0)
        pick = "ffv1";
    else if (strcmp(name,"mov") == 0)
        pick = "prores_ks";
    else if (strstr(name,"mp4") != NULL)
        pick = "libx264";
    if (pick != NULL && (c=avcodec_find_encoder_by_name(pick)) != NULL)
        return c;

    // prores_ks makes better ProRes than the default ProRes encoder
    if (id == AV_CODEC_ID_PRORES && (c=avcodec_find_encoder_by_name("prores_ks")) != NULL)
        return c;
    return avcodec_find_encoder(id);
}

// true for the codecs that can code interlaced video as fields, which FFmpeg's encoders for
// them do if asked to with AV_CODEC_FLAG_INTERLACED_DCT. Others, such as MJPEG, refuse to open.
static bool codec_codes_fields(enum AVCodecID id) {
    return id == AV_CODEC_ID_PRORES || id == AV_CODEC_ID_H264 || id == AV_CODEC_ID_MPEG2VIDEO ||
           id == AV_CODEC_ID_MPEG4 || id == AV_CODEC_ID_DNXHD;
}

// the stream's codec tag, if the output format can have it for that codec
static unsigned int copy_codec_tag(const AVOutputFormat *ofmt,const AVCodecParameters *par) {
    unsigned int tag = 0;

    if (ofmt->codec_tag == NULL || av_codec_get_id(ofmt->codec_tag,par->codec_tag) == par->codec_id ||
        !av_codec_get_tag2(ofmt->codec_tag,par->codec_id,&tag))
        return par->codec_tag;

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
// pictures

// what the video is like
struct video_info_t {
    int                         width = 0;
    int                         height = 0;
    enum AVPixelFormat          dec_fmt = AV_PIX_FMT_NONE;     // as decoded
    enum AVPixelFormat          work_fmt = AV_PIX_FMT_NONE;    // as worked on
    AVRational                  sar = {0,1};
    AVRational                  frame_rate = {0,1};
    enum AVColorRange           color_range = AVCOL_RANGE_UNSPECIFIED;
    enum AVColorPrimaries       color_primaries = AVCOL_PRI_UNSPECIFIED;
    enum AVColorTransferCharacteristic color_trc = AVCOL_TRC_UNSPECIFIED;
    enum AVColorSpace           colorspace = AVCOL_SPC_UNSPECIFIED;
    enum AVChromaLocation       chroma_location = AVCHROMA_LOC_UNSPECIFIED;
    enum AVFieldOrder           field_order = AV_FIELD_UNKNOWN;
    bool                        fields = true;      // each field is a picture of its own
    bool                        tff = true;         // the top field is shown first
    bool                        rgb = false;
};

// a color component of a frame of a workable format
struct comp_t {
    int                         plane = 0;
    unsigned int                hsub = 0;
    unsigned int                vsub = 0;
    unsigned int                depth = 8;
};

// the components of a workable format that are not alpha, in order: Y, U, V, or R, G, B, or Y
static std::vector<comp_t> color_comps(enum AVPixelFormat fmt) {
    const AVPixFmtDescriptor *d = av_pix_fmt_desc_get(fmt);
    const bool rgb = !!(d->flags & AV_PIX_FMT_FLAG_RGB);
    const unsigned int n = d->nb_components - ((d->flags & AV_PIX_FMT_FLAG_ALPHA) ? 1u : 0u);
    std::vector<comp_t> r;

    for (unsigned int i=0;i < n;i++) {
        const bool chroma = !rgb && (i == 1u || i == 2u);
        comp_t c;

        c.plane = d->comp[i].plane;
        c.hsub = chroma ? d->log2_chroma_w : 0u;
        c.vsub = chroma ? d->log2_chroma_h : 0u;
        c.depth = (unsigned int)d->comp[i].depth;
        r.push_back(c);
    }

    return r;
}


/////////////////////////////////////////////////////////////////////////////
// taking out the blocks: an FFmpeg filter graph

// the codecs of blocks of 8x8 DCT that FFmpeg's post-processing filters are for
static bool codec_has_blocks(enum AVCodecID id) {
    return id == AV_CODEC_ID_MPEG1VIDEO || id == AV_CODEC_ID_MPEG2VIDEO || id == AV_CODEC_ID_MPEG4 ||
           id == AV_CODEC_ID_H263 || id == AV_CODEC_ID_H263P || id == AV_CODEC_ID_MSMPEG4V1 ||
           id == AV_CODEC_ID_MSMPEG4V2 || id == AV_CODEC_ID_MSMPEG4V3 || id == AV_CODEC_ID_WMV1 ||
           id == AV_CODEC_ID_WMV2 || id == AV_CODEC_ID_MJPEG;
}

class graph_t {
public:
    ~graph_t() {
        avfilter_graph_free(&graph);
    }

    // a graph of the filters in desc, for frames like the first one
    bool open(const std::string &desc,const AVFrame *first,AVRational tb,AVRational sar) {
        char args[512];
        int r;

        if ((graph=avfilter_graph_alloc()) == NULL)
            return false;
        graph->nb_threads = 0;
        snprintf(args,sizeof(args),"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            first->width,first->height,first->format,tb.num,tb.den,sar.num > 0 ? sar.num : 1,sar.num > 0 ? sar.den : 1);
        if ((r=avfilter_graph_create_filter(&src,avfilter_get_by_name("buffer"),"in",args,NULL,graph)) < 0 ||
            (r=avfilter_graph_create_filter(&sink,avfilter_get_by_name("buffersink"),"out",NULL,NULL,graph)) < 0) {
            fprintf(stderr,"Cannot make a filter graph: %s\n",averr(r).c_str());
            return false;
        }

        AVFilterInOut *outputs = avfilter_inout_alloc(),*inputs = avfilter_inout_alloc();
        if (outputs == NULL || inputs == NULL) {
            avfilter_inout_free(&outputs);
            avfilter_inout_free(&inputs);
            return false;
        }
        outputs->name = av_strdup("in");
        outputs->filter_ctx = src;
        inputs->name = av_strdup("out");
        inputs->filter_ctx = sink;
        r = avfilter_graph_parse_ptr(graph,desc.c_str(),&inputs,&outputs,NULL);
        avfilter_inout_free(&outputs);
        avfilter_inout_free(&inputs);
        if (r < 0) {
            fprintf(stderr,"-deblock %s: %s\n",desc.c_str(),averr(r).c_str());
            return false;
        }
        if ((r=avfilter_graph_config(graph,NULL)) < 0) {
            fprintf(stderr,"-deblock %s: %s\n",desc.c_str(),averr(r).c_str());
            return false;
        }
        return true;
    }

    // a frame in (NULL at the end), and each frame out of the graph given to on_frame
    bool filter(AVFrame *f,const std::function<bool(AVFrame*)> &on_frame) {
        int r = av_buffersrc_add_frame_flags(src,f,AV_BUFFERSRC_FLAG_KEEP_REF);
        if (r < 0) {
            fprintf(stderr,"\nFiltering error: %s\n",averr(r).c_str());
            return false;
        }
        AVFrame *o = av_frame_alloc();
        if (o == NULL)
            return false;
        bool ok = true;
        while (ok && (r=av_buffersink_get_frame(sink,o)) >= 0) {
            ok = on_frame(o);
            av_frame_unref(o);
        }
        av_frame_free(&o);
        if (ok && r != AVERROR(EAGAIN) && r != AVERROR_EOF) {
            fprintf(stderr,"\nFiltering error: %s\n",averr(r).c_str());
            ok = false;
        }
        return ok;
    }
private:
    AVFilterGraph*              graph = NULL;
    AVFilterContext*            src = NULL;
    AVFilterContext*            sink = NULL;
};

/////////////////////////////////////////////////////////////////////////////
// the corrections

// make the corrections to a frame of the work format, in place: each component that is corrected, each
// field of it by itself if they are pictures of their own
static void correct_frame(AVFrame *f,const options_t &opt,const video_info_t &vi) {
    const std::vector<comp_t> comps = color_comps((enum AVPixelFormat)f->format);
    const unsigned int nf = vi.fields ? 2u : 1u;
    std::vector<float> buf;

    for (size_t ci=0;ci < comps.size();ci++) {
        // the brightness, or all of RGB, which each have it; the color only if asked
        if (!vi.rgb && ci != 0u && !opt.chroma)
            continue;

        const comp_t &c = comps[ci];
        const int w = AV_CEIL_RSHIFT(f->width,(int)c.hsub);
        const int fh = AV_CEIL_RSHIFT(f->height,(int)c.vsub);
        const float scale = (float)((1u << c.depth) - 1u) / 255.0f;
        const float inv = 1.0f / scale;
        const int maxv = (int)((1u << c.depth) - 1u);

        for (unsigned int field=0;field < nf;field++) {
            const int h = nf == 2u ? ((fh + 1 - (int)field) / 2) : fh;
            buf.resize((size_t)w * (size_t)h);

            // in levels of 0 to 255, whatever the depth
            for (int y=0;y < h;y++) {
                const uint8_t *s = f->data[c.plane] + ((ptrdiff_t)((y * (int)nf) + (int)field) * f->linesize[c.plane]);
                float *d = &buf[(size_t)y * (size_t)w];
                if (c.depth > 8u) {
                    const uint16_t *s16 = (const uint16_t*)s;
                    for (int x=0;x < w;x++)
                        d[x] = (float)s16[x] * inv;
                }
                else {
                    for (int x=0;x < w;x++)
                        d[x] = (float)s[x];
                }
            }

            for (int corr : opt.filters) {
                switch (corr) {
                    case CORR_AVG2Y:
                        avg2y_undo(&buf[0],w,h,w,opt.avg2y);
                        break;
                    default:
                        break;
                }
            }

            for (int y=0;y < h;y++) {
                uint8_t *d = f->data[c.plane] + ((ptrdiff_t)((y * (int)nf) + (int)field) * f->linesize[c.plane]);
                const float *s = &buf[(size_t)y * (size_t)w];
                if (c.depth > 8u) {
                    uint16_t *d16 = (uint16_t*)d;
                    for (int x=0;x < w;x++)
                        d16[x] = (uint16_t)std::min(maxv,std::max(0,(int)lrintf(s[x] * scale)));
                }
                else {
                    for (int x=0;x < w;x++)
                        d[x] = (uint8_t)std::min(maxv,std::max(0,(int)lrintf(s[x])));
                }
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
// the output file

static bool open_output(const options_t &opt,const video_info_t &vi,input_t &in,output_t &out,enum AVPixelFormat &enc_fmt) {
    const AVStream *ist = in.vstream();
    const AVCodecParameters *ipar = ist->codecpar;
    int r;

    if ((r=avformat_alloc_output_context2(&out.oc,NULL,NULL,opt.output.c_str())) < 0 || out.oc == NULL) {
        if ((r=avformat_alloc_output_context2(&out.oc,NULL,"mov",opt.output.c_str())) < 0) {
            fprintf(stderr,"%s: %s\n",opt.output.c_str(),averr(r).c_str());
            return false;
        }
    }
    if ((out.pkt=av_packet_alloc()) == NULL)
        return false;

    // the video encoder
    const AVCodec *codec = choose_encoder(opt,ipar->codec_id,out.oc->oformat);
    if (codec == NULL && !opt.codec.empty()) {
        fprintf(stderr,"No video encoder named %s\n",opt.codec.c_str());
        return false;
    }
    if (codec == NULL) {
        if ((codec=avcodec_find_encoder_by_name("prores_ks")) == NULL) {
            fprintf(stderr,"No encoder for %s video, and no prores_ks: use -c to choose one\n",avcodec_get_name(ipar->codec_id));
            return false;
        }
        fprintf(stderr,"No encoder for %s video, using %s\n",avcodec_get_name(ipar->codec_id),codec->name);
    }

    // the same pixel format as the input, if the encoder can take it
    const enum AVPixelFormat *fmts = encoder_pix_fmts(codec);
    enc_fmt = vi.dec_fmt;
    if (fmts != NULL) {
        bool found = false;
        for (const enum AVPixelFormat *p=fmts;*p != AV_PIX_FMT_NONE;p++) {
            if (*p == vi.dec_fmt) found = true;
        }
        if (!found)
            enc_fmt = avcodec_find_best_pix_fmt_of_list(fmts,vi.dec_fmt,pixfmt_has_alpha(vi.dec_fmt) ? 1 : 0,NULL);
    }
    if (enc_fmt == AV_PIX_FMT_NONE) {
        fprintf(stderr,"The %s encoder takes no pixel format this can give it\n",codec->name);
        return false;
    }

    AVCodecContext *enc = out.enc = avcodec_alloc_context3(codec);
    if (enc == NULL)
        return false;

    const AVRational fr = vi.frame_rate;
    const bool interlaced = field_order_interlaced(vi.field_order);

    enc->width = vi.width;
    enc->height = vi.height;
    enc->pix_fmt = enc_fmt;
    enc->sample_aspect_ratio = vi.sar;
    if (fr.num > 0 && fr.den > 0) {
        enc->framerate = fr;
        enc->time_base = av_inv_q(fr);
    }
    else {
        enc->time_base = ist->time_base;
    }
    enc->field_order = vi.field_order;
    enc->color_range = (pixfmt_is_yuvj(vi.dec_fmt) && !pixfmt_is_yuvj(enc_fmt)) ? AVCOL_RANGE_JPEG : vi.color_range;
    enc->color_primaries = vi.color_primaries;
    enc->color_trc = vi.color_trc;
    enc->colorspace = vi.colorspace;
    enc->chroma_sample_location = vi.chroma_location;
    enc->thread_count = 0;
    if (interlaced && codec_codes_fields(codec->id))
        enc->flags |= AV_CODEC_FLAG_INTERLACED_DCT | AV_CODEC_FLAG_INTERLACED_ME;
    if (out.oc->oformat->flags & AVFMT_GLOBALHEADER)
        enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (codec->id == ipar->codec_id) {
        if (opt.bit_rate <= 0 && ipar->bit_rate > 0)
            enc->bit_rate = ipar->bit_rate;

        // the same kind of ProRes or DNxHR
        const int profile = ipar->profile >= 0 ? ipar->profile : in.dec->profile;
        if (profile >= 0 && (codec->id == AV_CODEC_ID_PRORES || codec->id == AV_CODEC_ID_DNXHD))
            av_opt_set_int(enc->priv_data,"profile",profile,0);
    }
    if (opt.bit_rate > 0)
        enc->bit_rate = opt.bit_rate;

    AVDictionary *eopts = NULL;
    if (!opt.codec_opts.empty() && (r=av_dict_parse_string(&eopts,opt.codec_opts.c_str(),"=",":",0)) < 0) {
        fprintf(stderr,"-x %s: %s\n",opt.codec_opts.c_str(),averr(r).c_str());
        av_dict_free(&eopts);
        return false;
    }
    r = avcodec_open2(enc,codec,&eopts);
    for (const AVDictionaryEntry *e=NULL;(e=av_dict_get(eopts,"",e,AV_DICT_IGNORE_SUFFIX)) != NULL;)
        fprintf(stderr,"The %s encoder has no option %s\n",codec->name,e->key);
    av_dict_free(&eopts);
    if (r < 0) {
        fprintf(stderr,"Cannot open the %s encoder for %dx%d %s: %s (-c chooses another encoder)\n",
            codec->name,vi.width,vi.height,av_get_pix_fmt_name(enc_fmt),averr(r).c_str());
        return false;
    }
    if (fr.num > 0 && fr.den > 0)
        out.frame_duration = std::max((int64_t)1,av_rescale_q(1,av_inv_q(fr),enc->time_base));

    // the streams, in the same order as the input: the video, and the audio copied as it is.
    // A timecode track is made again from the video's timecode, which the video keeps.
    const bool has_timecode = av_dict_get(ist->metadata,"timecode",NULL,0) != NULL && opt.start_us <= 0;
    unsigned int skipped = 0;
    out.map.assign(in.fmt->nb_streams,-1);
    for (unsigned int i=0;i < in.fmt->nb_streams;i++) {
        const AVStream *s = in.fmt->streams[i];
        AVStream *os;

        if ((int)i != in.vindex && s->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
            if (!(has_timecode && s->codecpar->codec_type == AVMEDIA_TYPE_DATA && av_dict_get(s->metadata,"timecode",NULL,0) != NULL))
                skipped++;
            continue;
        }
        if ((os=avformat_new_stream(out.oc,NULL)) == NULL)
            return false;

        av_dict_copy(&os->metadata,s->metadata,0);
        os->disposition = s->disposition;
        out.map[i] = os->index;

        if ((int)i == in.vindex) {
            out.vindex = os->index;
            if ((r=avcodec_parameters_from_context(os->codecpar,enc)) < 0)
                return false;
            os->time_base = enc->time_base;
            os->avg_frame_rate = fr;
            os->sample_aspect_ratio = vi.sar;
            // it was encoded again, by something else
            av_dict_set(&os->metadata,"encoder",NULL,0);
            // which is not the timecode of the frame -ss starts at
            if (opt.start_us > 0)
                av_dict_set(&os->metadata,"timecode",NULL,0);
        }
        else {
            if ((r=avcodec_parameters_copy(os->codecpar,s->codecpar)) < 0)
                return false;
            os->codecpar->codec_tag = copy_codec_tag(out.oc->oformat,s->codecpar);
            os->time_base = s->time_base;
        }
    }
    av_dict_copy(&out.oc->metadata,in.fmt->metadata,0);

    if (!opt.quiet) {
        fprintf(stderr,"Writing %s: %s %s, %u audio stream%s copied",opt.output.c_str(),codec->name,av_get_pix_fmt_name(enc_fmt),
            (unsigned int)(out.oc->nb_streams - 1u),out.oc->nb_streams == 2u ? "" : "s");
        if (has_timecode)
            fprintf(stderr,", timecode kept");
        if (skipped != 0)
            fprintf(stderr,", %u other stream%s left out",skipped,skipped == 1u ? "" : "s");
        fprintf(stderr,"\n");
    }

    if (!(out.oc->oformat->flags & AVFMT_NOFILE)) {
        if ((r=avio_open(&out.oc->pb,opt.output.c_str(),AVIO_FLAG_WRITE)) < 0) {
            fprintf(stderr,"%s: %s\n",opt.output.c_str(),averr(r).c_str());
            return false;
        }
    }
    if ((r=avformat_write_header(out.oc,NULL)) < 0) {
        fprintf(stderr,"%s: cannot write the header: %s\n",opt.output.c_str(),averr(r).c_str());
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////

static bool run(const options_t &opt) {
    input_t in;
    output_t out;
    video_info_t vi;
    enum AVPixelFormat enc_fmt = AV_PIX_FMT_NONE;
    converter_t conv_in,conv_out;
    std::unique_ptr<graph_t> graph;
    std::vector<AVPacket*> early;                   // packets of the other streams before the output is open
    bool opened = false;
    bool ok_write_early = true;
    unsigned long written = 0;
    AVFrame *work = NULL,*conv = NULL;

    in.start_us = opt.start_us;
    in.length_us = opt.length_us;
    in.stop_message = "CTRL+C: stopping here, and finishing the output with what there is (CTRL+C again quits at once)";

    // whether to take the blocks out first, and with what: the post-processing filters need the quantizers
    std::string deblock = opt.deblock;
    {
        input_t probe;
        if (!probe.open(opt.input,true))
            return false;
        const enum AVCodecID id = probe.vstream()->codecpar->codec_id;
        if (deblock == "auto")
            deblock = codec_has_blocks(id) ? "spp" : "none";
        if (deblock == "none")
            deblock.clear();
    }
    in.export_qp = !deblock.empty();
    if (!in.open(opt.input,false))
        return false;

    const AVStream *st = in.vstream();
    const AVRational itb = st->time_base;
    const int64_t offset_us = in.offset_us();       // taken off the time of everything written, for -ss

    vi.frame_rate = av_guess_frame_rate(in.fmt,in.vstream(),NULL);
    progress_t progress("Filtering",frame_count(in),opt.quiet);

    auto write_packet = [&](AVPacket *pkt) -> bool {
        const int o = out.map[(size_t)pkt->stream_index];
        if (o < 0)
            return true;

        const AVRational tb = in.fmt->streams[pkt->stream_index]->time_base;
        if (offset_us != 0) {
            const int64_t off = av_rescale_q(offset_us,av_make_q(1,AV_TIME_BASE),tb);
            if (pkt->pts != AV_NOPTS_VALUE) pkt->pts -= off;
            if (pkt->dts != AV_NOPTS_VALUE) pkt->dts -= off;
        }
        av_packet_rescale_ts(pkt,tb,out.oc->streams[o]->time_base);
        pkt->stream_index = o;
        pkt->pos = -1;

        int r = av_interleaved_write_frame(out.oc,pkt);
        if (r < 0) {
            fprintf(stderr,"\nError writing audio: %s\n",averr(r).c_str());
            return false;
        }
        return true;
    };

    // correct a frame, out of the decoder or the graph, and encode it
    auto encode = [&](AVFrame *frame) -> bool {
        if (frame->width != vi.width || frame->height != vi.height) {
            fprintf(stderr,"\n%s: the frame size changes, which this cannot do\n",opt.input.c_str());
            return false;
        }

        if (work == NULL && (work=alloc_frame(vi.work_fmt,vi.width,vi.height)) == NULL)
            return false;
        if (av_frame_make_writable(work) < 0)
            return false;
        if (frame->format != vi.work_fmt) {
            if (!conv_in.convert(work,frame)) {
                fprintf(stderr,"\nCannot convert %s pixels to %s\n",av_get_pix_fmt_name((enum AVPixelFormat)frame->format),av_get_pix_fmt_name(vi.work_fmt));
                return false;
            }
        }
        else {
            av_frame_copy(work,frame);
        }
        av_frame_copy_props(work,frame);

        correct_frame(work,opt,vi);

        AVFrame *ef;
        if (vi.work_fmt != enc_fmt) {
            if (conv == NULL && (conv=alloc_frame(enc_fmt,vi.width,vi.height)) == NULL)
                return false;
            if (av_frame_make_writable(conv) < 0)
                return false;
            if (!conv_out.convert(conv,work)) {
                fprintf(stderr,"\nCannot convert %s pixels to %s\n",av_get_pix_fmt_name(vi.work_fmt),av_get_pix_fmt_name(enc_fmt));
                return false;
            }
            av_frame_copy_props(conv,work);
            ef = av_frame_clone(conv);
        }
        else {
            // the encoder may hold on to it
            ef = av_frame_clone(work);
        }
        if (ef == NULL)
            return false;
        av_frame_remove_side_data(ef,AV_FRAME_DATA_VIDEO_ENC_PARAMS);

        ef->pict_type = AV_PICTURE_TYPE_NONE;
        ef->sample_aspect_ratio = vi.sar;
        if (out.enc->color_range == AVCOL_RANGE_JPEG)
            ef->color_range = AVCOL_RANGE_JPEG;
        if (field_order_interlaced(vi.field_order) && !frame_is_interlaced(ef))
            frame_set_interlaced(ef,vi.tff);

        // timestamps in the encoder's time base, always going forward
        int64_t pts = frame->best_effort_timestamp != AV_NOPTS_VALUE ? frame->best_effort_timestamp : frame->pts;
        if (pts != AV_NOPTS_VALUE) {
            pts -= av_rescale_q(offset_us,av_make_q(1,AV_TIME_BASE),itb);
            pts = av_rescale_q(pts,itb,out.enc->time_base);
        }
        else {
            pts = (out.last_pts != AV_NOPTS_VALUE) ? (out.last_pts + std::max((int64_t)1,out.frame_duration)) : 0;
        }
        if (out.last_pts != AV_NOPTS_VALUE && pts <= out.last_pts)
            pts = out.last_pts + 1;
        ef->pts = out.last_pts = pts;

        int r = avcodec_send_frame(out.enc,ef);
        av_frame_free(&ef);
        if (r < 0) {
            fprintf(stderr,"\nVideo encoding error: %s\n",averr(r).c_str());
            return false;
        }
        if (!out.write_encoded())
            return false;

        progress.frame(++written);
        return true;
    };

    bool ok = in.run([&](AVFrame *frame) -> bool {
        if (!opened) {
            // what the video is like, from its first frame
            vi.width = frame->width;
            vi.height = frame->height;
            vi.dec_fmt = (enum AVPixelFormat)frame->format;
            vi.sar = frame->sample_aspect_ratio.num > 0 ? frame->sample_aspect_ratio : st->sample_aspect_ratio;
            vi.color_range = frame->color_range;
            vi.color_primaries = frame->color_primaries;
            vi.color_trc = frame->color_trc;
            vi.colorspace = frame->colorspace;
            vi.chroma_location = frame->chroma_location;

            vi.field_order = st->codecpar->field_order;
            if (!field_order_interlaced(vi.field_order) && frame_is_interlaced(frame))
                vi.field_order = frame_is_tff(frame) ? AV_FIELD_TT : AV_FIELD_BB;
            vi.fields = opt.fields >= 0 ? (opt.fields == 1) : field_order_interlaced(vi.field_order);
            vi.tff = !(vi.field_order == AV_FIELD_BB || vi.field_order == AV_FIELD_BT);
            if (vi.fields && vi.height < 16) {
                fprintf(stderr,"%s: too few lines to take the fields apart\n",opt.input.c_str());
                return false;
            }

            if (!deblock.empty()) {
                graph.reset(new graph_t);
                if (!graph->open(deblock,frame,itb,vi.sar))
                    return false;
            }

            if (!opt.quiet) {
                std::string names;
                for (int c : opt.filters)
                    names += std::string(names.empty() ? "" : ",") + corrections[c].name;
                fprintf(stderr,"%s: %dx%d %s, %s, %s\n",opt.input.c_str(),vi.width,vi.height,av_get_pix_fmt_name(vi.dec_fmt),
                    field_order_name(vi.field_order),vi.fields ? "each field corrected by itself" : "each frame corrected as a whole");
                fprintf(stderr,"Taking out the blocks: %s; correcting: %s, of the %s\n",deblock.empty() ? "no" : deblock.c_str(),names.c_str(),
                    opt.chroma ? "brightness and color" : "brightness");
            }

            // the work format is of what comes out of the graph, which may convert it
            vi.work_fmt = work_format_for(vi.dec_fmt);
            vi.rgb = !!(av_pix_fmt_desc_get(vi.work_fmt)->flags & AV_PIX_FMT_FLAG_RGB);

            if (!open_output(opt,vi,in,out,enc_fmt))
                return false;
            opened = true;

            for (AVPacket *p : early) {
                if (ok_write_early && !write_packet(p))
                    ok_write_early = false;
                av_packet_free(&p);
            }
            early.clear();
            if (!ok_write_early)
                return false;
        }

        if (graph)
            return graph->filter(frame,encode);
        return encode(frame);
    },[&](AVPacket *pkt) -> bool {
        // the other streams, as they are, for the part of the video being done
        if (!opened) {
            AVPacket *c = av_packet_clone(pkt);
            if (c == NULL)
                return false;
            early.push_back(c);
            return true;
        }
        return write_packet(pkt);
    });

    for (AVPacket *p : early)
        av_packet_free(&p);

    if (!opened) {
        if (in.stopped)
            fprintf(stderr,"Stopped by CTRL+C before any video, nothing written\n");
        else if (ok)
            fprintf(stderr,"%s: no video %s\n",opt.input.c_str(),opt.start_us > 0 ? "after the start" : "in it");
        return false;
    }

    // what the graph has left
    if (ok && graph)
        ok = graph->filter(NULL,encode);
    progress.done(written);

    // what the encoder has left
    if (ok) {
        int r = avcodec_send_frame(out.enc,NULL);
        if (r < 0 && r != AVERROR_EOF) {
            fprintf(stderr,"Video encoding error: %s\n",averr(r).c_str());
            ok = false;
        }
        else {
            ok = out.write_encoded();
        }
    }

    int r = av_write_trailer(out.oc);
    if (r < 0) {
        fprintf(stderr,"%s: %s\n",opt.output.c_str(),averr(r).c_str());
        ok = false;
    }
    av_frame_free(&work);
    av_frame_free(&conv);
    if (!ok)
        fprintf(stderr,"%s is not complete\n",opt.output.c_str());
    else if (in.stopped)
        fprintf(stderr,"Stopped by CTRL+C: %s has the %lu frames before it, and is complete\n",opt.output.c_str(),written);

    return ok;
}

/////////////////////////////////////////////////////////////////////////////

static bool parse_double(const char *s,double &v) {
    char *end = NULL;
    v = strtod(s,&end);
    return end != s && *end == 0;
}

// a bit rate, such as 50000000, 50000k or 50M
static bool parse_rate(const char *s,int64_t &v) {
    char *end = NULL;
    double d = strtod(s,&end);

    if (end == s)
        return false;
    if (*end == 'k' || *end == 'K') { d *= 1e3; end++; }
    else if (*end == 'M') { d *= 1e6; end++; }
    else if (*end == 'G') { d *= 1e9; end++; }
    if (*end != 0 || d <= 0)
        return false;

    v = (int64_t)d;
    return true;
}

int main(int argc,char **argv) {
    std::vector<std::string> files;
    options_t opt;

    for (int i=1;i < argc;i++) {
        const char *a = argv[i];

        if (a[0] == '-' && a[1] != 0) {
            const std::string o = a + 1;
            // options that take a value
            const bool takes = (o == "filter" || o == "smooth" || o == "iterations" || o == "deblock" ||
                                o == "c" || o == "b" || o == "x" || o == "ss" || o == "t" || o == "threads");
            const char *v = NULL;
            double d;

            if (takes) {
                if (++i >= argc) {
                    fprintf(stderr,"%s needs a value\n",a);
                    return 1;
                }
                v = argv[i];
            }

            if (o == "filter") {
                // names, separated by commas
                std::string s = v;
                size_t p = 0;
                opt.filters.clear();
                while (p <= s.size()) {
                    size_t e = s.find(',',p);
                    if (e == std::string::npos) e = s.size();
                    const std::string name = s.substr(p,e - p);
                    int found = -1;
                    for (size_t k=0;k < sizeof(corrections) / sizeof(corrections[0]);k++) {
                        if (name == corrections[k].name)
                            found = (int)k;
                    }
                    if (found < 0) {
                        fprintf(stderr,"-filter: no correction named \"%s\" (there is:",name.c_str());
                        for (const correction_info_t &c : corrections)
                            fprintf(stderr," %s",c.name);
                        fprintf(stderr,")\n");
                        return 1;
                    }
                    opt.filters.push_back(found);
                    p = e + 1u;
                }
            }
            else if (o == "smooth") {
                if (!parse_double(v,d) || d <= 0 || d > 1000) {
                    fprintf(stderr,"-smooth %s: must be more than 0\n",v);
                    return 1;
                }
                opt.avg2y.smooth = (float)d;
            }
            else if (o == "iterations") {
                if (!parse_double(v,d) || d < 1 || d > 100000 || d != floor(d)) {
                    fprintf(stderr,"-iterations %s: must be 1 or more\n",v);
                    return 1;
                }
                opt.avg2y.iterations = (int)d;
            }
            else if (o == "deblock") {
                opt.deblock = v;
            }
            else if (o == "chroma") {
                opt.chroma = true;
            }
            else if (o == "ss") {
                if (!parse_time(v,opt.start_us)) {
                    fprintf(stderr,"-ss %s: give the time as seconds, such as 90.5, or [HH:]MM:SS[.m...], such as 1:30.5\n",v);
                    return 1;
                }
            }
            else if (o == "t") {
                if (!parse_time(v,opt.length_us) || opt.length_us == 0) {
                    fprintf(stderr,"-t %s: give the time as seconds, such as 90.5, or [HH:]MM:SS[.m...], such as 1:30.5\n",v);
                    return 1;
                }
            }
            else if (o == "c") {
                opt.codec = v;
            }
            else if (o == "b") {
                if (!parse_rate(v,opt.bit_rate)) {
                    fprintf(stderr,"-b %s: give the bit rate as a number, such as 50000000, 50000k or 50M\n",v);
                    return 1;
                }
            }
            else if (o == "x") {
                if (!opt.codec_opts.empty()) opt.codec_opts += ":";
                opt.codec_opts += v;
            }
            else if (o == "threads") {
                if (!parse_double(v,d) || d < 1 || d > 1024 || d != floor(d)) {
                    fprintf(stderr,"-threads %s: must be 1 or more\n",v);
                    return 1;
                }
                opt.threads = (int)d;
            }
            else if (o == "I") {
                opt.fields = 1;
            }
            else if (o == "P") {
                opt.fields = 0;
            }
            else if (o == "y") {
                opt.overwrite = true;
            }
            else if (o == "q") {
                opt.quiet = true;
            }
            else if (o == "h" || o == "-help") {
                help();
                return 0;
            }
            else {
                fprintf(stderr,"Unknown option %s\n",a);
                help();
                return 1;
            }
        }
        else {
            files.push_back(a);
        }
    }

    if (files.size() != 2u) {
        help();
        return 1;
    }
    if (opt.filters.empty())
        opt.filters.push_back(CORR_AVG2Y);
    opt.input = files[0];
    opt.output = files[1];
    if (same_file(opt.input,opt.output)) {
        fprintf(stderr,"%s: the output would write over the input\n",opt.output.c_str());
        return 1;
    }
    if (!opt.overwrite && file_exists(opt.output)) {
        fprintf(stderr,"%s already exists (-y writes over it)\n",opt.output.c_str());
        return 1;
    }

    av_log_set_level(opt.quiet ? AV_LOG_ERROR : AV_LOG_WARNING);

    // OpenMP threads wait for work by spinning, which takes the CPUs from the decoder and encoder
    // if there are as many of them as CPUs
    if (opt.threads > 0)
        omp_set_num_threads(opt.threads);
    else if (getenv("OMP_NUM_THREADS") == NULL)
        omp_set_num_threads(std::max(1,omp_get_num_procs() / 2));

    catch_interrupt();

    return run(opt) ? 0 : 1;
}
