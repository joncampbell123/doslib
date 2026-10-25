/* msvideo-1-fixer: take out what Microsoft Video 1 (CRAM) did to a video.
 *
 * CRAM codes each 4x4 block of the picture as one color, as two colors with a bit to each pixel saying which,
 * or as two colors to each 2x2 quarter. Each pixel of a two color block is made the one of the two it is
 * nearer, which turns a diagonal edge into steps, the noise of a flat part into speckles, and a soft edge into
 * a hard one; and each block of one color is a flat square, which shows as squares in a gradient or the dark.
 *
 * Each packet of the video is read, as well as decoded, for how each block was coded, and the frame put back
 * as it most likely was before: smoothed, along the edges where there are edges, as much as the colors and
 * bits of its blocks allow (see cram.h). The blocks of one color are blended into the blocks around them,
 * each keeping its color as its mean.
 *
 * The video is read once, each frame filtered and encoded again, in RGB. The audio is copied as it is. */

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
#include <map>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/parseutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include <omp.h>

#include "cram.h"

struct options_t {
    std::string                 input;
    std::string                 output;
    std::string                 codec;              // video encoder name
    std::string                 codec_opts;         // key=value:key=value
    enum AVPixelFormat          pix_fmt = AV_PIX_FMT_NONE;     // -pix_fmt, or NONE for the encoder's nearest to RGB
    int64_t                     bit_rate = 0;
    int64_t                     start_us = 0;       // -ss: how far into the input to start, in microseconds
    int64_t                     length_us = -1;     // -t: how much of it to do, -1 for all of it
    int                         threads = 0;        // to work on the pictures with, 0 for half the CPUs
    cram_params_t               cram;
    bool                        show = false;       // -show: the blocks, colored by how they were coded
    bool                        overwrite = false;
    bool                        quiet = false;
};

static void help(void) {
    cram_params_t d;

    fprintf(stderr,"msvideo-1-fixer [options] <input> <output>\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"Takes out the steps on diagonal edges, the speckles and the flat 4x4 squares of video coded\n");
    fprintf(stderr,"with Microsoft Video 1 (CRAM). The video is encoded again, the audio is copied as it is.\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"  -iterations <n>  Turns of smoothing, more for smoother (default %d)\n",d.iterations);
    fprintf(stderr,"  -edge <n>      Levels of change per pixel above which the picture is smoothed only along\n");
    fprintf(stderr,"                 its edges, not across them; less keeps more of the detail (default %g)\n",d.edge);
    fprintf(stderr,"  -rate <n>      How much the blocks of two or eight colors are smoothed, against the blocks\n");
    fprintf(stderr,"                 of one color, which are smoothed the most: more takes out more of the steps\n");
    fprintf(stderr,"                 and speckles, and more of the detail with them (default %g)\n",d.rate);
    fprintf(stderr,"  -noise <n>     Two colors of a block nearer than this, in levels of 0 to 255, are noise, which\n");
    fprintf(stderr,"                 the encoder made speckles of: taken as one color, and smoothed as a block of\n");
    fprintf(stderr,"                 one color. The speckles go, and the noise that they were the most of, which\n");
    fprintf(stderr,"                 is cleaner but less like the noise as it was. 0 not to (default %g)\n",d.noise);
    fprintf(stderr,"  -flat <n>      How far a pixel of a block of one color may be from that color, in levels\n");
    fprintf(stderr,"                 of 0 to 255: more blends the blocks more into each other (default %g)\n",d.flat);
    fprintf(stderr,"  -spread <n>    How far beyond its color a pixel of a two color block may be, as a fraction\n");
    fprintf(stderr,"                 of the way from one color to the other (default %g)\n",d.spread);
    fprintf(stderr,"  -nosolid       Leave the blocks of one color as they are\n");
    fprintf(stderr,"  -show          Instead, show how each block was coded: one color dark, two colors green,\n");
    fprintf(stderr,"                 eight colors blue, and those not coded in the frame (copied from the\n");
    fprintf(stderr,"                 frame before) dimmed\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"Output:\n");
    fprintf(stderr,"  -c <encoder>   Video encoder, such as ffv1, prores_ks or libx264 (default: ffv1 for .mkv,\n");
    fprintf(stderr,"                 .avi and .nut, prores_ks for .mov, libx264 -crf 14 for .mp4)\n");
    fprintf(stderr,"  -pix_fmt <f>   Pixel format to encode, such as yuv420p (default: the encoder's nearest to RGB)\n");
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

// the old full range YUV formats
static bool pixfmt_is_yuvj(enum AVPixelFormat fmt) {
    const char *n = av_get_pix_fmt_name(fmt);
    return n != NULL && strncmp(n,"yuvj",4) == 0;
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
    std::function<void(const AVPacket*)> on_video_packet;  // given each packet of the video before it is decoded
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
                if (on_video_packet)
                    on_video_packet(pkt);
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

// The video encoder: -c, or else one for the kind of file the output is: lossless FFV1 in Matroska, AVI and
// NUT, ProRes in QuickTime, H.264 in MP4, and FFV1 otherwise. Not CRAM again, which would put back what was
// taken out.
static const AVCodec *choose_encoder(const options_t &opt,const AVOutputFormat *ofmt) {
    const char *name = ofmt->name;
    const char *pick = "ffv1";
    const AVCodec *c;

    if (!opt.codec.empty())
        return avcodec_find_encoder_by_name(opt.codec.c_str());

    if (strcmp(name,"mov") == 0)
        pick = "prores_ks";
    else if (strstr(name,"mp4") != NULL)
        pick = "libx264";
    if ((c=avcodec_find_encoder_by_name(pick)) != NULL)
        return c;
    return avcodec_find_encoder(AV_CODEC_ID_FFV1);
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
    AVRational                  sar = {0,1};
    AVRational                  frame_rate = {0,1};
    enum AVColorPrimaries       color_primaries = AVCOL_PRI_UNSPECIFIED;
    enum AVColorTransferCharacteristic color_trc = AVCOL_TRC_UNSPECIFIED;
    bool                        eight_bit = false;  // paletted, as the stream's bits per sample say
};

/////////////////////////////////////////////////////////////////////////////
// the frames, in RGB

// A frame as decoded, paletted or 15-bit RGB, into R, G and B planes of floats, 0 to 255. Each 5 bits of
// 15-bit RGB are made 8 as x * 255 / 31, with the top bits again below them, so that 31 is 255, which
// swscale does not do: it makes 31 248, and the whole picture a little darker. False for any other format.
static bool decoded_to_float(const AVFrame *f,float *rgb) {
    const size_t plane = (size_t)f->width * f->height;

    if (f->format == AV_PIX_FMT_PAL8) {
        const uint32_t *pal = (const uint32_t*)f->data[1];
        for (int y=0;y < f->height;y++) {
            const uint8_t *s = f->data[0] + ((ptrdiff_t)y * f->linesize[0]);
            const size_t o = (size_t)y * f->width;
            for (int x=0;x < f->width;x++) {
                const uint32_t c = pal[s[x]];
                rgb[o + x] = (float)((c >> 16) & 0xFFu);
                rgb[plane + o + x] = (float)((c >> 8) & 0xFFu);
                rgb[(2u * plane) + o + x] = (float)(c & 0xFFu);
            }
        }
        return true;
    }
    if (f->format == AV_PIX_FMT_RGB555LE || f->format == AV_PIX_FMT_RGB555BE) {
        const bool be = f->format == AV_PIX_FMT_RGB555BE;
        for (int y=0;y < f->height;y++) {
            const uint8_t *s = f->data[0] + ((ptrdiff_t)y * f->linesize[0]);
            const size_t o = (size_t)y * f->width;
            for (int x=0;x < f->width;x++) {
                const unsigned int c = be ? ((unsigned int)(s[x * 2] << 8u) | s[(x * 2) + 1]) : ((unsigned int)(s[(x * 2) + 1] << 8u) | s[x * 2]);
                const unsigned int r = (c >> 10u) & 31u,g = (c >> 5u) & 31u,b = c & 31u;
                rgb[o + x] = (float)((r << 3u) | (r >> 2u));
                rgb[plane + o + x] = (float)((g << 3u) | (g >> 2u));
                rgb[(2u * plane) + o + x] = (float)((b << 3u) | (b >> 2u));
            }
        }
        return true;
    }
    return false;
}

// a frame of the GBRP format, its planes of 0 to 255 into R, G and B planes of floats
static void gbrp_to_float(const AVFrame *f,float *rgb) {
    const size_t plane = (size_t)f->width * f->height;
    static const int src[3] = { 2,0,1 };            // GBRP has G, B, R

    for (int i=0;i < 3;i++) {
        for (int y=0;y < f->height;y++) {
            const uint8_t *s = f->data[src[i]] + ((ptrdiff_t)y * f->linesize[src[i]]);
            float *d = rgb + ((size_t)i * plane) + ((size_t)y * f->width);
            for (int x=0;x < f->width;x++)
                d[x] = (float)s[x];
        }
    }
}

static void float_to_gbrp(AVFrame *f,const float *rgb) {
    const size_t plane = (size_t)f->width * f->height;
    static const int dst[3] = { 2,0,1 };

    for (int i=0;i < 3;i++) {
        for (int y=0;y < f->height;y++) {
            const float *s = rgb + ((size_t)i * plane) + ((size_t)y * f->width);
            uint8_t *d = f->data[dst[i]] + ((ptrdiff_t)y * f->linesize[dst[i]]);
            for (int x=0;x < f->width;x++)
                d[x] = (uint8_t)std::min(255,std::max(0,(int)lrintf(s[x])));
        }
    }
}

// -show: each block colored by how it was coded, in place of the picture
static void show_blocks(float *rgb,int w,int h,const cram_map_t &map) {
    const size_t plane = (size_t)w * h;

    for (int by=0;by < map.bh;by++) {
        for (int bx=0;bx < map.bw;bx++) {
            const cram_block_t &b = map.at(bx,by);
            const float dim = map.coded[((size_t)by * map.bw) + bx] ? 1.0f : 0.5f;
            for (int py=0;py < 4;py++) {
                for (int px=0;px < 4;px++) {
                    const size_t o = ((size_t)((by * 4) + py) * w) + (bx * 4) + px;
                    const float Y = (rgb[o] * 0.299f) + (rgb[plane + o] * 0.587f) + (rgb[(2u * plane) + o] * 0.114f);
                    float c[3];
                    switch (b.mode) {
                        case CRAM_SOLID:    c[0] = c[1] = c[2] = Y * 0.35f; break;
                        case CRAM_TWO:      c[0] = c[2] = Y * 0.3f; c[1] = 80.0f + (Y * 0.6f); break;
                        case CRAM_EIGHT:    c[0] = c[1] = Y * 0.3f; c[2] = 80.0f + (Y * 0.6f); break;
                        default:            c[0] = 200.0f; c[1] = c[2] = 0.0f; break;
                    }
                    for (int i=0;i < 3;i++)
                        rgb[((size_t)i * plane) + o] = c[i] * dim;
                }
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
// the output file

static bool open_output(const options_t &opt,const video_info_t &vi,input_t &in,output_t &out,enum AVPixelFormat &enc_fmt) {
    const AVStream *ist = in.vstream();
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
    const AVCodec *codec = choose_encoder(opt,out.oc->oformat);
    if (codec == NULL) {
        if (!opt.codec.empty())
            fprintf(stderr,"No video encoder named %s\n",opt.codec.c_str());
        else
            fprintf(stderr,"No FFV1 encoder: use -c to choose one\n");
        return false;
    }

    // -pix_fmt, or the encoder's nearest to the RGB the frames are made in
    const enum AVPixelFormat *fmts = encoder_pix_fmts(codec);
    enc_fmt = opt.pix_fmt != AV_PIX_FMT_NONE ? opt.pix_fmt : AV_PIX_FMT_GBRP;
    if (fmts != NULL) {
        bool found = false;
        for (const enum AVPixelFormat *p=fmts;*p != AV_PIX_FMT_NONE;p++) {
            if (*p == enc_fmt) found = true;
        }
        if (!found && opt.pix_fmt != AV_PIX_FMT_NONE) {
            fprintf(stderr,"The %s encoder cannot take %s pixels\n",codec->name,av_get_pix_fmt_name(opt.pix_fmt));
            return false;
        }
        if (!found)
            enc_fmt = avcodec_find_best_pix_fmt_of_list(fmts,AV_PIX_FMT_GBRP,0,NULL);
    }
    if (enc_fmt == AV_PIX_FMT_NONE) {
        fprintf(stderr,"The %s encoder takes no pixel format this can give it\n",codec->name);
        return false;
    }
    const bool enc_rgb = !!(av_pix_fmt_desc_get(enc_fmt)->flags & AV_PIX_FMT_FLAG_RGB);

    AVCodecContext *enc = out.enc = avcodec_alloc_context3(codec);
    if (enc == NULL)
        return false;

    const AVRational fr = vi.frame_rate;

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
    enc->field_order = AV_FIELD_PROGRESSIVE;
    // RGB as it is, or YUV of standard definition video, as the frames are converted to it
    enc->color_range = enc_rgb ? AVCOL_RANGE_JPEG : (pixfmt_is_yuvj(enc_fmt) ? AVCOL_RANGE_JPEG : AVCOL_RANGE_MPEG);
    enc->colorspace = enc_rgb ? AVCOL_SPC_RGB : AVCOL_SPC_BT470BG;
    enc->color_primaries = vi.color_primaries;
    enc->color_trc = vi.color_trc;
    enc->thread_count = 0;
    if (out.oc->oformat->flags & AVFMT_GLOBALHEADER)
        enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (opt.bit_rate > 0)
        enc->bit_rate = opt.bit_rate;
    // H.264 near enough to the picture not to put back the steps that were taken out
    if (strcmp(codec->name,"libx264") == 0 && opt.bit_rate <= 0 && opt.codec_opts.find("crf") == std::string::npos)
        av_opt_set(enc->priv_data,"crf","14",0);
    // FFV1 in slices, which are encoded at once on the CPUs, a frame a CPU being too slow for 60 of them a second
    if (strcmp(codec->name,"ffv1") == 0 && opt.codec_opts.empty()) {
        av_opt_set_int(enc->priv_data,"slices",16,0);
        enc->level = 3;
    }

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
    std::vector<AVPacket*> early;                   // packets of the other streams before the output is open
    bool opened = false;
    bool ok_write_early = true;
    unsigned long written = 0;
    AVFrame *rgb = NULL,*conv = NULL;
    std::vector<float> fin,fout;
    cram_map_t map;                                 // the blocks, as of the last packet read
    std::map<int64_t,cram_map_t> pending;           // the blocks of each packet not yet decoded, by its time
    cram_params_t cp = opt.cram;

    in.start_us = opt.start_us;
    in.length_us = opt.length_us;
    in.stop_message = "CTRL+C: stopping here, and finishing the output with what there is (CTRL+C again quits at once)";
    if (!in.open(opt.input,false))
        return false;

    const AVStream *st = in.vstream();
    const AVRational itb = st->time_base;
    const int64_t offset_us = in.offset_us();       // taken off the time of everything written, for -ss

    if (st->codecpar->codec_id != AV_CODEC_ID_MSVIDEO1) {
        fprintf(stderr,"%s: the video is %s, not Microsoft Video 1 (CRAM)\n",opt.input.c_str(),avcodec_get_name(st->codecpar->codec_id));
        return false;
    }
    // as FFmpeg's decoder decides it: 8 bits a pixel is paletted, anything else 15-bit RGB, and the colors
    // rounded to a palette or to 5 bits each
    vi.eight_bit = st->codecpar->bits_per_coded_sample == 8;
    cp.tolerance = vi.eight_bit ? 3.0f : 4.5f;
    map.alloc(st->codecpar->width,st->codecpar->height);

    // each packet read, before it is decoded, for how its blocks were coded
    in.on_video_packet = [&](const AVPacket *pkt) {
        if (!map.parse(pkt->data,(size_t)pkt->size,vi.eight_bit))
            return;
        const int64_t t = pkt->pts != AV_NOPTS_VALUE ? pkt->pts : pkt->dts;
        pending[t] = map;
        while (pending.size() > 16u)
            pending.erase(pending.begin());
    };

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

    // put a frame back as it was, and encode it
    auto encode = [&](AVFrame *frame) -> bool {
        if (frame->width != vi.width || frame->height != vi.height) {
            fprintf(stderr,"\n%s: the frame size changes, which this cannot do\n",opt.input.c_str());
            return false;
        }

        // the blocks of its packet, or if it cannot be found by its time, of the last packet, which the
        // decoder, a frame out for each packet in, has just decoded
        const cram_map_t *fm = &map;
        const int64_t t = frame->pts != AV_NOPTS_VALUE ? frame->pts : frame->pkt_dts;
        std::map<int64_t,cram_map_t>::iterator pi = pending.find(t);
        if (pi != pending.end())
            fm = &pi->second;

        if (rgb == NULL && (rgb=alloc_frame(AV_PIX_FMT_GBRP,vi.width,vi.height)) == NULL)
            return false;
        if (av_frame_make_writable(rgb) < 0)
            return false;
        if (!decoded_to_float(frame,&fin[0])) {
            if (!conv_in.convert(rgb,frame)) {
                fprintf(stderr,"\nCannot convert %s pixels to RGB\n",av_get_pix_fmt_name((enum AVPixelFormat)frame->format));
                return false;
            }
            gbrp_to_float(rgb,&fin[0]);
        }
        av_frame_copy_props(rgb,frame);
        rgb->color_range = AVCOL_RANGE_UNSPECIFIED;
        rgb->colorspace = AVCOL_SPC_UNSPECIFIED;

        if (opt.show) {
            show_blocks(&fin[0],vi.width,vi.height,*fm);
            float_to_gbrp(rgb,&fin[0]);
        }
        else {
            cram_restore(&fin[0],&fout[0],vi.width,vi.height,*fm,cp);
            float_to_gbrp(rgb,&fout[0]);
        }
        if (pi != pending.end())
            pending.erase(pending.begin(),++pi);

        AVFrame *ef;
        if (enc_fmt != AV_PIX_FMT_GBRP) {
            if (conv == NULL && (conv=alloc_frame(enc_fmt,vi.width,vi.height)) == NULL)
                return false;
            if (av_frame_make_writable(conv) < 0)
                return false;
            if (!conv_out.convert(conv,rgb)) {
                fprintf(stderr,"\nCannot convert RGB pixels to %s\n",av_get_pix_fmt_name(enc_fmt));
                return false;
            }
            av_frame_copy_props(conv,rgb);
            ef = av_frame_clone(conv);
        }
        else {
            // the encoder may hold on to it
            ef = av_frame_clone(rgb);
        }
        if (ef == NULL)
            return false;

        ef->pict_type = AV_PICTURE_TYPE_NONE;
        ef->sample_aspect_ratio = vi.sar;
        ef->color_range = out.enc->color_range;
        ef->colorspace = out.enc->colorspace;

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
            vi.color_primaries = frame->color_primaries;
            vi.color_trc = frame->color_trc;
            if (vi.width != map.bw * 4 || vi.height != map.bh * 4) {
                if (vi.width < 4 || vi.height < 4) {
                    fprintf(stderr,"%s: too small for a block\n",opt.input.c_str());
                    return false;
                }
                // the pixels to the right of or below the last whole block, which the decoder does not
                // touch, are left as they are
                if (!opt.quiet)
                    fprintf(stderr,"%s: %dx%d is not a multiple of 4: the pixels past the last blocks are left as they are\n",
                        opt.input.c_str(),vi.width,vi.height);
            }
            fin.resize((size_t)vi.width * vi.height * 3u);
            fout.resize(fin.size());

            if (!opt.quiet) {
                fprintf(stderr,"%s: %dx%d Microsoft Video 1, %s\n",opt.input.c_str(),vi.width,vi.height,
                    vi.eight_bit ? "8-bit paletted" : "15-bit RGB");
                if (opt.show)
                    fprintf(stderr,"Showing how the blocks were coded\n");
                else
                    fprintf(stderr,"Smoothing: %d turns, edges above %g levels a pixel, blocks of one color %s\n",cp.iterations,cp.edge,
                        cp.solid ? "blended" : "left as they are");
            }

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
    av_frame_free(&rgb);
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
            const bool takes = (o == "iterations" || o == "edge" || o == "rate" || o == "noise" || o == "flat" || o == "spread" || o == "pix_fmt" ||
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

            if (o == "iterations") {
                if (!parse_double(v,d) || d < 0 || d > 10000 || d != floor(d)) {
                    fprintf(stderr,"-iterations %s: must be 0 or more\n",v);
                    return 1;
                }
                opt.cram.iterations = (int)d;
            }
            else if (o == "edge") {
                if (!parse_double(v,d) || d <= 0 || d > 1000) {
                    fprintf(stderr,"-edge %s: must be more than 0\n",v);
                    return 1;
                }
                opt.cram.edge = (float)d;
            }
            else if (o == "rate") {
                if (!parse_double(v,d) || d < 0 || d > 1) {
                    fprintf(stderr,"-rate %s: must be 0 to 1\n",v);
                    return 1;
                }
                opt.cram.rate = (float)d;
            }
            else if (o == "noise") {
                if (!parse_double(v,d) || d < 0 || d > 255) {
                    fprintf(stderr,"-noise %s: must be 0 to 255\n",v);
                    return 1;
                }
                opt.cram.noise = (float)d;
            }
            else if (o == "flat") {
                if (!parse_double(v,d) || d < 0 || d > 255) {
                    fprintf(stderr,"-flat %s: must be 0 to 255\n",v);
                    return 1;
                }
                opt.cram.flat = (float)d;
            }
            else if (o == "spread") {
                if (!parse_double(v,d) || d < 0 || d > 10) {
                    fprintf(stderr,"-spread %s: must be 0 to 10\n",v);
                    return 1;
                }
                opt.cram.spread = (float)d;
            }
            else if (o == "nosolid") {
                opt.cram.solid = false;
            }
            else if (o == "show") {
                opt.show = true;
            }
            else if (o == "pix_fmt") {
                if ((opt.pix_fmt=av_get_pix_fmt(v)) == AV_PIX_FMT_NONE) {
                    fprintf(stderr,"-pix_fmt %s: no such pixel format\n",v);
                    return 1;
                }
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
