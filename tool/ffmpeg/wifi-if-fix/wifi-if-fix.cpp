/* wifi-if-fix: take the thumping of a wireless router out of audio captured from tape.
 *
 * A VCR or a cassette deck that is too close to a wireless router picks up the beacon the router
 * sends every 102.4 ms (100 TU), and it is heard under the audio as a quiet thump about ten times
 * a second. Each beacon is a burst of radio a few milliseconds long, which the audio circuits turn
 * into a pulse of about the same shape every time: the shape that the coupling and filtering of
 * the audio give a short burst. So it can be taken out by subtracting that shape where each pulse is.
 *
 * The input file is read twice. The first pass finds the pulses: it looks for a train of them that
 * comes at about the period of a beacon, then follows the train pulse by pulse, since the period
 * wanders a little with the speed of the tape, and a beacon is sent a little late when the router
 * has to wait for the channel to be clear. Under loud audio, where a pulse cannot be found by itself,
 * the train is followed by the pulses around it matched together, and the pulses are put where the
 * train is. How much of each pulse is taken out goes down with how sure it is that the train is
 * there and where the pulse is, so that where that is not known, as under very loud audio, nothing
 * is taken out rather than something that is not there. The second pass works out the shape of each
 * pulse from the pulses around it, fits that to the pulse, subtracts it, and writes the audio to a
 * WAV file. Samples that are not near a pulse are written exactly as they were read. */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include <algorithm>
#include <deque>
#include <functional>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
}

#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(57,24,100)
#error FFmpeg 5.1 or later is needed, for AVChannelLayout
#endif

// how much of the audio is looked at at a time to find a train of pulses, and how often
static const double WINDOW_SECONDS = 8.0;
static const double HOP_SECONDS = 2.0;
// how much audio the first pass keeps behind where it has read up to, to follow a train
// back from where it was found to where it starts
static const double KEEP_SECONDS = 16.0;
// the rate the audio is brought down to, to look for a train of pulses: the thump is mostly below 300 Hz
static const double SEARCH_RATE = 4000.0;
// the harmonics of a train that are looked at to find it
static const double COMB_LO_HZ = 15.0;
static const double COMB_HI_HZ = 250.0;
// each harmonic is compared to the average of the spectrum this far each side of it
static const double COMB_BASE_HZ = 2.0;
// how many of the best periods from the harmonics are tried
static const unsigned int COMB_CANDIDATES = 3;
// most of a pulse is in this long a part of it
static const double CORE_MS = 20.0;
// A train of pulses has at least this much of what repeats each period in CORE_MS of it.
// Hum, and the buzz of the vertical sync of a VCR, repeat every 100 ms too, but they
// repeat several times within that, so they are spread over the whole period.
static const double MIN_CONCENTRATION = 0.5;
// where a pulse starts in the average of a train of them: see start_run()
static const double ONSET_FRACTION = 0.05;
static const double ONSET_SMOOTH_MS = 2.0;
// audio before the start of a pulse that is taken out with it
static const double PRE_MS = 2.0;
// how much of a pulse is matched to find where it is
static const double MATCH_MS = 20.0;
// A pulse is looked for this far each way from where the train says it is. It can be found
// further than that, as far as -j, only if it matches that much better there.
static const double NEAR_MS = 0.75;
// How well a pulse has to match the pulses before it to be taken as found where it matches, how many
// standard errors of its size above 0 it has to be, and how much bigger than them it can be: what
// matches under loud audio that is much bigger than the pulses is the audio.
static const double MIN_SCORE = 0.4;
static const double FOUND_Z = 5.0;
static const double FOUND_MAX_GAIN = 3.0;
static const double FAR_SCORE = 0.6;
static const double FAR_BETTER = 0.2;
// how well it has to match for the shape that later pulses are matched to to be changed by it, and by how much
static const double LEARN_SCORE = 0.6;
static const double LEARN_Z = 10.0;
static const double LEARN_RATE = 1.0 / 32.0;
// how many of the pulses found the period and phase of the train are worked out from
static const size_t LINE_POINTS = 24;
// How sure it is that each pulse is there: the train is either there, with pulses about as big as
// the pulses before them, or not, with pulses of size 0, and goes from one to the other between two
// pulses with the chance SWITCH_CHANCE. The size measured of each pulse is taken to be no more certain
// than MIN_GAIN_SD, as the shape it is measured with is not exactly its shape.
static const double SWITCH_CHANCE = 0.002;
static const double MIN_GAIN_SD = 0.01;
// How much the size measured of a pulse varies with the rest of the audio is also measured this many
// periods away from each pulse, where there is no pulse. The fit assumes that what is left is noise,
// but music has much of what it has at the low frequencies of the pulse, which makes the size measured
// vary more than the fit says.
static const double OFF_PERIODS[4] = { -1.5,-0.5,0.5,1.5 };
// A pulse too small to be found under the audio is looked for together with the GROUP_PULSES before it:
// how well each matched the shape at each lag, lined up to where the train says each is, added up. The
// train is where they match best together, if that is GROUP_Z standard errors above 0. That is put
// in what the period and phase of the train are worked out from, as a pulse found with GROUP_WEIGHT.
static const size_t GROUP_PULSES = 16;
static const double GROUP_Z = 5.0;
static const double GROUP_WEIGHT = 0.25;
// a train is lost when the chance it is there, from the pulses so far, has been less than LOST_CHANCE for LOST_PULSES
static const double LOST_CHANCE = 0.05;
static const size_t LOST_PULSES = 16;
// going back from where a train was found, the chance it is there starts from this many pulses after
static const size_t PRESENCE_START = 32;
// A pulse not found is put on a line through PLACE_POINTS pulses each side of it that were found, by
// themselves or together. That is off by about PLACE_JITTER_MS, as each beacon is a little early or late,
// and by PLACE_BETWEEN_MS more for each pulse it is from the nearest pulse found by itself, or
// PLACE_BEYOND_MS if there are pulses found by themselves only on one side, and pulses more than
// PLACE_BLIND from any pulse found by itself are not taken out at all.
// These are from captures of a router that was heard as the tape played, with the capture clock
// following the tape. PLACE_MAX_MS is as far off as the shape is looked at for how much good it does.
static const size_t PLACE_POINTS = 8;
static const double PLACE_JITTER_MS = 0.3;
static const double PLACE_BETWEEN_MS = 0.015;
static const double PLACE_BEYOND_MS = 0.05;
static const double PLACE_MAX_MS = 10.0;
static const int32_t PLACE_BLIND = 32;
// pulses less sure than this to be there are left out at the start and end of a train, and not taken out
static const double MIN_WEIGHT = 0.01;
// How much the size of one pulse varies from the pulses around it. A size that is measured
// with less certainty than this, under loud audio, is pulled toward the size of the pulses around it.
static const double GAIN_SPREAD = 0.35;
static const double MAX_GAIN = 3.0;
// The shape of a pulse is made from the pulses around it that are surely there, weighted by how quiet
// the audio under each is, but no quieter than NOISE_FLOOR.
static const double SHAPE_MIN_WEIGHT = 0.5;
static const double NOISE_FLOOR = 1e-4;
// the end of the shape of a pulse is faded out over this long
static const double TAIL_FADE_MS = 5.0;
// the windowed sinc that moves audio by a fraction of a sample
static const int SINC_TAPS = 16;
static const int SINC_PHASES = 1024;

enum sample_out_t {
    OUT_DEFAULT = 0,
    OUT_S16,
    OUT_S24,
    OUT_S32,
    OUT_F32
};

struct options_t {
    std::string                 input;
    std::string                 output;
    std::string                 log_path;
    double                      period_ms = 102.4;  // beacon period
    double                      range = 1.0;        // percent each way from period_ms to look for a train
    double                      jitter_ms = 2.0;    // how far from where the train says a pulse can be
    double                      length_ms = 50.0;   // how long a pulse lasts, from where it starts
    unsigned int                neighbors = 32;     // pulses each side averaged for the shape of a pulse
    double                      threshold = 8.0;    // how far above the rest of the audio a train has to be to be found
    int                         stream = -1;        // stream of the input file, -1 for the best audio stream
    sample_out_t                format = OUT_DEFAULT;
    bool                        measure_only = false;
    bool                        overwrite = false;
    bool                        quiet = false;
};

// a pulse found by the first pass
struct pulse_t {
    double                      t = 0;              // file position where it starts, in samples, to a fraction of a sample
    int32_t                     k = 0;              // which pulse of its train, counting from where the train was found
    uint32_t                    run = 0;            // which train
    float                       score = 0;          // how well it matched the pulses before it, -1 to 1
    float                       gain = 0;           // how big it is, relative to the pulses before it
    float                       gain_sd = 1;        // standard error of that
    float                       noise = 0;          // RMS of the rest of the audio under it, -1 to 1
    float                       offset = 0;         // how far from where the train says it should be, in samples
    float                       sd_scale = 1;       // how much more uncertain the size is than the fit says, from the audio between pulses
    float                       weight = 0;         // how much of it to take out, 0 to 1
    bool                        found = false;      // found where it is, rather than placed where the train says
    bool                        grouped = false;    // not found, but placed where the pulses around it were found together
};

// a train of pulses found by the first pass
struct run_t {
    size_t                      first = 0;          // its pulses
    size_t                      count = 0;
    double                      period = 0;         // samples
    double                      level = 0;          // the biggest the pulses get, -1 to 1
};

static void help(void) {
    fprintf(stderr,"wifi-if-fix [options] <input> <output.wav>\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"Takes out the quiet thumping that a wireless router near a VCR or a tape deck puts\n");
    fprintf(stderr,"into the audio, from the beacon it sends about ten times a second. The input can be\n");
    fprintf(stderr,"any file FFmpeg can read the audio of, and the output is a WAV file. Audio that is not\n");
    fprintf(stderr,"near a pulse is written exactly as it was read.\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"  -p <ms>        Beacon period (default 102.4, which is 100 TU, what most routers send)\n");
    fprintf(stderr,"  -r <percent>   How far the period of the pulses in the audio can be from -p (default 1).\n");
    fprintf(stderr,"                 The router is heard as the tape is played, so the period is that of the\n");
    fprintf(stderr,"                 router as the capture clock counts it, but if it was heard as the tape\n");
    fprintf(stderr,"                 was recorded, and the tape plays faster or slower, it can be further off.\n");
    fprintf(stderr,"                 Too wide finds music with a beat at about the period.\n");
    fprintf(stderr,"  -j <ms>        How far a pulse can be from where the pulses before it say it should\n");
    fprintf(stderr,"                 be, for a beacon sent late when the router waits for the channel (default 2)\n");
    fprintf(stderr,"  -L <ms>        How long a pulse lasts, with the ringing after it (default 50)\n");
    fprintf(stderr,"  -k <pulses>    How many pulses each side of a pulse are averaged for its shape (default 32)\n");
    fprintf(stderr,"  -t <snr>       How far above the rest of the audio a train of pulses has to be to be\n");
    fprintf(stderr,"                 found, as a ratio of energy (default 8). Higher finds fewer.\n");
    fprintf(stderr,"  -a <stream>    Which stream of the input file to read (default: the best audio stream)\n");
    fprintf(stderr,"  -f <format>    Output samples: s16, s24, s32 or f32 (default: the same as the input)\n");
    fprintf(stderr,"  -y             Write over the output file if it exists\n");
    fprintf(stderr,"  -n             Find the pulses only, write no output file\n");
    fprintf(stderr,"  -log <file>    Write each pulse found to a CSV file\n");
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

// h:mm:ss.sss
static std::string time_str(double s) {
    char buf[64];

    if (!(s >= 0)) s = 0;
    const long long ms = llrint(s * 1000.0);
    snprintf(buf,sizeof(buf),"%lld:%02lld:%02lld.%03lld",ms / 3600000ll,(ms / 60000ll) % 60ll,(ms / 1000ll) % 60ll,ms % 1000ll);
    return buf;
}

static double dbfs(double v) {
    return v > 0 ? 20.0 * log10(v) : -999.0;
}

static double clampd(double v,double lo,double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/////////////////////////////////////////////////////////////////////////////
// arithmetic

// in place FFT of re and im, whose size is a power of 2
static void fft(std::vector<double> &re,std::vector<double> &im) {
    const size_t n = re.size();

    for (size_t i=1,j=0;i < n;i++) {
        size_t bit = n >> 1;
        for (;j & bit;bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            std::swap(re[i],re[j]);
            std::swap(im[i],im[j]);
        }
    }

    for (size_t len=2;len <= n;len <<= 1) {
        const double a = -2.0 * M_PI / (double)len;
        const size_t half = len >> 1;

        for (size_t k=0;k < half;k++) {
            const double wr = cos(a * (double)k),wi = sin(a * (double)k);

            for (size_t i=k;i < n;i += len) {
                const size_t j = i + half;
                const double tr = re[j] * wr - im[j] * wi;
                const double ti = re[j] * wi + im[j] * wr;
                re[j] = re[i] - tr;
                im[j] = im[i] - ti;
                re[i] += tr;
                im[i] += ti;
            }
        }
    }
}

// solve the 3x3 system m x = r, false if it is singular. inv00 is (m^-1)[0][0].
static bool solve3(const double m[3][3],const double r[3],double x[3],double &inv00) {
    const double c00 = m[1][1]*m[2][2] - m[1][2]*m[2][1];
    const double c01 = m[1][2]*m[2][0] - m[1][0]*m[2][2];
    const double c02 = m[1][0]*m[2][1] - m[1][1]*m[2][0];
    const double det = m[0][0]*c00 + m[0][1]*c01 + m[0][2]*c02;

    if (!(fabs(det) > 1e-300))
        return false;

    const double c10 = m[0][2]*m[2][1] - m[0][1]*m[2][2];
    const double c11 = m[0][0]*m[2][2] - m[0][2]*m[2][0];
    const double c12 = m[0][1]*m[2][0] - m[0][0]*m[2][1];
    const double c20 = m[0][1]*m[1][2] - m[0][2]*m[1][1];
    const double c21 = m[0][2]*m[1][0] - m[0][0]*m[1][2];
    const double c22 = m[0][0]*m[1][1] - m[0][1]*m[1][0];

    // m is symmetric, so the inverse is the cofactors over the determinant
    x[0] = (c00*r[0] + c10*r[1] + c20*r[2]) / det;
    x[1] = (c01*r[0] + c11*r[1] + c21*r[2]) / det;
    x[2] = (c02*r[0] + c12*r[1] + c22*r[2]) / det;
    inv00 = c00 / det;
    return true;
}

// How well x fits g*s plus a straight line, over n samples: the least squares fit.
struct fit_t {
    double                      g = 0;              // how much of s
    double                      c0 = 0;             // the line, at the middle
    double                      c1 = 0;             // and its slope, per half of n
    double                      noise = 0;          // RMS of what is left
    double                      g_sd = 1;           // standard error of g
    bool                        ok = false;
};

template <typename X> static fit_t fit_shape(const X *x,const double *s,size_t n) {
    double m[3][3] = {{0,0,0},{0,0,0},{0,0,0}},r[3] = {0,0,0},sol[3],inv00;
    const double mid = (double)(n - 1) / 2.0,half = std::max(1.0,(double)n / 2.0);
    double xx = 0;
    fit_t f;

    for (size_t i=0;i < n;i++) {
        const double u = ((double)i - mid) / half,v = (double)x[i],w = s[i];
        m[0][0] += w*w; m[0][1] += w; m[0][2] += w*u;
        m[1][1] += 1.0; m[1][2] += u; m[2][2] += u*u;
        r[0] += w*v; r[1] += v; r[2] += u*v;
        xx += v*v;
    }
    m[1][0] = m[0][1]; m[2][0] = m[0][2]; m[2][1] = m[1][2];

    if (n < 8 || !solve3(m,r,sol,inv00))
        return f;

    // what is left: x.x - sol.r, for a least squares fit
    const double res = std::max(0.0,xx - (sol[0]*r[0] + sol[1]*r[1] + sol[2]*r[2]));
    f.g = sol[0];
    f.c0 = sol[1];
    f.c1 = sol[2];
    f.noise = sqrt(res / (double)(n - 3));
    f.g_sd = std::max(1e-6,f.noise * sqrt(std::max(0.0,inv00)));
    f.ok = true;
    return f;
}

// a second order high pass filter
class highpass_t {
public:
    highpass_t(double fc,double sr) {
        const double w = 2.0 * M_PI * fc / sr,cw = cos(w),al = sin(w) / (2.0 * M_SQRT1_2),a0 = 1.0 + al;
        b0 = (1.0 + cw) / 2.0 / a0;
        b1 = -(1.0 + cw) / a0;
        b2 = b0;
        a1 = -2.0 * cw / a0;
        a2 = (1.0 - al) / a0;
    }

    double run(double x) {
        const double y = b0*x + z1;
        z1 = b1*x - a1*y + z2;
        z2 = b2*x - a2*y;
        return y;
    }
private:
    double                      b0,b1,b2,a1,a2;
    double                      z1 = 0,z2 = 0;
};

// Kaiser windowed sinc filters that move audio by a fraction of a sample: for each of SINC_PHASES
// fractions f from 0 to 1, the SINC_TAPS weights of the samples from -SINC_TAPS/2+1 to SINC_TAPS/2
// that make the value f past sample 0
class sinc_t {
public:
    sinc_t() {
        const double beta = 8.0;
        const int half = SINC_TAPS / 2;

        table.resize((size_t)SINC_PHASES * SINC_TAPS);
        for (int p=0;p < SINC_PHASES;p++) {
            const double f = (double)p / SINC_PHASES;
            double *w = &table[(size_t)p * SINC_TAPS],sum = 0;

            for (int j=0;j < SINC_TAPS;j++) {
                const double d = (double)(j - half + 1) - f;
                const double x = d / (double)half;
                const double sinc = fabs(d) < 1e-12 ? 1.0 : sin(M_PI * d) / (M_PI * d);
                const double win = fabs(x) >= 1.0 ? 0.0 : bessel_i0(beta * sqrt(1.0 - x*x)) / bessel_i0(beta);
                w[j] = sinc * win;
                sum += w[j];
            }
            for (int j=0;j < SINC_TAPS;j++)
                w[j] /= sum;
        }
    }

    // the weights for fraction f, 0 <= f < 1
    const double *at(double f) const {
        int p = (int)lrint(f * SINC_PHASES);
        if (p < 0) p = 0;
        if (p >= SINC_PHASES) p = SINC_PHASES - 1;
        return &table[(size_t)p * SINC_TAPS];
    }
private:
    static double bessel_i0(double x) {
        double sum = 1,term = 1;
        for (int k=1;k < 50;k++) {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
            if (term < sum * 1e-17)
                break;
        }
        return sum;
    }

    std::vector<double>         table;
};

static const sinc_t &sinc(void) {
    static const sinc_t s;
    return s;
}

/////////////////////////////////////////////////////////////////////////////
// reading and decoding the input file

class input_t {
public:
    AVFormatContext*            fmt = NULL;
    AVCodecContext*             dec = NULL;
    int                         aindex = -1;

    ~input_t() {
        close();
    }

    void close(void) {
        avcodec_free_context(&dec);
        avformat_close_input(&fmt);
        aindex = -1;
    }

    bool open(const std::string &path,int stream) {
        int r;

        if ((r=avformat_open_input(&fmt,path.c_str(),NULL,NULL)) < 0) {
            fprintf(stderr,"%s: %s\n",path.c_str(),averr(r).c_str());
            return false;
        }
        if ((r=avformat_find_stream_info(fmt,NULL)) < 0) {
            fprintf(stderr,"%s: cannot read the streams: %s\n",path.c_str(),averr(r).c_str());
            return false;
        }
        if ((aindex=av_find_best_stream(fmt,AVMEDIA_TYPE_AUDIO,stream,-1,NULL,0)) < 0) {
            if (stream >= 0)
                fprintf(stderr,"%s: stream %d is not audio, or there is no stream %d\n",path.c_str(),stream,stream);
            else
                fprintf(stderr,"%s: no audio stream\n",path.c_str());
            return false;
        }

        const AVStream *st = fmt->streams[aindex];
        const AVCodec *codec = avcodec_find_decoder(st->codecpar->codec_id);
        if (codec == NULL) {
            fprintf(stderr,"%s: no decoder for %s audio\n",path.c_str(),avcodec_get_name(st->codecpar->codec_id));
            return false;
        }

        if ((dec=avcodec_alloc_context3(codec)) == NULL)
            return false;
        if ((r=avcodec_parameters_to_context(dec,st->codecpar)) < 0) {
            fprintf(stderr,"%s: %s\n",path.c_str(),averr(r).c_str());
            return false;
        }
        dec->pkt_timebase = st->time_base;
        if ((r=avcodec_open2(dec,codec,NULL)) < 0) {
            fprintf(stderr,"%s: cannot open the %s decoder: %s\n",path.c_str(),codec->name,averr(r).c_str());
            return false;
        }
        if (dec->sample_rate <= 0 || dec->ch_layout.nb_channels <= 0) {
            fprintf(stderr,"%s: the sample rate or the number of channels is not given\n",path.c_str());
            return false;
        }

        for (unsigned int i=0;i < fmt->nb_streams;i++) {
            if ((int)i != aindex)
                fmt->streams[i]->discard = AVDISCARD_ALL;
        }

        return true;
    }

    // seconds, or 0 if not known
    double duration(void) const {
        const AVStream *st = fmt->streams[aindex];

        if (st->duration > 0 && st->time_base.den > 0)
            return (double)st->duration * av_q2d(st->time_base);
        if (fmt->duration > 0)
            return (double)fmt->duration / AV_TIME_BASE;

        return 0;
    }

    // read the whole file, and give each frame of audio decoded to on_frame, which can stop it by returning false
    bool run(const std::function<bool(AVFrame*)> &on_frame) {
        AVPacket *pkt = av_packet_alloc();
        AVFrame *frame = av_frame_alloc();
        unsigned int errors = 0;
        bool ok = (pkt != NULL && frame != NULL);
        int r;

        // give on_frame each frame the decoder has ready
        auto drain = [&](void) -> bool {
            for (;;) {
                r = avcodec_receive_frame(dec,frame);
                if (r == AVERROR(EAGAIN) || r == AVERROR_EOF)
                    return true;
                if (r < 0) {
                    if (errors++ < 10u) fprintf(stderr,"\nAudio decoding error: %s\n",averr(r).c_str());
                    return true;
                }

                const bool keep = on_frame(frame);
                av_frame_unref(frame);
                if (!keep)
                    return false;
            }
        };

        while (ok) {
            if ((r=av_read_frame(fmt,pkt)) < 0) {
                if (r != AVERROR_EOF)
                    fprintf(stderr,"\nError reading the input, stopping there: %s\n",averr(r).c_str());
                break;
            }

            if (pkt->stream_index == aindex) {
                for (;;) {
                    r = avcodec_send_packet(dec,pkt);
                    if (r == AVERROR(EAGAIN)) {
                        if (!(ok=drain())) break;
                        continue;
                    }
                    if (r < 0 && errors++ < 10u)
                        fprintf(stderr,"\nAudio decoding error: %s\n",averr(r).c_str());
                    break;
                }
                if (ok)
                    ok = drain();
            }

            av_packet_unref(pkt);
        }

        // and the frames the decoder still has
        if (ok) {
            avcodec_send_packet(dec,NULL);
            ok = drain();
        }

        av_packet_free(&pkt);
        av_frame_free(&frame);
        return ok;
    }
};

template <typename S> static void deinterleave(const AVFrame *f,bool planar,unsigned int nch,double scale,double offset,
    std::vector<std::vector<double>> &out) {
    const size_t n = (size_t)f->nb_samples;

    for (unsigned int c=0;c < nch;c++) {
        std::vector<double> &o = out[c];
        const size_t at = o.size();
        const S *p = planar ? (const S*)f->extended_data[c] : ((const S*)f->extended_data[0] + c);
        const size_t step = planar ? 1u : nch;

        o.resize(at + n);
        for (size_t i=0;i < n;i++)
            o[at + i] = ((double)p[i * step] - offset) * scale;
    }
}

// append the samples of a decoded frame to out, one vector for each channel, from -1 to 1
static bool frame_samples(const AVFrame *f,std::vector<std::vector<double>> &out) {
    const unsigned int nch = (unsigned int)f->ch_layout.nb_channels;
    const enum AVSampleFormat sf = (enum AVSampleFormat)f->format;
    const bool planar = !!av_sample_fmt_is_planar(sf);

    if (nch != out.size()) {
        fprintf(stderr,"\nThe number of channels changed from %u to %u\n",(unsigned int)out.size(),nch);
        return false;
    }

    switch (av_get_packed_sample_fmt(sf)) {
        case AV_SAMPLE_FMT_U8:  deinterleave<uint8_t>(f,planar,nch,1.0 / 128.0,128.0,out); break;
        case AV_SAMPLE_FMT_S16: deinterleave<int16_t>(f,planar,nch,1.0 / 32768.0,0.0,out); break;
        case AV_SAMPLE_FMT_S32: deinterleave<int32_t>(f,planar,nch,1.0 / 2147483648.0,0.0,out); break;
        case AV_SAMPLE_FMT_S64: deinterleave<int64_t>(f,planar,nch,1.0 / 9223372036854775808.0,0.0,out); break;
        case AV_SAMPLE_FMT_FLT: deinterleave<float>(f,planar,nch,1.0,0.0,out); break;
        case AV_SAMPLE_FMT_DBL: deinterleave<double>(f,planar,nch,1.0,0.0,out); break;
        default:
            fprintf(stderr,"\nThe decoder gives samples as %s, which is not supported\n",av_get_sample_fmt_name(sf));
            return false;
    }

    return true;
}

class progress_t {
public:
    progress_t(const char *what,double total,double rate,bool quiet) : what(what), total(total), rate(rate), quiet(quiet) { }

    void at(int64_t pos) {
        const double t = now_seconds();
        if (!quiet && (t - last) >= 0.5) {
            show(pos);
            last = t;
        }
    }

    void done(int64_t pos) {
        if (!quiet) {
            show(pos);
            fprintf(stderr,"\n");
        }
    }
private:
    void show(int64_t pos) {
        const double s = (double)pos / rate;

        if (total > 0)
            fprintf(stderr,"\r%s: %s of %s (%.1f%%)  ",what,time_str(s).c_str(),time_str(total).c_str(),std::min(100.0,s * 100.0 / total));
        else
            fprintf(stderr,"\r%s: %s  ",what,time_str(s).c_str());
    }

    const char*                 what;
    double                      total;
    double                      rate;
    bool                        quiet;
    double                      last = 0;
};

/////////////////////////////////////////////////////////////////////////////
// first pass: finding the pulses

// samples of the audio kept in memory for a while, by their position in the file
class history_t {
public:
    std::vector<float>          s;
    int64_t                     base = 0;           // file position of s[0]

    int64_t end(void) const {
        return base + (int64_t)s.size();
    }

    bool has(int64_t from,int64_t to) const {
        return from >= base && to <= end();
    }

    const float *at(int64_t pos) const {
        return &s[(size_t)(pos - base)];
    }

    // forget what is before pos, a lot at a time
    void forget(int64_t pos,size_t chunk) {
        if (pos - base >= (int64_t)chunk) {
            const size_t n = (size_t)std::min<int64_t>(pos - base,(int64_t)s.size());
            s.erase(s.begin(),s.begin() + (ptrdiff_t)n);
            base += (int64_t)n;
        }
    }
};

// what was found of a train of pulses in a window of audio
struct train_t {
    bool                        found = false;
    double                      period = 0;         // samples
    double                      phase = 0;          // file position where the part of a pulse most of it is in starts
    double                      snr = 0;
    double                      concentration = 0;
};

// fold x at period p (in samples) into the nb bins of prof: the average of x at each phase.
// Returns the energy of that, weighted by how many samples are in each bin.
static double fold(const double *x,size_t n,double p,unsigned int nb,std::vector<double> &prof,std::vector<unsigned int> &count) {
    const double scale = (double)nb / p;
    double ph = 0,e = 0;

    prof.assign(nb,0.0);
    count.assign(nb,0u);
    for (size_t i=0;i < n;i++) {
        unsigned int b = (unsigned int)(ph * scale);
        if (b >= nb) b = nb - 1u;
        prof[b] += x[i];
        count[b]++;
        ph += 1.0;
        if (ph >= p) ph -= p;
    }
    for (unsigned int b=0;b < nb;b++) {
        if (count[b] != 0) {
            prof[b] /= count[b];
            e += prof[b] * prof[b] * count[b];
        }
    }

    return e;
}

// Look for a train of pulses in n samples of x, which start at file position pos.
//
// It is looked for first by the harmonics of its period in the spectrum: the period with the strongest
// harmonics, by the median of them, since a train of short pulses has all of them. Then the audio is
// folded at that period, and at periods around it to find it more closely: a train of pulses is
// there if what repeats at that period stands out from the rest of the audio, and most of it is
// in one short part of the period, which is the pulse.
static train_t find_train(const float *x,size_t n,int64_t pos,double sr,const options_t &opt) {
    const unsigned int D = std::max(1u,(unsigned int)floor(sr / SEARCH_RATE));
    const double srd = sr / D;
    const size_t nd = n / D;
    train_t best;

    if (nd < 256)
        return best;

    // brought down to about SEARCH_RATE by averaging each D samples
    std::vector<double> xd(nd);
    double mean = 0,var = 0;
    for (size_t i=0;i < nd;i++) {
        double s = 0;
        for (unsigned int j=0;j < D;j++)
            s += x[i * D + j];
        xd[i] = s / D;
        mean += xd[i];
    }
    mean /= (double)nd;
    for (size_t i=0;i < nd;i++) {
        xd[i] -= mean;
        var += xd[i] * xd[i];
    }
    var /= (double)nd;
    if (!(var > 1e-16))
        return best;

    // the spectrum, against the average of the spectrum around each frequency
    size_t N = 1;
    while (N < nd) N <<= 1;
    std::vector<double> re(N,0.0),im(N,0.0);
    for (size_t i=0;i < nd;i++)
        re[i] = xd[i] * (0.5 - 0.5 * cos(2.0 * M_PI * ((double)i + 0.5) / (double)nd));
    fft(re,im);

    const size_t nb = N / 2 + 1;
    const double binw = srd / (double)N;
    std::vector<double> pw(nb),cs(nb + 1,0.0),pn(nb);
    for (size_t b=0;b < nb;b++) {
        pw[b] = re[b] * re[b] + im[b] * im[b];
        cs[b + 1] = cs[b] + pw[b];
    }
    const size_t hw = std::max<size_t>(3,(size_t)lrint(COMB_BASE_HZ / binw));
    for (size_t b=0;b < nb;b++) {
        const size_t lo = b > hw ? b - hw : 0,hi = std::min(nb,b + hw + 1);
        const double avg = (cs[hi] - cs[lo]) / (double)(hi - lo);
        pn[b] = avg > 0 ? pw[b] / avg : 0;
    }

    // the comb
    const double f0lo = 1000.0 / (opt.period_ms * (1.0 + opt.range / 100.0));
    const double f0hi = 1000.0 / (opt.period_ms * (1.0 - opt.range / 100.0));
    const double step = binw / (2.0 * (COMB_HI_HZ / f0lo));
    std::vector<std::pair<double,double>> comb;   // score, fundamental
    std::vector<double> h;

    for (double f0=f0lo;f0 <= f0hi;f0 += step) {
        h.clear();
        for (unsigned int k=std::max(1u,(unsigned int)ceil(COMB_LO_HZ / f0));k * f0 <= COMB_HI_HZ;k++) {
            const size_t b = (size_t)lrint(k * f0 / binw);
            if (b >= 1 && b + 1 < nb)
                h.push_back(std::max(pn[b],std::max(pn[b - 1],pn[b + 1])));
        }
        if (h.size() < 3)
            continue;
        std::nth_element(h.begin(),h.begin() + (ptrdiff_t)(h.size() / 2u),h.end());
        comb.push_back(std::make_pair(h[h.size() / 2u],f0));
    }

    // the best few, not close to each other
    std::vector<double> cand;
    {
        std::vector<std::pair<double,double>> sorted = comb;
        std::sort(sorted.begin(),sorted.end(),[](const std::pair<double,double> &a,const std::pair<double,double> &b) { return a.first > b.first; });
        for (const auto &c : sorted) {
            bool near = false;
            for (const double f : cand)
                near = near || fabs(f - c.second) < f * 0.003;
            if (!near)
                cand.push_back(c.second);
            if (cand.size() >= COMB_CANDIDATES)
                break;
        }
    }

    std::vector<double> prof;
    std::vector<unsigned int> count;
    for (const double f0 : cand) {
        // fold at periods around the one from the comb, to find it more closely
        const double p0 = srd / f0;
        double bp = p0,be = -1;
        for (int j=-15;j <= 15;j++) {
            const double p = p0 * (1.0 + j * 2e-5);
            const double e = fold(xd.data(),nd,p,(unsigned int)floor(p),prof,count);
            if (e > be) {
                be = e;
                bp = p;
            }
        }

        // what stands out of the rest of the audio at each phase
        const unsigned int bins = (unsigned int)floor(bp);
        fold(xd.data(),nd,bp,bins,prof,count);
        double pm = 0;
        unsigned int pc = 0;
        for (unsigned int b=0;b < bins;b++) {
            if (count[b] != 0) { pm += prof[b]; pc++; }
        }
        pm = pc != 0 ? pm / pc : 0;
        std::vector<double> ex(bins,0.0),nz(bins,0.0);
        double ex_total = 0;
        for (unsigned int b=0;b < bins;b++) {
            if (count[b] != 0) {
                nz[b] = var / count[b];
                ex[b] = (prof[b] - pm) * (prof[b] - pm) - nz[b];
                ex_total += ex[b];
            }
        }

        // the part of the period most of it is in
        const unsigned int wb = std::max(1u,std::min(bins,(unsigned int)lrint(CORE_MS / 1000.0 * srd * bins / bp)));
        double we = 0,wn = 0,best_we = -1e300,best_wn = 0;
        unsigned int best_b = 0;
        for (unsigned int b=0;b < wb;b++) {
            we += ex[b];
            wn += nz[b];
        }
        for (unsigned int b=0;b < bins;b++) {
            if (we > best_we) {
                best_we = we;
                best_wn = wn;
                best_b = b;
            }
            const unsigned int o = b,in = (b + wb) % bins;
            we += ex[in] - ex[o];
            wn += nz[in] - nz[o];
        }

        train_t tr;
        tr.snr = best_wn > 0 ? best_we / best_wn : 0;
        tr.concentration = ex_total > 0 ? best_we / ex_total : 0;
        tr.period = bp * D;
        tr.phase = (double)pos + (double)best_b * bp / bins * D;
        tr.found = tr.snr >= opt.threshold && tr.concentration >= MIN_CONCENTRATION;

        if ((tr.found && !best.found) || (tr.found == best.found && tr.snr > best.snr))
            best = tr;
    }

    return best;
}

// log of how likely the size measured of a pulse is if the train is not there (a) and if it is (p)
static void pulse_likelihood(const pulse_t &q,double &a,double &p) {
    const double sd = std::max((double)q.gain_sd,MIN_GAIN_SD),g = q.gain;
    const double vp = GAIN_SPREAD * GAIN_SPREAD + sd * sd;
    a = -0.5 * (g / sd) * (g / sd) - log(sd);
    p = -0.5 * (g - 1.0) * (g - 1.0) / vp - 0.5 * log(vp);
}

static double log_add(double a,double b) {
    const double m = std::max(a,b);
    return m + log(exp(a - m) + exp(b - m));
}

// The chance that a train is there, from the pulses of it so far, one pulse at a time: the forward
// half of what presence() works out, and how many pulses in a row it has been less than LOST_CHANCE.
class presence_filter_t {
public:
    void add(const pulse_t &q) {
        const double ls = log(SWITCH_CHANCE),lk = log(1.0 - SWITCH_CHANCE);
        double ea,ep;

        pulse_likelihood(q,ea,ep);
        const double na = ea + log_add(la + lk,lp + ls);
        const double np = ep + log_add(la + ls,lp + lk);
        const double m = log_add(na,np);
        la = na - m;
        lp = np - m;

        if (exp(lp) < LOST_CHANCE)
            absent++;
        else
            absent = 0;
    }

    bool lost(void) const {
        return absent >= LOST_PULSES;
    }
private:
    double                      la = log(0.5),lp = log(0.5);
    size_t                      absent = 0;
};

// Where the pulses of a train are expected: a straight line through where the last few were
// found, in time against which pulse of the train they are, leaving out the pulses far off it.
class line_t {
public:
    line_t(double t0,double period) : t0(t0), period(period) { }

    void add(int32_t k,double t,double w) {
        pts.push_back(point_t{k,t,w});
        if (pts.size() > LINE_POINTS)
            pts.pop_front();
    }

    size_t size(void) const {
        return pts.size();
    }

    double predict(int32_t k) const {
        if (pts.empty())
            return t0 + period * k;
        if (pts.size() < 4u)
            return pts.back().t + period * (k - pts.back().k);

        double a,b;
        std::vector<bool> use(pts.size(),true);
        if (!fit(use,a,b))
            return pts.back().t + period * (k - pts.back().k);

        // again, without the pulses far off the line, such as beacons sent late
        std::vector<double> r;
        for (size_t i=0;i < pts.size();i++)
            r.push_back(fabs(pts[i].t - (a + b * (pts[i].k - kc))));
        std::vector<double> rs = r;
        std::nth_element(rs.begin(),rs.begin() + (ptrdiff_t)(rs.size() / 2u),rs.end());
        const double lim = std::max(1.0,3.0 * 1.4826 * rs[rs.size() / 2u]);
        size_t kept = 0;
        for (size_t i=0;i < pts.size();i++) {
            use[i] = r[i] <= lim;
            if (use[i]) kept++;
        }
        if (kept >= 4u) {
            double a2,b2;
            if (fit(use,a2,b2)) {
                a = a2;
                b = b2;
            }
        }

        return a + b * (k - kc);
    }
private:
    struct point_t {
        int32_t                 k;
        double                  t;
        double                  w;
    };

    // weighted least squares line through the points in use, about kc, their middle
    bool fit(const std::vector<bool> &use,double &a,double &b) const {
        double sw = 0,sk = 0;
        for (size_t i=0;i < pts.size();i++) {
            if (use[i]) { sw += pts[i].w; sk += pts[i].w * pts[i].k; }
        }
        if (!(sw > 0))
            return false;
        kc = sk / sw;

        double st = 0,skk = 0,skt = 0;
        for (size_t i=0;i < pts.size();i++) {
            if (!use[i]) continue;
            const double dk = pts[i].k - kc;
            st += pts[i].w * pts[i].t;
            skk += pts[i].w * dk * dk;
            skt += pts[i].w * dk * pts[i].t;
        }
        a = st / sw;
        b = skk > 0 ? skt / skk : period;

        // a period that far off is not believable, so keep the one there was
        if (!(fabs(b - period) < period * 0.05))
            b = period;
        return true;
    }

    double                      t0;
    double                      period;
    mutable double              kc = 0;
    std::deque<point_t>         pts;
};

// what is kept to follow a train one way: where the pulses are expected, and how the last few matched
// the shape at each lag
struct follow_t {
    struct match_t {
        int32_t                 k;
        double                  p;                  // where it was expected
        std::vector<float>      z;                  // how well it matched at each lag, in standard errors
    };

    line_t                      line;
    std::deque<match_t>         matches;

    follow_t(double t0,double period) : line(t0,period) { }
};

class finder_t {
public:
    std::vector<pulse_t>        pulses;
    std::vector<run_t>          runs;

    finder_t(const options_t &opt,double sr) : opt(opt), sr(sr), hp(20.0,sr), fwd(0,1) {
        window = (int64_t)llrint(WINDOW_SECONDS * sr);
        hop = (int64_t)llrint(HOP_SECONDS * sr);
        keep = (int64_t)llrint(KEEP_SECONDS * sr);
        next_window = window;
        pre = (int)lrint(PRE_MS / 1000.0 * sr);
        len = pre + (int)lrint(opt.length_ms / 1000.0 * sr);
        match = std::min(len,pre + (int)lrint(MATCH_MS / 1000.0 * sr));
        reach = std::max(1,(int)lrint(opt.jitter_ms / 1000.0 * sr));
        near = std::max(1,std::min(reach,(int)lrint(NEAR_MS / 1000.0 * sr)));
    }

    // the next samples of the audio, mixed to one channel
    void push(const double *x,size_t n) {
        for (size_t i=0;i < n;i++)
            hist.s.push_back((float)hp.run(x[i]));
        process(false);
    }

    // at the end of the file
    void finish(void) {
        process(true);
    }

    int64_t position(void) const {
        return hist.end();
    }
private:
    void process(bool eof) {
        at_eof = eof;
        for (;;) {
            if (locked) {
                if (!back_done && found_in_run >= LINE_POINTS)
                    extend_back();

                const double p = fwd.line.predict(k_next);
                pulse_t pl;
                if (!measure(p,k_next,pl,true,fwd)) {
                    if (eof || p < (double)hist.base) {
                        end_run();
                        continue;
                    }
                    break;
                }

                pl.k = k_next++;
                pl.run = run_id;
                pulses.push_back(pl);
                fwd_presence.add(pl);
                if (pl.found) {
                    fwd.line.add(pl.k,pl.t,(double)pl.score * pl.score);
                    found_in_run++;
                }
                else if (pl.grouped) {
                    fwd.line.add(pl.k,pl.t,GROUP_WEIGHT);
                }

                // not given up on in the window it was found in
                if (pl.t > (double)run_window_end && fwd_presence.lost())
                    end_run();
            }
            else if (hist.end() >= next_window) {
                const int64_t from = next_window - window;
                next_window += hop;
                if (from >= hist.base)
                    look(from,next_window - hop);
            }
            else if (eof && !tail_done) {
                // and what is at the end, if it was not a whole window
                tail_done = true;
                const int64_t from = std::max(hist.base,hist.end() - window);
                if (next_window - hop < hist.end() && hist.end() - from >= hop)
                    look(from,hist.end());
            }
            else {
                break;
            }
        }

        // forget what is not needed, keeping what is needed to follow a train back from where it was found
        int64_t forget = hist.end() - keep;
        if (locked && !back_done)
            forget = std::min(forget,back_lower - (int64_t)reach - pre - 1);
        hist.forget(forget,(size_t)llrint(sr));
    }

    void look(int64_t from,int64_t to) {
        const train_t tr = find_train(hist.at(from),(size_t)(to - from),from,sr,opt);

        if (tr.found)
            start_run(tr,from,to);
    }

    // the average of the pulses of a train in [from,to), from off to off+n from where each starts
    std::vector<double> average(double t0,double period,int64_t from,int64_t to,int off,int n,unsigned int &count) const {
        std::vector<double> avg((size_t)n,0.0);
        const double k0 = ceil(((double)from - off - t0) / period);

        count = 0;
        for (double k=k0;;k += 1) {
            const int64_t s = (int64_t)llrint(t0 + k * period) + off;
            if (s + n > to || s + n > hist.end())
                break;
            if (s < from || s < hist.base)
                continue;

            const float *x = hist.at(s);
            for (int i=0;i < n;i++)
                avg[(size_t)i] += x[i];
            count++;
        }
        if (count != 0) {
            for (auto &v : avg)
                v /= count;
        }

        return avg;
    }

    void set_shape(const std::vector<double> &s) {
        shape = s;
        shape_changed();
    }

    // the part of the shape that is matched to find a pulse, less its mean
    void shape_changed(void) {
        double m = 0;

        for (int i=0;i < match;i++)
            m += shape[(size_t)i];
        m /= match;

        shape_match.resize((size_t)match);
        shape_norm = 0;
        for (int i=0;i < match;i++) {
            shape_match[(size_t)i] = shape[(size_t)i] - m;
            shape_norm += shape_match[(size_t)i] * shape_match[(size_t)i];
        }
        shape_norm = sqrt(shape_norm);
    }

    void start_run(const train_t &tr,int64_t from,int64_t to) {
        // Find where the pulses start in their average, so that the shape starts PRE_MS before that.
        // The part of the period found to have most of the pulse in it starts somewhere before that.
        const int before = (int)lrint(0.020 * sr),after = (int)lrint(0.060 * sr);
        unsigned int count = 0;
        std::vector<double> avg = average(tr.phase,tr.period,from,to,-before,before + after,count);
        if (count < 4u)
            return;

        // Its energy, smoothed over ONSET_SMOOTH_MS so that it does not drop where the pulse crosses
        // 0 between its first and second half, and how much of that is the rest of the audio, which is
        // most of the time around the pulse. The pulse starts where the energy, going back from the
        // most of it, drops to ONSET_FRACTION of the way from the rest of the audio to the most.
        const size_t an = avg.size();
        double m = 0;
        for (const double v : avg) m += v;
        m /= (double)an;

        std::vector<double> cs(an + 1u,0.0),env(an);
        for (size_t i=0;i < an;i++)
            cs[i + 1u] = cs[i] + (avg[i] - m) * (avg[i] - m);
        const size_t half = (size_t)std::max(1l,lrint(ONSET_SMOOTH_MS / 2000.0 * sr));
        size_t peak = 0;
        for (size_t i=0;i < an;i++) {
            const size_t a = i >= half ? i - half : 0,b = std::min(an,i + half + 1u);
            env[i] = (cs[b] - cs[a]) / (double)(b - a);
            if (env[i] > env[peak]) peak = i;
        }
        std::vector<double> sorted = env;
        std::nth_element(sorted.begin(),sorted.begin() + (ptrdiff_t)(an / 2u),sorted.end());
        const double floor_e = sorted[an / 2u];
        const double thr = floor_e + ONSET_FRACTION * (env[peak] - floor_e);
        size_t onset = peak;
        while (onset > 0 && env[onset - 1u] > thr)
            onset--;

        const double t0 = tr.phase - before + (double)onset;
        std::vector<double> s = average(t0,tr.period,from,to,-pre,len,count);
        if (count < 4u)
            return;

        // The train may be in only part of the window, which makes the average smaller than the pulses.
        // Make it the size of most of the pulses that can be seen in the window.
        {
            std::vector<double> gains;
            const double k0 = ceil(((double)from + pre - t0) / tr.period);
            for (double k=k0;;k += 1) {
                const int64_t a = (int64_t)llrint(t0 + k * tr.period) - pre;
                if (a + len > to || a + len > hist.end())
                    break;
                if (a < hist.base)
                    continue;
                const fit_t f = fit_shape(hist.at(a),s.data(),(size_t)len);
                if (f.ok && f.g > 4.0 * f.g_sd)
                    gains.push_back(f.g);
            }
            if (gains.size() >= 4u) {
                std::nth_element(gains.begin(),gains.begin() + (ptrdiff_t)(gains.size() / 2u),gains.end());
                const double g = gains[gains.size() / 2u];
                for (auto &v : s) v *= g;
            }
        }
        set_shape(s);

        run_id++;
        locked = true;
        back_done = false;
        found_in_run = 0;
        run_first = pulses.size();
        run_window_end = to;
        period = tr.period;
        fwd = follow_t(t0,tr.period);
        fwd_presence = presence_filter_t();
        run_matches.clear();

        // after the train before, if it was close
        back_lower = hist.base + reach + pre + 1;
        if (!runs.empty()) {
            const pulse_t &last = pulses[runs.back().first + runs.back().count - 1u];
            back_lower = std::max(back_lower,(int64_t)ceil(last.t + period / 2.0));
        }
        k_next = 0;
        while (t0 + k_next * period < (double)back_lower)
            k_next++;
    }

    // Find the pulse expected at p: the best match to the shape of the pulses before it near there.
    // False if the audio there is not in memory. If learn, the shape is changed a little to be more like it.
    bool measure(double p,int32_t k,pulse_t &pl,bool learn,follow_t &fw) {
        const int64_t ip = (int64_t)llrint(p);

        // and the audio between it and the pulses each side of it, unless it is the end of the file
        const int64_t off_end = ip - pre + (int64_t)llrint(OFF_PERIODS[3] * period) + len;
        if (!hist.has(ip - reach - pre - 1,std::max(ip + reach - pre + len + 2,at_eof ? 0 : off_end)))
            return false;

        // match the shape at each lag
        std::vector<double> c((size_t)(2 * reach + 1));
        for (int l=-reach;l <= reach;l++) {
            const float *x = hist.at(ip + l - pre);
            double s = 0;
            for (int i=0;i < match;i++)
                s += shape_match[(size_t)i] * x[i];
            c[(size_t)(l + reach)] = s;
        }

        auto score = [&](int l) -> double {
            const float *x = hist.at(ip + l - pre);
            double m = 0,e = 0;
            for (int i=0;i < match;i++) m += x[i];
            m /= match;
            for (int i=0;i < match;i++) e += (x[i] - m) * (x[i] - m);
            return (e > 0 && shape_norm > 0) ? c[(size_t)(l + reach)] / (sqrt(e) * shape_norm) : 0;
        };

        int ln = 0,lf = 0;
        for (int l=-reach;l <= reach;l++) {
            if (abs(l) <= near && c[(size_t)(l + reach)] > c[(size_t)(ln + reach)]) ln = l;
            if (c[(size_t)(l + reach)] > c[(size_t)(lf + reach)]) lf = l;
        }

        // the best match near where it should be has to be a peak, not where it would be further out
        auto peak = [&](int q) -> bool {
            return q > -reach && q < reach && c[(size_t)(q + reach)] >= c[(size_t)(q - 1 + reach)] && c[(size_t)(q + reach)] >= c[(size_t)(q + 1 + reach)];
        };

        int l = ln;
        double sc = peak(ln) ? score(ln) : 0.0;
        if (lf != ln && abs(lf) > near && peak(lf)) {
            const double sf = score(lf);
            if (sf >= FAR_SCORE && sf >= sc + FAR_BETTER) {
                l = lf;
                sc = sf;
            }
        }

        // where it is: where it matches best, if it matches well enough
        double tm = p;
        if (sc >= MIN_SCORE) {
            double d = 0;
            if (l > -reach && l < reach) {
                const double y0 = c[(size_t)(l - 1 + reach)],y1 = c[(size_t)(l + reach)],y2 = c[(size_t)(l + 1 + reach)];
                const double den = y0 - 2.0 * y1 + y2;
                if (den < 0)
                    d = clampd(0.5 * (y0 - y2) / den,-0.5,0.5);
            }
            tm = (double)(ip + l) + d;
        }

        // how much the size measured and the match vary with the rest of the audio around it, where there
        // are no pulses: their RMS, which the drum hits and such in music, more than noise has, count in
        double off_g = 0,off_c = 0;
        {
            unsigned int no = 0;
            for (const double d : OFF_PERIODS) {
                const int64_t qo = ip - pre + (int64_t)llrint(d * period);
                if (!hist.has(qo,qo + len))
                    continue;

                const float *x = hist.at(qo);
                const fit_t fo = fit_shape(x,shape.data(),(size_t)len);
                double co = 0;
                for (int i=0;i < match;i++)
                    co += shape_match[(size_t)i] * x[i];
                off_g += fo.g * fo.g;
                off_c += co * co;
                no++;
            }
            if (no != 0) {
                off_g = sqrt(off_g / no);
                off_c = sqrt(off_c / no);
            }
        }

        // and how well this one matched at each lag, against that, for the pulses after it
        const double csd = off_c;
        if (csd > 0) {
            follow_t::match_t m;
            m.k = k;
            m.p = p;
            m.z.resize(c.size());
            for (size_t i=0;i < c.size();i++)
                m.z[i] = (float)(c[i] / csd);
            run_matches.push_back(m);
            fw.matches.push_back(std::move(m));
            if (fw.matches.size() > GROUP_PULSES)
                fw.matches.pop_front();
        }

        // How big it is, and how sure that is. Under loud audio, the best match can be the audio and
        // not the pulse, so it is only taken as where the pulse is if it is big enough for that to be
        // sure, and otherwise the pulse is where the train says.
        fit_t f = fit_shape(hist.at((int64_t)llrint(tm) - pre),shape.data(),(size_t)len);
        double sd = std::max(f.g_sd,off_g);
        const bool found = sc >= MIN_SCORE && f.ok && f.g >= FOUND_Z * sd && f.g <= FOUND_MAX_GAIN;
        bool grouped = false;
        if (!found) {
            // where it and the pulses before it match best together
            tm = p;
            double gm;
            if (group_match(fw,gm)) {
                tm = p + gm;
                grouped = true;
            }
            f = fit_shape(hist.at((int64_t)llrint(tm) - pre),shape.data(),(size_t)len);
            sd = std::max(f.g_sd,off_g);
        }

        pl.t = tm;
        pl.found = found;
        pl.grouped = grouped;
        pl.offset = (float)(tm - p);
        pl.score = (float)sc;
        pl.gain = (float)f.g;
        pl.gain_sd = (float)std::max(1e-3,sd);
        pl.sd_scale = (float)(f.g_sd > 0 ? std::max(1.0,sd / f.g_sd) : 1.0);
        pl.noise = (float)f.noise;

        if (learn && found && sc >= LEARN_SCORE && f.g >= LEARN_Z * sd) {
            const float *x = hist.at((int64_t)llrint(tm) - pre);
            const double mid = (double)(len - 1) / 2.0,half = std::max(1.0,(double)len / 2.0);
            for (int i=0;i < len;i++) {
                const double u = ((double)i - mid) / half;
                shape[(size_t)i] += LEARN_RATE * ((x[i] - f.c0 - f.c1 * u) - shape[(size_t)i]);
            }
            shape_changed();
        }

        return true;
    }

    // How far the train is from where the line of fw says, from how the last few pulses matched the
    // shape together, lined up to where the line says each is now. False if they do not match well enough.
    bool group_match(const follow_t &fw,double &offset) const {
        const size_t nl = (size_t)(2 * reach + 1);
        std::vector<double> sum(nl,0.0);
        std::vector<unsigned int> count(nl,0u);

        if (fw.matches.size() < GROUP_PULSES / 2u)
            return false;

        for (const follow_t::match_t &m : fw.matches) {
            const int d = (int)lrint(fw.line.predict(m.k) - m.p);
            for (int l=-reach;l <= reach;l++) {
                const int j = l + d;
                if (j >= -reach && j <= reach) {
                    sum[(size_t)(l + reach)] += m.z[(size_t)(j + reach)];
                    count[(size_t)(l + reach)]++;
                }
            }
        }

        int best = 0;
        double bz = -1e300;
        for (int l=-reach;l <= reach;l++) {
            const unsigned int n = count[(size_t)(l + reach)];
            if (n < GROUP_PULSES / 2u)
                continue;
            const double z = sum[(size_t)(l + reach)] / sqrt((double)n);
            if (z > bz) {
                bz = z;
                best = l;
            }
        }
        if (bz < GROUP_Z || best <= -reach || best >= reach)
            return false;

        double d = 0;
        if (best > -reach && best < reach && count[(size_t)(best - 1 + reach)] != 0 && count[(size_t)(best + 1 + reach)] != 0) {
            const double y0 = sum[(size_t)(best - 1 + reach)],y1 = sum[(size_t)(best + reach)],y2 = sum[(size_t)(best + 1 + reach)];
            const double den = y0 - 2.0 * y1 + y2;
            if (den < 0)
                d = clampd(0.5 * (y0 - y2) / den,-0.5,0.5);
        }
        offset = best + d;
        return true;
    }

    // From where the train was found, follow it back to where it starts.
    void extend_back(void) {
        back_done = true;

        follow_t bk(0,period);
        int32_t kmin = 0;
        size_t used = 0;
        bool any = false;
        for (size_t i=run_first;i < pulses.size();i++) {
            if (!any || pulses[i].k < kmin) kmin = pulses[i].k;
            any = true;
            if (pulses[i].found && used < LINE_POINTS) {
                bk.line.add(pulses[i].k,pulses[i].t,(double)pulses[i].score * pulses[i].score);
                used++;
            }
        }
        if (!any || used < 4u)
            return;

        // and the chance the train is there going back, starting from the pulses after
        presence_filter_t pf;
        for (size_t i=std::min(pulses.size(),run_first + PRESENCE_START);i-- > run_first;)
            pf.add(pulses[i]);
        std::vector<pulse_t> back;
        for (int32_t k=kmin - 1;;k--) {
            const double p = bk.line.predict(k);
            if (p < (double)back_lower)
                break;

            pulse_t pl;
            if (!measure(p,k,pl,false,bk))
                break;
            pl.k = k;
            pl.run = run_id;
            back.push_back(pl);
            pf.add(pl);
            if (pl.found)
                bk.line.add(k,pl.t,(double)pl.score * pl.score);
            else if (pl.grouped)
                bk.line.add(k,pl.t,GROUP_WEIGHT);
            if (pf.lost())
                break;
        }

        std::reverse(back.begin(),back.end());
        pulses.insert(pulses.begin() + (ptrdiff_t)run_first,back.begin(),back.end());
    }

    // How sure it is that each of these pulses is there, as its weight. The train is either there or
    // not, and goes from one to the other with SWITCH_CHANCE between two pulses. Where it is there, the
    // size of each pulse measured is about 1, as the shape is the average of the pulses before it,
    // give or take GAIN_SPREAD and how certain the measurement is, and where it is not there, it is
    // 0 give or take how certain the measurement is. The chance that the train is there at each
    // pulse, given the sizes measured of all of them, is worked out by the forward-backward algorithm.
    static void presence(pulse_t *p,size_t n) {
        if (n == 0)
            return;

        const auto lse = log_add;
        std::vector<double> ea(n),ep(n),fa(n),fp(n);
        for (size_t i=0;i < n;i++)
            pulse_likelihood(p[i],ea[i],ep[i]);

        const double ls = log(SWITCH_CHANCE),lk = log(1.0 - SWITCH_CHANCE);
        fa[0] = log(0.5) + ea[0];
        fp[0] = log(0.5) + ep[0];
        for (size_t i=1;i < n;i++) {
            fa[i] = ea[i] + lse(fa[i - 1u] + lk,fp[i - 1u] + ls);
            fp[i] = ep[i] + lse(fa[i - 1u] + ls,fp[i - 1u] + lk);
        }

        double ba = 0,bp = 0;
        for (size_t i=n;i-- > 0;) {
            const double d = (fa[i] + ba) - (fp[i] + bp);
            p[i].weight = (float)(d > 700 ? 0.0 : 1.0 / (1.0 + exp(d)));

            if (i > 0) {
                const double na = lse(lk + ea[i] + ba,ls + ep[i] + bp);
                const double np = lse(ls + ea[i] + ba,lk + ep[i] + bp);
                ba = na;
                bp = np;
            }
        }
    }

    // As the train was followed, a pulse not found was looked for together with the pulses before it,
    // which are behind where the train is as it moves. Now that the pulses after it are known too, look
    // for it with the pulses each side of it, lined up to a line through where they are.
    void regroup(pulse_t *p,size_t n) {
        std::sort(run_matches.begin(),run_matches.end(),[](const follow_t::match_t &a,const follow_t::match_t &b) { return a.k < b.k; });

        // how each pulse matched, if it was measured
        std::vector<const follow_t::match_t*> mt(n,(const follow_t::match_t*)NULL);
        {
            size_t j = 0;
            for (size_t i=0;i < n;i++) {
                while (j < run_matches.size() && run_matches[j].k < p[i].k) j++;
                if (j < run_matches.size() && run_matches[j].k == p[i].k) mt[i] = &run_matches[j];
            }
        }

        const size_t half = GROUP_PULSES / 2u;
        std::vector<double> t(n);
        for (size_t i=0;i < n;i++)
            t[i] = p[i].t;

        for (size_t i=0;i < n;i++) {
            if (p[i].found)
                continue;

            const size_t a = i >= half ? i - half : 0,b = std::min(n,i + half + 1u);
            follow_t fw(p[i].t,period);
            for (size_t j=a;j < b;j++) {
                if (j != i)
                    fw.line.add(p[j].k,t[j],p[j].found ? 1.0 : (p[j].grouped ? GROUP_WEIGHT : 0.05));
                if (mt[j] != NULL)
                    fw.matches.push_back(*mt[j]);
            }

            double gm;
            if (group_match(fw,gm)) {
                const double nt = fw.line.predict(p[i].k) + gm;
                p[i].offset += (float)(nt - p[i].t);
                p[i].t = nt;
                p[i].grouped = true;
            }
        }
    }

    // The pulses not found are where the train said as it was followed, which is from the pulses found
    // before them. Now that the pulses found after them are known too, put them on a line through the
    // pulses found each side of them, which is closer. How far off that can be, and so how much of the
    // shape it does any good to take out there, grows with how many pulses it is from a pulse found.
    void place_unfound(pulse_t *p,size_t n) {
        const double ms = sr / 1000.0;

        // how much the shape is like itself moved by each number of samples
        const int maxlag = (int)lrint(PLACE_MAX_MS * ms);
        std::vector<double> rho((size_t)maxlag + 1u,0.0);
        {
            double m = 0;
            for (const double v : shape) m += v;
            m /= (double)shape.size();
            for (int l=0;l <= maxlag;l++) {
                double a = 0;
                for (size_t i=0;i + (size_t)l < shape.size();i++)
                    a += (shape[i] - m) * (shape[i + (size_t)l] - m);
                rho[(size_t)l] = a;
            }
            const double r0 = rho[0];
            if (r0 > 0) {
                for (auto &v : rho) v /= r0;
            }
        }

        // how much of the shape it does good to take out, if where the pulse is is off by sd samples:
        // the average of rho over that
        auto good = [&](double sd) -> double {
            if (sd < 0.5)
                return 1.0;
            double sw = 0,sr_ = 0;
            for (int l=-maxlag;l <= maxlag;l++) {
                const double w = exp(-0.5 * (l / sd) * (l / sd));
                sw += w;
                sr_ += w * rho[(size_t)abs(l)];
            }
            // and off by more than maxlag, none of it
            return std::max(0.0,sr_ / std::max(sw,sd * sqrt(2.0 * M_PI)));
        };

        // the pulses found, and those placed where the pulses around them were found together
        std::vector<size_t> anchor;
        for (size_t i=0;i < n;i++) {
            if (p[i].found || p[i].grouped)
                anchor.push_back(i);
        }

        // how many pulses each is from the nearest pulse found before and after it, by itself,
        // which is surely where the train is; -1 if none
        std::vector<int32_t> gap_before(n,-1),gap_after(n,-1);
        for (size_t i=0,last=SIZE_MAX;i < n;i++) {
            if (p[i].found) last = i;
            if (last != SIZE_MAX) gap_before[i] = p[i].k - p[last].k;
        }
        for (size_t i=n,last=SIZE_MAX;i-- > 0;) {
            if (p[i].found) last = i;
            if (last != SIZE_MAX) gap_after[i] = p[last].k - p[i].k;
        }

        size_t next = 0;    // first in anchor after i
        for (size_t i=0;i < n;i++) {
            while (next < anchor.size() && anchor[next] <= i)
                next++;
            if (p[i].found)
                continue;

            // put on a line through the anchors each side of it
            if (!p[i].grouped) {
                line_t ln(p[i].t,period);
                const size_t before = std::min<size_t>(next,PLACE_POINTS);
                const size_t after = std::min<size_t>(anchor.size() - next,PLACE_POINTS);
                for (size_t j=next - before;j < next + after;j++)
                    ln.add(p[anchor[j]].k,p[anchor[j]].t,p[anchor[j]].found ? 1.0 : GROUP_WEIGHT);
                if (before + after >= 2u) {
                    const double t = ln.predict(p[i].k);
                    p[i].offset += (float)(t - p[i].t);
                    p[i].t = t;
                }
            }

            // How far off that can be: from the pulses found by themselves, as what is found by matching
            // pulses together can be the audio when it is loud, and at least as early or late as a beacon is
            double sd;
            const int32_t gb = gap_before[i],ga = gap_after[i];
            if (gb >= 0 && ga >= 0)
                sd = hypot(PLACE_JITTER_MS,PLACE_BETWEEN_MS * std::min(gb,ga)) * ms;
            else if (gb >= 0 || ga >= 0)
                sd = hypot(PLACE_JITTER_MS,PLACE_BEYOND_MS * std::max(gb,ga)) * ms;
            else
                sd = PLACE_MAX_MS * ms;

            if ((gb < 0 || gb > PLACE_BLIND) && (ga < 0 || ga > PLACE_BLIND))
                p[i].weight = 0;
            else
                p[i].weight *= (float)good(sd);
        }
    }

    void end_run(void) {
        if (!back_done)
            extend_back();
        locked = false;

        const size_t n = pulses.size() - run_first;
        presence(&pulses[run_first],n);
        regroup(&pulses[run_first],n);
        place_unfound(&pulses[run_first],n);

        // without the pulses at each end that are not there
        const pulse_t *p = &pulses[run_first];
        size_t a = 0,b = n;
        while (a < b && p[a].weight < MIN_WEIGHT) a++;
        while (b > a && p[b - 1u].weight < MIN_WEIGHT) b--;
        std::vector<pulse_t> kept(p + a,p + b);
        pulses.resize(run_first);
        pulses.insert(pulses.end(),kept.begin(),kept.end());

        // look for the next train after this one, not in what this one was found in again
        if (!kept.empty())
            next_window = std::max(next_window,(int64_t)ceil(kept.back().t) + window);
        else
            next_window = std::max(next_window,hist.end());

        if (kept.size() >= 2u) {
            run_t r;
            r.first = run_first;
            r.count = kept.size();
            r.period = (kept.back().t - kept.front().t) / (double)(kept.back().k - kept.front().k);
            for (const double v : shape) r.level = std::max(r.level,fabs(v));
            runs.push_back(r);

            if (!opt.quiet)
                fprintf(stderr,"\rPulses from %s to %s, %zu of them, every %.3f ms, up to %.1f dBFS      \n",
                    time_str(kept.front().t / sr).c_str(),time_str(kept.back().t / sr).c_str(),kept.size(),
                    r.period * 1000.0 / sr,dbfs(r.level));
        }
        else {
            pulses.resize(run_first);
        }
    }

    const options_t&            opt;
    double                      sr;
    highpass_t                  hp;
    history_t                   hist;
    int64_t                     window,hop,keep;
    int64_t                     next_window;
    bool                        tail_done = false;
    bool                        at_eof = false;
    int                         pre,len,match,reach,near;

    // the train being followed
    bool                        locked = false;
    uint32_t                    run_id = 0;
    size_t                      run_first = 0;
    int64_t                     run_window_end = 0;
    double                      period = 0;
    follow_t                    fwd;
    presence_filter_t           fwd_presence;
    std::vector<follow_t::match_t> run_matches;     // how each pulse of the train matched, going either way
    int32_t                     k_next = 0;
    size_t                      found_in_run = 0;
    bool                        back_done = true;
    int64_t                     back_lower = 0;
    std::vector<double>         shape;
    std::vector<double>         shape_match;
    double                      shape_norm = 0;
};

static bool find_pulses(const options_t &opt,std::vector<pulse_t> &pulses,std::vector<run_t> &runs) {
    input_t in;

    if (!in.open(opt.input,opt.stream))
        return false;

    const double sr = in.dec->sample_rate;
    const unsigned int nch = (unsigned int)in.dec->ch_layout.nb_channels;
    finder_t finder(opt,sr);
    progress_t progress("Finding pulses",in.duration(),sr,opt.quiet);
    std::vector<std::vector<double>> x(nch);
    std::vector<double> mono;
    bool ok = true;

    const bool read = in.run([&](AVFrame *f) -> bool {
        for (auto &c : x) c.clear();
        if (!frame_samples(f,x)) {
            ok = false;
            return false;
        }

        const size_t n = x[0].size();
        mono.assign(n,0.0);
        for (unsigned int c=0;c < nch;c++) {
            for (size_t i=0;i < n;i++)
                mono[i] += x[c][i];
        }
        for (size_t i=0;i < n;i++)
            mono[i] /= nch;

        finder.push(mono.data(),n);
        progress.at(finder.position());
        return true;
    });
    if (!read || !ok)
        return false;

    finder.finish();
    progress.done(finder.position());

    pulses = std::move(finder.pulses);
    runs = std::move(finder.runs);
    return true;
}

static bool write_log(const options_t &opt,double sr,const std::vector<pulse_t> &pulses) {
    FILE *fp = fopen(opt.log_path.c_str(),"w");

    if (fp == NULL) {
        fprintf(stderr,"%s: %s\n",opt.log_path.c_str(),strerror(errno));
        return false;
    }

    fprintf(fp,"pulse,seconds,position,run,found,offset_ms,score,gain,gain_sd,noise_dbfs,weight\n");
    // found: 2 if found by matching it, 1 by matching the pulses around it together, 0 put on a line between them
    for (size_t i=0;i < pulses.size();i++) {
        const pulse_t &p = pulses[i];
        fprintf(fp,"%zu,%.6f,%.3f,%u,%d,%.4f,%.3f,%.4f,%.4f,%.1f,%.3f\n",i,p.t / sr,p.t,p.run,p.found ? 2 : (p.grouped ? 1 : 0),
            p.offset * 1000.0 / sr,p.score,p.gain,p.gain_sd,dbfs(p.noise),p.weight);
    }

    if (fclose(fp) != 0) {
        fprintf(stderr,"%s: %s\n",opt.log_path.c_str(),strerror(errno));
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
// writing the output file

class output_t {
public:
    AVFormatContext*            oc = NULL;
    AVCodecContext*             enc = NULL;
    AVPacket*                   pkt = NULL;
    AVFrame*                    frame = NULL;
    sample_out_t                format = OUT_S16;
    int64_t                     pts = 0;
    uint64_t                    clipped = 0;

    ~output_t() {
        if (oc != NULL) {
            if (!(oc->oformat->flags & AVFMT_NOFILE))
                avio_closep(&oc->pb);
            avformat_free_context(oc);
        }
        avcodec_free_context(&enc);
        av_packet_free(&pkt);
        av_frame_free(&frame);
    }

    bool open(const std::string &path,int sample_rate,const AVChannelLayout &layout,sample_out_t fmt) {
        enum AVCodecID id;
        enum AVSampleFormat sf;
        int r;

        format = fmt;
        switch (format) {
            case OUT_S16:   id = AV_CODEC_ID_PCM_S16LE; sf = AV_SAMPLE_FMT_S16; break;
            case OUT_S24:   id = AV_CODEC_ID_PCM_S24LE; sf = AV_SAMPLE_FMT_S32; break;
            case OUT_S32:   id = AV_CODEC_ID_PCM_S32LE; sf = AV_SAMPLE_FMT_S32; break;
            default:        id = AV_CODEC_ID_PCM_F32LE; sf = AV_SAMPLE_FMT_FLT; break;
        }

        if ((r=avformat_alloc_output_context2(&oc,NULL,"wav",path.c_str())) < 0 || oc == NULL) {
            fprintf(stderr,"%s: %s\n",path.c_str(),averr(r).c_str());
            return false;
        }

        const AVCodec *codec = avcodec_find_encoder(id);
        if (codec == NULL || (enc=avcodec_alloc_context3(codec)) == NULL) {
            fprintf(stderr,"No %s encoder\n",avcodec_get_name(id));
            return false;
        }
        enc->sample_fmt = sf;
        enc->sample_rate = sample_rate;
        enc->time_base = av_make_q(1,sample_rate);
        if (layout.order == AV_CHANNEL_ORDER_UNSPEC)
            av_channel_layout_default(&enc->ch_layout,layout.nb_channels);
        else if ((r=av_channel_layout_copy(&enc->ch_layout,&layout)) < 0)
            return false;
        if (format == OUT_S24)
            enc->bits_per_raw_sample = 24;
        if ((r=avcodec_open2(enc,codec,NULL)) < 0) {
            fprintf(stderr,"Cannot open the %s encoder: %s\n",codec->name,averr(r).c_str());
            return false;
        }

        AVStream *st = avformat_new_stream(oc,NULL);
        if (st == NULL)
            return false;
        if ((r=avcodec_parameters_from_context(st->codecpar,enc)) < 0)
            return false;
        st->time_base = enc->time_base;

        if ((r=avio_open(&oc->pb,path.c_str(),AVIO_FLAG_WRITE)) < 0) {
            fprintf(stderr,"%s: %s\n",path.c_str(),averr(r).c_str());
            return false;
        }

        // RF64 if it gets past 4GB
        AVDictionary *mopts = NULL;
        av_dict_set(&mopts,"rf64","auto",0);
        r = avformat_write_header(oc,&mopts);
        av_dict_free(&mopts);
        if (r < 0) {
            fprintf(stderr,"%s: cannot write the header: %s\n",path.c_str(),averr(r).c_str());
            return false;
        }

        pkt = av_packet_alloc();
        frame = av_frame_alloc();
        return pkt != NULL && frame != NULL;
    }

    // write n samples of each channel, from off in each of buf
    bool write(const std::vector<std::vector<double>> &buf,size_t off,size_t n) {
        const unsigned int nch = (unsigned int)buf.size();

        while (n > 0) {
            const size_t m = std::min<size_t>(n,4096);

            av_frame_unref(frame);
            frame->format = enc->sample_fmt;
            frame->nb_samples = (int)m;
            frame->sample_rate = enc->sample_rate;
            if (av_channel_layout_copy(&frame->ch_layout,&enc->ch_layout) < 0 || av_frame_get_buffer(frame,0) < 0)
                return false;

            for (unsigned int c=0;c < nch;c++) {
                const double *s = &buf[c][off];
                switch (format) {
                    case OUT_S16: {
                        int16_t *d = (int16_t*)frame->data[0] + c;
                        for (size_t i=0;i < m;i++) d[i * nch] = (int16_t)to_int(s[i],32768.0,1);
                        break; }
                    case OUT_S24: {
                        int32_t *d = (int32_t*)frame->data[0] + c;
                        for (size_t i=0;i < m;i++) d[i * nch] = (int32_t)to_int(s[i],8388608.0,256);
                        break; }
                    case OUT_S32: {
                        int32_t *d = (int32_t*)frame->data[0] + c;
                        for (size_t i=0;i < m;i++) d[i * nch] = (int32_t)to_int(s[i],2147483648.0,1);
                        break; }
                    default: {
                        float *d = (float*)frame->data[0] + c;
                        for (size_t i=0;i < m;i++) d[i * nch] = (float)s[i];
                        break; }
                }
            }

            frame->pts = pts;
            pts += (int64_t)m;
            int r = avcodec_send_frame(enc,frame);
            if (r < 0) {
                fprintf(stderr,"\nAudio encoding error: %s\n",averr(r).c_str());
                return false;
            }
            if (!write_encoded())
                return false;

            off += m;
            n -= m;
        }

        return true;
    }

    bool finish(void) {
        avcodec_send_frame(enc,NULL);
        if (!write_encoded())
            return false;

        int r = av_write_trailer(oc);
        if (r < 0) {
            fprintf(stderr,"Error finishing the output: %s\n",averr(r).c_str());
            return false;
        }

        return true;
    }
private:
    // v from -1 to 1 as an integer from -full to full-1, times mul
    int64_t to_int(double v,double full,int64_t mul) {
        double s = nearbyint(v * full);
        if (s < -full) { s = -full; clipped++; }
        else if (s > full - 1.0) { s = full - 1.0; clipped++; }
        return (int64_t)s * mul;
    }

    bool write_encoded(void) {
        for (;;) {
            int r = avcodec_receive_packet(enc,pkt);
            if (r == AVERROR(EAGAIN) || r == AVERROR_EOF)
                return true;
            if (r < 0) {
                fprintf(stderr,"\nAudio encoding error: %s\n",averr(r).c_str());
                return false;
            }

            av_packet_rescale_ts(pkt,enc->time_base,oc->streams[0]->time_base);
            pkt->stream_index = 0;
            if ((r=av_interleaved_write_frame(oc,pkt)) < 0) {
                fprintf(stderr,"\nError writing the output: %s\n",averr(r).c_str());
                return false;
            }
        }
    }
};

/////////////////////////////////////////////////////////////////////////////
// second pass: taking the pulses out

// For each pulse, the shape of the pulse is the average of the pulses of the same train around
// it, not counting itself, each lined up to where it is to a fraction of a sample, and weighted
// by how quiet the rest of the audio is under it. That is fitted to the pulse, with a straight
// line for the rest of the audio under it, and subtracted. Each channel is done on its own.
class remover_t {
public:
    remover_t(const options_t &opt,double sr,unsigned int nch,const std::vector<pulse_t> &all,output_t *out) : opt(opt), out(out) {
        for (const pulse_t &p : all) {
            if (p.weight >= MIN_WEIGHT)
                pl.push_back(p);
        }

        pre = (int)lrint(PRE_MS / 1000.0 * sr);
        len = pre + (int)lrint(opt.length_ms / 1000.0 * sr);
        buf.resize(nch);
        sums.assign(nch,std::vector<double>((size_t)len,0.0));

        // where the train of each pulse starts and ends
        run_lo.resize(pl.size());
        run_hi.resize(pl.size());
        for (size_t i=0;i < pl.size();i++)
            run_lo[i] = (i > 0 && pl[i - 1u].run == pl[i].run) ? run_lo[i - 1u] : i;
        for (size_t i=pl.size();i-- > 0;)
            run_hi[i] = (i + 1u < pl.size() && pl[i + 1u].run == pl[i].run) ? run_hi[i + 1u] : i;

        // the shape fades in over the first half of PRE_MS and out over the last TAIL_FADE_MS
        fade.assign((size_t)len,1.0);
        const int fi = std::max(1,pre / 2),fo = std::max(1,std::min(len / 2,(int)lrint(TAIL_FADE_MS / 1000.0 * sr)));
        for (int i=0;i < fi;i++)
            fade[(size_t)i] = 0.5 - 0.5 * cos(M_PI * (i + 0.5) / fi);
        for (int i=0;i < fo;i++)
            fade[(size_t)(len - 1 - i)] = 0.5 - 0.5 * cos(M_PI * (i + 0.5) / fo);
    }

    bool push(const AVFrame *f) {
        if (!frame_samples(f,buf))
            return false;
        return process(false);
    }

    bool finish(void) {
        return process(true);
    }

    int64_t position(void) const {
        return base + (int64_t)buf[0].size();
    }

    size_t removed = 0;
private:
    int64_t end(void) const {
        return base + (int64_t)buf[0].size();
    }

    double at(unsigned int c,int64_t pos) const {
        if (pos < base || pos >= end())
            return 0;
        return buf[c][(size_t)(pos - base)];
    }

    struct seg_t {
        std::vector<std::vector<double>> s;         // each channel
        double                  w = 0;
    };

    // the audio of pulse j, lined up to where it starts to a fraction of a sample
    void extract(size_t j) {
        const pulse_t &p = pl[j];
        const double s0 = p.t - pre;
        const int64_t i0 = (int64_t)floor(s0);
        const double *w = sinc().at(s0 - (double)i0);
        const int half = SINC_TAPS / 2;
        seg_t sg;

        sg.w = p.weight >= SHAPE_MIN_WEIGHT ? (double)p.weight / ((double)p.noise * p.noise + NOISE_FLOOR * NOISE_FLOOR) : 0.0;
        sg.s.resize(buf.size());
        for (unsigned int c=0;c < buf.size();c++) {
            std::vector<double> &o = sg.s[c];
            o.resize((size_t)len);

            const int64_t from = i0 - half + 1,to = i0 + len + half;
            if (from >= base && to <= end()) {
                const double *x = &buf[c][(size_t)(from - base)];
                for (int i=0;i < len;i++) {
                    double v = 0;
                    for (int k=0;k < SINC_TAPS;k++) v += w[k] * x[i + k];
                    o[(size_t)i] = v;
                }
            }
            else {
                for (int i=0;i < len;i++) {
                    double v = 0;
                    for (int k=0;k < SINC_TAPS;k++) v += w[k] * at(c,from + i + k);
                    o[(size_t)i] = v;
                }
            }
        }

        segs.push_back(std::move(sg));
    }

    seg_t &seg(size_t j) {
        return segs[j - seg_first];
    }

    void add(size_t j,double sign) {
        seg_t &sg = seg(j);
        for (unsigned int c=0;c < buf.size();c++) {
            std::vector<double> &s = sums[c];
            for (int i=0;i < len;i++)
                s[(size_t)i] += sign * sg.w * sg.s[c][(size_t)i];
        }
        wsum += sign * sg.w;
    }

    void remove(size_t k) {
        const size_t lo = std::max(run_lo[k],k >= opt.neighbors ? k - opt.neighbors : 0);
        const size_t hi = std::min(run_hi[k],k + opt.neighbors) + 1u;

        // the sums over the pulses from lo to hi
        if (lo >= sum_hi) {
            for (auto &s : sums) std::fill(s.begin(),s.end(),0.0);
            wsum = 0;
            sum_lo = sum_hi = lo;
        }
        while (sum_hi < hi) add(sum_hi++,1.0);
        while (sum_lo < lo) add(sum_lo++,-1.0);
        while (seg_first < sum_lo) {
            segs.pop_front();
            seg_first++;
        }

        seg_t &me = seg(k);
        const double wo = wsum - me.w;
        if (!(wo > 0) || !(wo > wsum * 1e-9))
            return;

        const pulse_t &p = pl[k];
        const double s0 = p.t - pre;
        const int64_t n0 = (int64_t)ceil(s0);
        const double *w = sinc().at((double)n0 - s0);
        const int half = SINC_TAPS / 2;
        const int ends = std::max(1,pre / 2);
        std::vector<double> sh((size_t)len),pad((size_t)(len + 2 * SINC_TAPS),0.0);
        bool any = false;

        for (unsigned int c=0;c < buf.size();c++) {
            // the shape from the others, starting and ending at 0
            const std::vector<double> &s = sums[c];
            const std::vector<double> &mine = me.s[c];
            for (int i=0;i < len;i++)
                sh[(size_t)i] = (s[(size_t)i] - me.w * mine[(size_t)i]) / wo;

            double m0 = 0,m1 = 0;
            for (int i=0;i < ends;i++) {
                m0 += sh[(size_t)i];
                m1 += sh[(size_t)(len - 1 - i)];
            }
            m0 /= ends;
            m1 /= ends;
            for (int i=0;i < len;i++)
                sh[(size_t)i] = (sh[(size_t)i] - (m0 + (m1 - m0) * i / (len - 1))) * fade[(size_t)i];

            // how much of it is in this pulse
            const fit_t f = fit_shape(mine.data(),sh.data(),(size_t)len);
            if (!f.ok)
                continue;

            // pulled toward 1 as much as the pulse is uncertain, if the train is there, or 0 if not
            const double vg = (double)f.g_sd * f.g_sd * p.sd_scale * p.sd_scale,vp = GAIN_SPREAD * GAIN_SPREAD;
            const double g = clampd((f.g / vg + (double)p.weight / vp) / (1.0 / vg + 1.0 / vp),0.0,MAX_GAIN);
            if (!(g > 0))
                continue;

            // subtract it where the pulse is, moved back from lined up
            for (int i=0;i < len;i++)
                pad[(size_t)(i + SINC_TAPS)] = sh[(size_t)i];
            for (int i=-half;i < len + half;i++) {
                const int64_t pos = n0 + i;
                if (pos < base || pos >= end())
                    continue;

                double v = 0;
                for (int k=0;k < SINC_TAPS;k++)
                    v += w[k] * pad[(size_t)(i + SINC_TAPS - half + 1 + k)];
                buf[c][(size_t)(pos - base)] -= g * v;
            }
            any = true;
        }

        if (any)
            removed++;
    }

    bool process(bool eof) {
        const int half = SINC_TAPS / 2;

        for (;;) {
            // line up the pulses the audio is here for
            while (next_ext < pl.size() && (eof || (int64_t)floor(pl[next_ext].t - pre) + len + half + 1 <= end()))
                extract(next_ext++);

            if (next_rem >= pl.size())
                break;

            // take out the next pulse once the pulses its shape comes from are lined up, and the pulses
            // that are close enough to it to overlap it, so that they are lined up from the audio as it was
            const size_t k = next_rem;
            const size_t need = std::min(run_hi[k],k + opt.neighbors);
            if (next_ext <= need)
                break;
            if (next_ext < pl.size() && pl[next_ext].t < pl[k].t + len + 2 * SINC_TAPS)
                break;

            remove(k);
            next_rem++;
        }

        // write what no pulse left can change
        int64_t upto = end();
        if (!eof && next_rem < pl.size())
            upto = std::min(upto,(int64_t)floor(pl[next_rem].t - pre) - half - 2);
        if (upto > written) {
            if (!out->write(buf,(size_t)(written - base),(size_t)(upto - written)))
                return false;
            written = upto;
        }

        // and forget it, a lot at a time
        if (written - base >= (int64_t)(1 << 20) || (eof && written > base)) {
            const size_t n = (size_t)(written - base);
            for (auto &c : buf)
                c.erase(c.begin(),c.begin() + (ptrdiff_t)n);
            base = written;
        }

        return true;
    }

    const options_t&            opt;
    output_t*                   out;
    std::vector<pulse_t>        pl;                 // the pulses to take out
    std::vector<size_t>         run_lo,run_hi;      // the first and last pulse of the train of each
    int                         pre = 0,len = 0;
    std::vector<double>         fade;

    std::vector<std::vector<double>> buf;           // audio of each channel, from base
    int64_t                     base = 0;
    int64_t                     written = 0;

    std::deque<seg_t>           segs;               // the pulses lined up, from seg_first
    size_t                      seg_first = 0;
    std::vector<std::vector<double>> sums;          // weighted sums of the pulses from sum_lo to sum_hi
    double                      wsum = 0;
    size_t                      sum_lo = 0,sum_hi = 0;
    size_t                      next_ext = 0,next_rem = 0;
};

static sample_out_t default_format(const AVCodecContext *dec) {
    switch (av_get_packed_sample_fmt(dec->sample_fmt)) {
        case AV_SAMPLE_FMT_U8:
        case AV_SAMPLE_FMT_S16:
            return OUT_S16;
        case AV_SAMPLE_FMT_S32:
            return (dec->bits_per_raw_sample > 0 && dec->bits_per_raw_sample <= 24) ? OUT_S24 : OUT_S32;
        case AV_SAMPLE_FMT_S64:
            return OUT_S32;
        default:
            break;
    }

    return OUT_F32;
}

static const char *format_name(sample_out_t f) {
    switch (f) {
        case OUT_S16:   return "16-bit";
        case OUT_S24:   return "24-bit";
        case OUT_S32:   return "32-bit";
        case OUT_F32:   return "32-bit float";
        default:        break;
    }
    return "?";
}

static bool remove_pulses(const options_t &opt,const std::vector<pulse_t> &pulses) {
    input_t in;
    output_t out;

    if (!in.open(opt.input,opt.stream))
        return false;

    const double sr = in.dec->sample_rate;
    const unsigned int nch = (unsigned int)in.dec->ch_layout.nb_channels;
    const sample_out_t fmt = opt.format != OUT_DEFAULT ? opt.format : default_format(in.dec);

    if (!out.open(opt.output,in.dec->sample_rate,in.dec->ch_layout,fmt))
        return false;

    remover_t rm(opt,sr,nch,pulses,&out);
    progress_t progress("Taking them out",in.duration(),sr,opt.quiet);

    const bool read = in.run([&](AVFrame *f) -> bool {
        if (!rm.push(f))
            return false;
        progress.at(rm.position());
        return true;
    });
    if (!read || !rm.finish())
        return false;
    progress.done(rm.position());

    if (!out.finish())
        return false;

    if (!opt.quiet) {
        fprintf(stderr,"Took out %zu pulses, wrote %s of %s audio to %s\n",rm.removed,time_str((double)out.pts / sr).c_str(),
            format_name(fmt),opt.output.c_str());
        if (out.clipped != 0)
            fprintf(stderr,"%llu samples were clipped\n",(unsigned long long)out.clipped);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////

static bool parse_double(const char *s,double &v) {
    char *end = NULL;
    v = strtod(s,&end);
    return end != s && *end == 0;
}

int main(int argc,char **argv) {
    std::vector<std::string> files;
    options_t opt;

    for (int i=1;i < argc;i++) {
        const char *a = argv[i];

        if (a[0] == '-' && a[1] != 0) {
            const std::string o = a + 1;
            // options that take a value
            const bool takes = (o == "p" || o == "r" || o == "j" || o == "L" || o == "k" || o == "t" ||
                                o == "a" || o == "f" || o == "log");
            const char *v = NULL;
            double d;

            if (takes) {
                if (++i >= argc) {
                    fprintf(stderr,"%s needs a value\n",a);
                    return 1;
                }
                v = argv[i];
            }

            if (o == "p") {
                if (!parse_double(v,d) || d < 10 || d > 2000) {
                    fprintf(stderr,"-p %s: the period must be 10 to 2000 ms\n",v);
                    return 1;
                }
                opt.period_ms = d;
            }
            else if (o == "r") {
                if (!parse_double(v,d) || d < 0.1 || d > 20) {
                    fprintf(stderr,"-r %s: the range must be 0.1 to 20 percent\n",v);
                    return 1;
                }
                opt.range = d;
            }
            else if (o == "j") {
                if (!parse_double(v,d) || d < 0.05 || d > 20) {
                    fprintf(stderr,"-j %s: must be 0.05 to 20 ms\n",v);
                    return 1;
                }
                opt.jitter_ms = d;
            }
            else if (o == "L") {
                if (!parse_double(v,d) || d < 5 || d > 1000) {
                    fprintf(stderr,"-L %s: must be 5 to 1000 ms\n",v);
                    return 1;
                }
                opt.length_ms = d;
            }
            else if (o == "k") {
                if (!parse_double(v,d) || d < 1 || d > 1000 || d != floor(d)) {
                    fprintf(stderr,"-k %s: must be 1 to 1000 pulses\n",v);
                    return 1;
                }
                opt.neighbors = (unsigned int)d;
            }
            else if (o == "t") {
                if (!parse_double(v,d) || d <= 0) {
                    fprintf(stderr,"-t %s: must be more than 0\n",v);
                    return 1;
                }
                opt.threshold = d;
            }
            else if (o == "a") {
                if (!parse_double(v,d) || d < 0 || d != floor(d)) {
                    fprintf(stderr,"-a %s: give the index of the stream, from 0\n",v);
                    return 1;
                }
                opt.stream = (int)d;
            }
            else if (o == "f") {
                const std::string s = v;
                if (s == "s16") opt.format = OUT_S16;
                else if (s == "s24") opt.format = OUT_S24;
                else if (s == "s32") opt.format = OUT_S32;
                else if (s == "f32") opt.format = OUT_F32;
                else {
                    fprintf(stderr,"-f %s: must be s16, s24, s32 or f32\n",v);
                    return 1;
                }
            }
            else if (o == "log") {
                opt.log_path = v;
            }
            else if (o == "y") {
                opt.overwrite = true;
            }
            else if (o == "n") {
                opt.measure_only = true;
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

    if (files.size() != (opt.measure_only ? 1u : 2u)) {
        help();
        return 1;
    }
    opt.input = files[0];
    if (!opt.measure_only) {
        opt.output = files[1];
        if (same_file(opt.input,opt.output)) {
            fprintf(stderr,"%s: the output would write over the input\n",opt.output.c_str());
            return 1;
        }
        if (!opt.overwrite && file_exists(opt.output)) {
            fprintf(stderr,"%s already exists (-y writes over it)\n",opt.output.c_str());
            return 1;
        }
    }
    if (!opt.log_path.empty() && (same_file(opt.log_path,opt.input) || (!opt.measure_only && same_file(opt.log_path,opt.output)))) {
        fprintf(stderr,"%s: the log would write over the input or the output\n",opt.log_path.c_str());
        return 1;
    }

    av_log_set_level(AV_LOG_ERROR);

    double sr;
    {
        input_t in;
        if (!in.open(opt.input,opt.stream))
            return 1;

        sr = in.dec->sample_rate;
        if (!opt.quiet) {
            char layout[128] = "";
            av_channel_layout_describe(&in.dec->ch_layout,layout,sizeof(layout));
            fprintf(stderr,"%s: %s audio, %d Hz, %d channels (%s), %s, %s\n",opt.input.c_str(),avcodec_get_name(in.dec->codec_id),
                in.dec->sample_rate,in.dec->ch_layout.nb_channels,layout,av_get_sample_fmt_name(in.dec->sample_fmt),
                time_str(in.duration()).c_str());
        }
    }

    std::vector<pulse_t> pulses;
    std::vector<run_t> runs;
    if (!find_pulses(opt,pulses,runs))
        return 1;

    if (!opt.quiet) {
        size_t n = 0;
        double secs = 0;
        for (const run_t &r : runs) {
            n += r.count;
            secs += (pulses[r.first + r.count - 1u].t - pulses[r.first].t) / sr;
        }
        if (runs.empty())
            fprintf(stderr,"No pulses found\n");
        else
            fprintf(stderr,"Found %zu pulses in %zu trains, over %s of the audio\n",n,runs.size(),time_str(secs).c_str());
    }

    if (!opt.log_path.empty() && !write_log(opt,sr,pulses))
        return 1;

    if (opt.measure_only)
        return 0;

    return remove_pulses(opt,pulses) ? 0 : 1;
}
