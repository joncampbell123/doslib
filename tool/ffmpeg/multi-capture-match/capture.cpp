#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include <algorithm>
#include <atomic>
#include <complex>
#include <thread>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include "capture.h"

// set by CTRL+C
extern volatile sig_atomic_t interrupted;

// the audio is looked at as mono at this rate, which is enough for the spectrum up to AUDIO_TOP
static const int AUDIO_RATE = 16000;
static const int AUDIO_FFT = 512;                   // samples of each slice the spectrum is of, about 1/30 second
static const double AUDIO_BOTTOM = 125.0;           // Hz, the bands, spaced evenly in pitch
static const double AUDIO_TOP = 7000.0;
static const uint8_t AUDIO_SILENT = 10;             // a slice with every band below this (-75 dB) is digital silence

// the parts of each line of a frame looked at, across, for dropouts
static const int CELLS_ACROSS = 16;

// A cell of a frame is a dropout where it differs from the pictures two frames before and after it, the
// pictures of the same field of a video of fields (a VHS capture at 59.94 frames a second is), by this
// many times, and this many levels more than, how much they differ from each other: a dropout is in one
// field only, and the pictures either side of it are the same where nothing moves.
static const float DROPOUT_RATIO = 2.5f;
static const float DROPOUT_LEVELS = 5.0f;

// A cell is a dropout too where it differs from the lines three above and below it by this many times, and
// this many levels more than, how much they differ from each other: a streak a line or two high, which
// a dropout that stays on the same lines for many frames, as from a crease, is, where the frames around it
// cannot tell. Real things a line or two high are rarer than edges, and every capture has them.
static const float STREAK_RATIO = 3.0f;
static const float STREAK_LEVELS = 12.0f;

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC,&ts);
    return (double)ts.tv_sec + ((double)ts.tv_nsec / 1e9);
}

static std::string averr(int e) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(e,buf,sizeof(buf));
    return buf;
}

/////////////////////////////////////////////////////////////////////////////
// the thumbnail, and the dropouts and noise, of each frame

// the thumbnail of a frame of 8-bit gray: the middle of the picture, leaving out the blanking at the
// sides and the head switching noise at the bottom, each sample the mean of its part of it.
// (as vhs-dropout-filter makes them, for match_frames)
static void make_thumb(uint8_t *t,const uint8_t *g,ptrdiff_t stride,int width,int height) {
    const int x0 = width / 18,x1 = width - (width / 18);
    const int y0 = height / 12,y1 = height - (height / 12);

    for (int ty=0;ty < THUMB_H;ty++) {
        const int ya = y0 + (((y1 - y0) * ty) / THUMB_H),yb = std::max(ya + 1,y0 + (((y1 - y0) * (ty + 1)) / THUMB_H));
        for (int tx=0;tx < THUMB_W;tx++) {
            const int xa = x0 + (((x1 - x0) * tx) / THUMB_W),xb = std::max(xa + 1,x0 + (((x1 - x0) * (tx + 1)) / THUMB_W));
            unsigned int sum = 0;
            for (int y=ya;y < yb;y++) {
                const uint8_t *r = g + ((ptrdiff_t)y * stride);
                for (int x=xa;x < xb;x++)
                    sum += r[x];
            }
            t[(ty * THUMB_W) + tx] = (uint8_t)((sum + (unsigned int)(((yb - ya) * (xb - xa)) / 2)) / (unsigned int)((yb - ya) * (xb - xa)));
        }
    }
}

// the lines and parts across of a frame that are looked at for dropouts: not the blanking at the sides,
// nor the head switching noise at the bottom
struct cell_area_t {
    int x0,x1,y0,y1;

    cell_area_t(int w,int h) : x0(w / 18), x1(w - (w / 18)), y0(h / 24), y1(h - (h / 12)) { }

    unsigned int cells(void) const {
        return (unsigned int)((y1 - y0) * CELLS_ACROSS);
    }
};

// How many cells of frame c look like dropouts, from the frames two before and after it, p and n, and how
// noisy it is: the least that p and n differ, where they differ the least, as nothing moves there.
static void frame_damage(uint16_t &damage,float &noise,const uint8_t *p,const uint8_t *c,const uint8_t *n,int w,int h) {
    const cell_area_t a(w,h);
    std::vector<float> still;
    unsigned int count = 0;

    still.reserve(a.cells());
    for (int y=a.y0;y < a.y1;y++) {
        const uint8_t *pr = p + ((size_t)y * (size_t)w),*cr = c + ((size_t)y * (size_t)w),*nr = n + ((size_t)y * (size_t)w);
        const uint8_t *ur = c + ((size_t)std::max(0,y - 3) * (size_t)w),*dr = c + ((size_t)std::min(h - 1,y + 3) * (size_t)w);
        for (int k=0;k < CELLS_ACROSS;k++) {
            const int xa = a.x0 + (((a.x1 - a.x0) * k) / CELLS_ACROSS),xb = a.x0 + (((a.x1 - a.x0) * (k + 1)) / CELLS_ACROSS);
            unsigned int dp = 0,dn = 0,dpn = 0,du = 0,dd = 0,dud = 0;
            for (int x=xa;x < xb;x++) {
                dp += (unsigned int)abs((int)cr[x] - (int)pr[x]);
                dn += (unsigned int)abs((int)cr[x] - (int)nr[x]);
                dpn += (unsigned int)abs((int)pr[x] - (int)nr[x]);
                du += (unsigned int)abs((int)cr[x] - (int)ur[x]);
                dd += (unsigned int)abs((int)cr[x] - (int)dr[x]);
                dud += (unsigned int)abs((int)ur[x] - (int)dr[x]);
            }
            const float inv = 1.0f / (float)std::max(1,xb - xa);
            const float fpn = (float)dpn * inv;
            if ((float)std::min(dp,dn) * inv > (DROPOUT_RATIO * fpn) + DROPOUT_LEVELS ||
                (float)std::min(du,dd) * inv > (STREAK_RATIO * (float)dud * inv) + STREAK_LEVELS)
                count++;
            still.push_back(fpn);
        }
    }

    damage = (uint16_t)std::min(count,65535u);
    noise = 0;
    if (!still.empty()) {
        const size_t k = still.size() / 10u;
        std::nth_element(still.begin(),still.begin() + (ptrdiff_t)k,still.end());
        noise = still[k] / sqrtf(2.0f);
    }
}

/////////////////////////////////////////////////////////////////////////////
// the spectrum of the audio

typedef std::complex<float> cplxf;

static void fft(std::vector<cplxf> &a) {
    const size_t n = a.size();

    for (size_t i=1,j=0;i < n;i++) {
        size_t bit = n >> 1u;
        for (;j & bit;bit >>= 1u)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i],a[j]);
    }
    for (size_t len=2;len <= n;len <<= 1u) {
        const double ang = -2.0 * M_PI / (double)len;
        const cplxf wl((float)cos(ang),(float)sin(ang));
        for (size_t i=0;i < n;i += len) {
            cplxf w(1.0f,0.0f);
            for (size_t j=0;j < len / 2u;j++) {
                const cplxf u = a[i + j],v = a[i + j + (len / 2u)] * w;
                a[i + j] = u + v;
                a[i + j + (len / 2u)] = u - v;
                w *= wl;
            }
        }
    }
}

// the spectrum of the audio of a capture, a slice of it for each frame, as the audio is decoded
class spectrum_t {
public:
    spectrum_t(double frame_seconds) : step(frame_seconds) {
        window.resize(AUDIO_FFT);
        for (int i=0;i < AUDIO_FFT;i++)
            window[i] = (float)(0.5 - (0.5 * cos((2.0 * M_PI * (i + 0.5)) / AUDIO_FFT)));
        // the bins of each band, spaced evenly in pitch
        const double bin = (double)AUDIO_RATE / AUDIO_FFT;
        for (int b=0;b <= AUDIO_BANDS;b++) {
            const double f = AUDIO_BOTTOM * pow(AUDIO_TOP / AUDIO_BOTTOM,(double)b / AUDIO_BANDS);
            edge[b] = std::max(1,(int)lround(f / bin));
        }
        for (int b=0;b < AUDIO_BANDS;b++)
            edge[b + 1] = std::max(edge[b + 1],edge[b] + 1);
        buf.assign(AUDIO_FFT,0.0f);                 // silence before the start
        base = -(int64_t)(AUDIO_FFT / 2);
    }

    // the audio starts this many seconds into the file
    void start(double t) {
        start_time = t;
    }

    // more of the audio, mono at AUDIO_RATE, and each slice there is now enough of
    void add(const float *s,size_t n,std::vector<uint8_t> &out) {
        buf.insert(buf.end(),s,s + n);
        slices(out,false);
    }

    // the rest of the slices, with silence after the end
    void finish(std::vector<uint8_t> &out) {
        slices(out,true);
    }
private:
    void slices(std::vector<uint8_t> &out,bool end) {
        std::vector<cplxf> a(AUDIO_FFT);
        for (;;) {
            const size_t k = out.size() / AUDIO_BANDS;
            // the middle of slice k, in samples of the audio, from where it starts
            const int64_t mid = (int64_t)llround((((double)k * step) - start_time) * AUDIO_RATE);
            const int64_t first = mid - (AUDIO_FFT / 2);
            const int64_t have = base + (int64_t)buf.size();
            if (first + AUDIO_FFT > have && !(end && first < have))
                break;

            for (int i=0;i < AUDIO_FFT;i++) {
                const int64_t si = first + i - base;
                const float v = (si >= 0 && si < (int64_t)buf.size()) ? buf[(size_t)si] : 0.0f;
                a[i] = cplxf(v * window[i],0.0f);
            }
            fft(a);
            for (int b=0;b < AUDIO_BANDS;b++) {
                double e = 0;
                for (int i=edge[b];i < edge[b + 1];i++)
                    e += std::norm(a[i]);
                // in dB, 2 levels a dB, from -80 dB
                const double db = 10.0 * log10(e + 1e-12);
                out.push_back((uint8_t)std::max(0.0,std::min(255.0,2.0 * (db + 80.0))));
            }

            // what is not needed again
            const int64_t keep = first - base;
            if (keep > 65536) {
                buf.erase(buf.begin(),buf.begin() + (ptrdiff_t)keep);
                base += keep;
            }
        }
    }

    double                      step;               // seconds of a frame
    double                      start_time = 0;
    std::vector<float>          window;
    int                         edge[AUDIO_BANDS + 1];
    std::vector<float>          buf;                // the audio, from sample base
    int64_t                     base = 0;
};

void audio_thumb(uint8_t *t,const capture_t &c,size_t i) {
    const size_t slices = c.audio.size() / AUDIO_BANDS;
    const int64_t mid = (int64_t)llround(c.thumbs.time[i] * c.frame_rate());

    for (int r=0;r < THUMB_H;r++) {
        const int64_t k = mid - (THUMB_H / 2) + r;
        bool silent = true;
        if (k >= 0 && k < (int64_t)slices) {
            memcpy(t + (r * THUMB_W),&c.audio[(size_t)k * AUDIO_BANDS],AUDIO_BANDS);
            for (int b=0;b < AUDIO_BANDS && silent;b++)
                silent = t[(r * THUMB_W) + b] < AUDIO_SILENT;
        }
        // Audio that is not there, or is digital silence, as before a capture starts or a card has muted it,
        // is blank: the step from it to the sound would be all that is matched, and match any other such step.
        // Sound from a tape always has some hiss.
        if (silent) {
            memset(t,0,THUMB_SIZE);
            return;
        }
    }
}

void audio_thumbs(thumbs_t &out,const capture_t &c) {
    out = thumbs_t();
    out.time = c.thumbs.time;
    out.v.resize(c.frames() * THUMB_SIZE);
    for (size_t i=0;i < c.frames();i++)
        audio_thumb(&out.v[i * THUMB_SIZE],c,i);
}

/////////////////////////////////////////////////////////////////////////////
// reading a capture

// The frame rate, as the standard rate it is near, if it is: a file with a time base of milliseconds, as
// Matroska has, says 59.94 frames a second as something a little off it, and an editor counts frames at
// the rate itself.
static AVRational standard_rate(AVRational r) {
    static const int rates[][2] = {
        { 24000,1001 },{ 24,1 },{ 25,1 },{ 30000,1001 },{ 30,1 },{ 48,1 },{ 50,1 },{ 60000,1001 },{ 60,1 },
        { 100,1 },{ 120000,1001 },{ 120,1 },{ 15,1 },{ 12,1 }
    };
    if (r.num <= 0 || r.den <= 0)
        return r;
    const double v = av_q2d(r);
    for (const auto &s : rates) {
        const double sv = (double)s[0] / s[1];
        if (fabs(v - sv) < sv * 0.0005)
            return av_make_q(s[0],s[1]);
    }
    return r;
}

// how far along each capture being read is, for the progress shown
struct progress_t {
    std::atomic<unsigned long>  frames{0};
    int64_t                     total = 0;          // about how many frames it has, or 0
    std::atomic<bool>           done{false};
};

static bool read_capture(capture_t &c,progress_t &prog,int threads) {
    AVFormatContext *fmt = NULL;
    AVCodecContext *vdec = NULL,*adec = NULL;
    struct SwsContext *sws = NULL;
    SwrContext *swr = NULL;
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    bool ok = false;
    int r,vindex = -1,aindex = -1;

    auto open_decoder = [&](int index,AVCodecContext *&dec) -> bool {
        const AVStream *st = fmt->streams[index];
        const AVCodec *codec = avcodec_find_decoder(st->codecpar->codec_id);
        if (codec == NULL) {
            fprintf(stderr,"%s: no decoder for %s\n",c.path.c_str(),avcodec_get_name(st->codecpar->codec_id));
            return false;
        }
        if ((dec=avcodec_alloc_context3(codec)) == NULL || avcodec_parameters_to_context(dec,st->codecpar) < 0)
            return false;
        dec->pkt_timebase = st->time_base;
        dec->thread_count = threads;
        if ((r=avcodec_open2(dec,codec,NULL)) < 0) {
            fprintf(stderr,"%s: cannot open the %s decoder: %s\n",c.path.c_str(),codec->name,averr(r).c_str());
            return false;
        }
        return true;
    };

    // the last five frames in gray, for the dropouts and noise of the one in the middle of them
    std::vector<std::vector<uint8_t> > ring(5);
    std::vector<uint8_t> gray;
    spectrum_t *spec = NULL;
    std::vector<float> mono;
    int64_t t0 = 0;
    double step = 0;
    unsigned int errors = 0;

    c.thumbs = thumbs_t();
    c.damage.clear();
    c.noise.clear();
    c.audio.clear();

    // a frame of the video, decoded
    auto on_video = [&](AVFrame *f) -> bool {
        const int w = f->width,h = f->height;
        const AVPixFmtDescriptor *d = av_pix_fmt_desc_get((enum AVPixelFormat)f->format);
        const uint8_t *g;
        ptrdiff_t stride;

        // 8-bit luma as it is, or anything else as 8-bit gray
        if (d != NULL && !(d->flags & (AV_PIX_FMT_FLAG_RGB | AV_PIX_FMT_FLAG_PAL)) && d->nb_components >= 1 &&
            d->comp[0].plane == 0 && d->comp[0].depth == 8 && d->comp[0].step == 1 && (d->flags & AV_PIX_FMT_FLAG_PLANAR)) {
            g = f->data[0];
            stride = f->linesize[0];
        }
        else {
            sws = sws_getCachedContext(sws,w,h,(enum AVPixelFormat)f->format,w,h,AV_PIX_FMT_GRAY8,SWS_BILINEAR,NULL,NULL,NULL);
            if (sws == NULL) {
                fprintf(stderr,"%s: cannot convert %s pixels to gray\n",c.path.c_str(),av_get_pix_fmt_name((enum AVPixelFormat)f->format));
                return false;
            }
            gray.resize((size_t)w * (size_t)h);
            uint8_t *dst[4] = { &gray[0],NULL,NULL,NULL };
            int dls[4] = { w,0,0,0 };
            sws_scale(sws,f->data,f->linesize,0,h,dst,dls);
            g = &gray[0];
            stride = w;
        }
        if (w != c.width || h != c.height) {
            if (errors++ < 10u)
                fprintf(stderr,"%s: a frame is %dx%d, not %dx%d, and is left out\n",c.path.c_str(),w,h,c.width,c.height);
            return true;
        }

        const size_t n = c.thumbs.count();
        c.thumbs.v.resize((n + 1u) * THUMB_SIZE);
        make_thumb(&c.thumbs.v[n * THUMB_SIZE],g,stride,w,h);
        if (f->best_effort_timestamp != AV_NOPTS_VALUE)
            c.thumbs.time.push_back((double)(av_rescale_q(f->best_effort_timestamp,fmt->streams[vindex]->time_base,AV_TIME_BASE_Q) - t0) / AV_TIME_BASE);
        else
            c.thumbs.time.push_back(n != 0u ? c.thumbs.time[n - 1u] + step : 0.0);

        // the dropouts and noise of the frame two before this one
        std::vector<uint8_t> cur = std::move(ring[0]);
        for (int i=0;i < 4;i++)
            ring[i] = std::move(ring[i + 1]);
        cur.resize((size_t)w * (size_t)h);
        for (int y=0;y < h;y++)
            memcpy(&cur[(size_t)y * (size_t)w],g + (y * stride),(size_t)w);
        ring[4] = std::move(cur);
        c.damage.push_back(0);
        c.noise.push_back(-1.0f);
        if (n >= 4u)
            frame_damage(c.damage[n - 2u],c.noise[n - 2u],&ring[0][0],&ring[2][0],&ring[4][0],w,h);

        prog.frames = n + 1u;
        return true;
    };

    // audio, decoded
    auto on_audio = [&](AVFrame *f) -> bool {
        if (swr == NULL) {
            AVChannelLayout mono_layout;
            av_channel_layout_default(&mono_layout,1);
            if (swr_alloc_set_opts2(&swr,&mono_layout,AV_SAMPLE_FMT_FLT,AUDIO_RATE,&f->ch_layout,(enum AVSampleFormat)f->format,f->sample_rate,0,NULL) < 0 ||
                swr_init(swr) < 0) {
                fprintf(stderr,"%s: cannot convert the audio\n",c.path.c_str());
                return false;
            }
            if (f->best_effort_timestamp != AV_NOPTS_VALUE)
                spec->start((double)(av_rescale_q(f->best_effort_timestamp,fmt->streams[aindex]->time_base,AV_TIME_BASE_Q) - t0) / AV_TIME_BASE);
        }
        const int out = swr_get_out_samples(swr,f->nb_samples);
        if (out <= 0)
            return true;
        mono.resize((size_t)out);
        uint8_t *dst[1] = { (uint8_t*)&mono[0] };
        const int got = swr_convert(swr,dst,out,(const uint8_t**)f->extended_data,f->nb_samples);
        if (got > 0)
            spec->add(&mono[0],(size_t)got,c.audio);
        return true;
    };

    // everything the decoder has ready
    auto drain = [&](AVCodecContext *dec,bool video) -> bool {
        for (;;) {
            r = avcodec_receive_frame(dec,frame);
            if (r == AVERROR(EAGAIN) || r == AVERROR_EOF)
                return true;
            if (r < 0) {
                if (errors++ < 10u) fprintf(stderr,"%s: decoding error: %s\n",c.path.c_str(),averr(r).c_str());
                return true;
            }
            const bool keep = video ? on_video(frame) : on_audio(frame);
            av_frame_unref(frame);
            if (!keep)
                return false;
        }
    };

    auto send = [&](AVCodecContext *dec,const AVPacket *p,bool video) -> bool {
        for (;;) {
            r = avcodec_send_packet(dec,p);
            if (r == AVERROR(EAGAIN)) {
                if (!drain(dec,video))
                    return false;
                continue;
            }
            if (r < 0 && r != AVERROR_EOF && errors++ < 10u)
                fprintf(stderr,"%s: decoding error: %s\n",c.path.c_str(),averr(r).c_str());
            return drain(dec,video);
        }
    };

    if (pkt == NULL || frame == NULL)
        goto done;
    if ((r=avformat_open_input(&fmt,c.path.c_str(),NULL,NULL)) < 0) {
        fprintf(stderr,"%s: %s\n",c.path.c_str(),averr(r).c_str());
        goto done;
    }
    if ((r=avformat_find_stream_info(fmt,NULL)) < 0) {
        fprintf(stderr,"%s: cannot read the streams: %s\n",c.path.c_str(),averr(r).c_str());
        goto done;
    }
    if ((vindex=av_find_best_stream(fmt,AVMEDIA_TYPE_VIDEO,-1,-1,NULL,0)) < 0) {
        fprintf(stderr,"%s: no video stream\n",c.path.c_str());
        goto done;
    }
    aindex = av_find_best_stream(fmt,AVMEDIA_TYPE_AUDIO,-1,vindex,NULL,0);
    for (unsigned int i=0;i < fmt->nb_streams;i++) {
        if ((int)i != vindex && (int)i != aindex)
            fmt->streams[i]->discard = AVDISCARD_ALL;
    }

    {
        const AVStream *vs = fmt->streams[vindex];
        const AVRational fr = standard_rate(av_guess_frame_rate(fmt,(AVStream*)vs,NULL));
        const AVRational sar = av_guess_sample_aspect_ratio(fmt,(AVStream*)vs,NULL);
        c.width = vs->codecpar->width;
        c.height = vs->codecpar->height;
        c.rate_num = fr.num > 0 ? fr.num : 60000;
        c.rate_den = fr.num > 0 ? fr.den : 1001;
        c.sar_num = sar.num > 0 ? sar.num : 1;
        c.sar_den = sar.num > 0 ? sar.den : 1;
        step = (double)c.rate_den / (double)c.rate_num;
        t0 = fmt->start_time != AV_NOPTS_VALUE ? fmt->start_time : 0;
        if (vs->nb_frames > 0)
            prog.total = vs->nb_frames;
        else if (fmt->duration > 0)
            prog.total = (int64_t)((double)fmt->duration / AV_TIME_BASE / step);
    }
    if (!open_decoder(vindex,vdec))
        goto done;
    if (aindex >= 0) {
        if (open_decoder(aindex,adec)) {
            c.audio_rate = adec->sample_rate;
            c.audio_channels = adec->ch_layout.nb_channels;
            spec = new spectrum_t(step);
        }
        else {
            fprintf(stderr,"%s: its audio cannot be read, and is not used\n",c.path.c_str());
            avcodec_free_context(&adec);
            aindex = -1;
        }
    }

    ok = true;
    while (ok) {
        if (interrupted) {
            ok = false;
            break;
        }
        if ((r=av_read_frame(fmt,pkt)) < 0) {
            if (r != AVERROR_EOF)
                fprintf(stderr,"%s: error reading it, stopping there: %s\n",c.path.c_str(),averr(r).c_str());
            break;
        }
        if (pkt->stream_index == vindex)
            ok = send(vdec,pkt,true);
        else if (pkt->stream_index == aindex)
            ok = send(adec,pkt,false);
        av_packet_unref(pkt);
    }
    if (ok) {
        ok = send(vdec,NULL,true);
        if (ok && adec != NULL)
            ok = send(adec,NULL,false);
    }
    if (ok && spec != NULL) {
        // what the resampler still has, and the slices to the end of the video
        if (swr != NULL) {
            mono.resize(4096);
            uint8_t *dst[1] = { (uint8_t*)&mono[0] };
            int got;
            while ((got=swr_convert(swr,dst,4096,NULL,0)) > 0)
                spec->add(&mono[0],(size_t)got,c.audio);
        }
        spec->finish(c.audio);
        const size_t want = c.frames() != 0u ? (size_t)llround(c.thumbs.time.back() / step) + 1u : 0u;
        c.audio.resize(std::min(c.audio.size(),want * AUDIO_BANDS));
    }

    // the frames at the ends, which do not have two either side, are as noisy as the nearest that does
    if (ok) {
        const size_t n = c.frames();
        c.cells = cell_area_t(c.width,c.height).cells();
        float last = 0;
        for (size_t i=0;i < n;i++) {
            if (c.noise[i] >= 0) { last = c.noise[i]; break; }
        }
        for (size_t i=0;i < n;i++) {
            if (c.noise[i] < 0)
                c.noise[i] = last;
            else
                last = c.noise[i];
        }
        if (n == 0) {
            fprintf(stderr,"%s: no frames of video could be read\n",c.path.c_str());
            ok = false;
        }
    }

done:
    delete spec;
    sws_freeContext(sws);
    swr_free(&swr);
    avcodec_free_context(&vdec);
    avcodec_free_context(&adec);
    avformat_close_input(&fmt);
    av_packet_free(&pkt);
    av_frame_free(&frame);
    prog.done = true;
    return ok;
}

/////////////////////////////////////////////////////////////////////////////
// the cache

// where what is read of a file is kept, so that it is read only once: the cache directory, and a name
// from where the file is, how big it is and when it was changed, or "" if it is not kept
static std::string cache_path(const std::string &dir,const std::string &path) {
    struct stat st;
    char *real;

    if (dir.empty() || stat(path.c_str(),&st) != 0 || (real=realpath(path.c_str(),NULL)) == NULL)
        return "";

    char key[64];
    std::string s = real;
    free(real);
    snprintf(key,sizeof(key),"|%lld|%lld|mcm2",(long long)st.st_size,(long long)st.st_mtime);
    s += key;

    // FNV-1a
    uint64_t h = 14695981039346656037ull;
    for (unsigned char ch : s) {
        h ^= ch;
        h *= 1099511628211ull;
    }
    snprintf(key,sizeof(key),"%016llx.mcm",(unsigned long long)h);
    return dir + "/" + key;
}

static const char CACHE_MAGIC[8] = { 'M','C','M','I','D','X','0','1' };

struct cache_head_t {
    int32_t                     width,height,sar_num,sar_den,rate_num,rate_den,audio_rate,audio_channels;
    uint32_t                    cells,reserved;
    uint64_t                    frames,audio_bytes;
};

static bool load_cache(capture_t &c,const std::string &cpath) {
    FILE *fp = fopen(cpath.c_str(),"rb");
    char magic[8];
    cache_head_t h;
    bool ok = false;

    if (fp == NULL)
        return false;
    if (fread(magic,8,1,fp) == 1 && memcmp(magic,CACHE_MAGIC,8) == 0 && fread(&h,sizeof(h),1,fp) == 1 &&
        h.frames != 0 && h.frames < (1ull << 32) && h.audio_bytes < (1ull << 36)) {
        const size_t n = (size_t)h.frames;
        c.width = h.width; c.height = h.height;
        c.sar_num = h.sar_num; c.sar_den = h.sar_den;
        c.rate_num = h.rate_num; c.rate_den = h.rate_den;
        c.audio_rate = h.audio_rate; c.audio_channels = h.audio_channels;
        c.cells = h.cells;
        c.thumbs.time.resize(n);
        c.thumbs.v.resize(n * THUMB_SIZE);
        c.damage.resize(n);
        c.noise.resize(n);
        c.audio.resize((size_t)h.audio_bytes);
        ok = fread(&c.thumbs.time[0],sizeof(double),n,fp) == n &&
             fread(&c.thumbs.v[0],THUMB_SIZE,n,fp) == n &&
             fread(&c.damage[0],sizeof(uint16_t),n,fp) == n &&
             fread(&c.noise[0],sizeof(float),n,fp) == n &&
             (h.audio_bytes == 0 || fread(&c.audio[0],(size_t)h.audio_bytes,1,fp) == 1);
    }
    fclose(fp);
    return ok;
}

// make each directory of the path that is not there
static void make_dirs(const std::string &dir) {
    for (size_t i=1;i <= dir.size();i++) {
        if (i == dir.size() || dir[i] == '/')
            mkdir(dir.substr(0,i).c_str(),0755);
    }
}

static void save_cache(const capture_t &c,const std::string &cpath) {
    const size_t slash = cpath.rfind('/');
    if (slash != std::string::npos)
        make_dirs(cpath.substr(0,slash));

    const std::string tmp = cpath + ".tmp";
    FILE *fp = fopen(tmp.c_str(),"wb");
    if (fp == NULL)
        return;

    cache_head_t h;
    memset(&h,0,sizeof(h));
    h.width = c.width; h.height = c.height;
    h.sar_num = c.sar_num; h.sar_den = c.sar_den;
    h.rate_num = c.rate_num; h.rate_den = c.rate_den;
    h.audio_rate = c.audio_rate; h.audio_channels = c.audio_channels;
    h.cells = c.cells;
    h.frames = c.frames();
    h.audio_bytes = c.audio.size();

    const size_t n = c.frames();
    bool ok = fwrite(CACHE_MAGIC,8,1,fp) == 1 && fwrite(&h,sizeof(h),1,fp) == 1 &&
              fwrite(&c.thumbs.time[0],sizeof(double),n,fp) == n &&
              fwrite(&c.thumbs.v[0],THUMB_SIZE,n,fp) == n &&
              fwrite(&c.damage[0],sizeof(uint16_t),n,fp) == n &&
              fwrite(&c.noise[0],sizeof(float),n,fp) == n &&
              (c.audio.empty() || fwrite(&c.audio[0],c.audio.size(),1,fp) == 1);
    if (fclose(fp) != 0)
        ok = false;
    if (ok)
        rename(tmp.c_str(),cpath.c_str());
    else
        unlink(tmp.c_str());
}

/////////////////////////////////////////////////////////////////////////////

bool index_captures(std::vector<capture_t> &caps,const std::string &cache_dir,unsigned int jobs,bool quiet) {
    std::vector<size_t> todo;

    for (size_t i=0;i < caps.size();i++) {
        const std::string cp = cache_path(cache_dir,caps[i].path);
        if (cp.empty() || !load_cache(caps[i],cp))
            todo.push_back(i);
    }
    if (todo.empty())
        return true;

    // each file is read by a thread of its own, and its decoder has its share of the CPUs
    const unsigned int cpus = std::max(1u,std::thread::hardware_concurrency());
    jobs = std::max(1u,std::min(jobs,(unsigned int)todo.size()));
    const int threads = (int)std::max(1u,cpus / jobs);

    std::vector<progress_t> prog(todo.size());
    std::vector<char> good(todo.size(),0);
    std::atomic<size_t> next{0};
    std::vector<std::thread> workers;

    for (unsigned int j=0;j < jobs;j++) {
        workers.emplace_back([&]() {
            for (;;) {
                const size_t k = next++;
                if (k >= todo.size())
                    break;
                if (interrupted) {
                    prog[k].done = true;
                    continue;
                }
                capture_t &c = caps[todo[k]];
                good[k] = read_capture(c,prog[k],threads) ? 1 : 0;
                if (good[k]) {
                    const std::string cp = cache_path(cache_dir,c.path);
                    if (!cp.empty())
                        save_cache(c,cp);
                }
            }
        });
    }

    // how far along they all are, together
    double last = 0;
    for (;;) {
        bool all = true;
        unsigned long got = 0;
        int64_t total = 0;
        for (const progress_t &p : prog) {
            all = all && p.done;
            got += p.frames;
            total += p.total;
        }
        if (!quiet && (all || now_seconds() - last >= 0.5)) {
            last = now_seconds();
            if (total > 0)
                fprintf(stderr,"\rReading %zu file%s: frame %lu of about %lld (%.1f%%)  ",todo.size(),todo.size() == 1 ? "" : "s",
                    got,(long long)total,std::min(100.0,(100.0 * (double)got) / (double)total));
            else
                fprintf(stderr,"\rReading %zu file%s: frame %lu  ",todo.size(),todo.size() == 1 ? "" : "s",got);
        }
        if (all)
            break;
        usleep(100000);
    }
    if (!quiet)
        fprintf(stderr,"\n");
    for (std::thread &t : workers)
        t.join();

    if (interrupted) {
        fprintf(stderr,"Stopped by CTRL+C while reading the files, nothing written\n");
        return false;
    }
    for (size_t k=0;k < todo.size();k++) {
        if (!good[k])
            return false;
    }
    return true;
}
