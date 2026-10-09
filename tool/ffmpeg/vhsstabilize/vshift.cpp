/* vhsstabilize: moving the lines of a picture up or down, to a fraction of a line */

#include <math.h>
#include <string.h>

#include "vshift.h"

static inline int clampi(int v,int lo,int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

template <typename T> static void fill_line(T *d,unsigned int width,unsigned int fill) {
    for (unsigned int x=0;x < width;x++) d[x] = (T)fill;
}

template <typename T> static void shift_lines_t(const plane_t &dst,const plane_t &src,unsigned int first,unsigned int step,double shift,interp_t interp,unsigned int fill) {
    const unsigned int n = count_lines(src.height,first,step);
    const size_t line_bytes = (size_t)src.width * sizeof(T);
    const int maxval = (int)src.maxval();
    const T *sl[4];
    int w[4];

    for (unsigned int j=0;j < n;j++) {
        T *d = (T*)dst.line(first + (j * step));
        const double s = (double)j - shift;        // where in src this line comes from

        // lines that nothing moved into
        if (s < -0.5 || s > ((double)n - 0.5)) {
            fill_line<T>(d,src.width,fill);
            continue;
        }

        const double fl = floor(s);
        const int i = (int)fl;
        const double f = s - fl;

        // whole lines are copied as they are
        int whole = -1;
        if (interp == INTERP_NEAREST)
            whole = clampi((int)floor(s + 0.5),0,(int)n - 1);
        else if (f < (1.0 / 1024.0))
            whole = clampi(i,0,(int)n - 1);
        else if (f > (1023.0 / 1024.0))
            whole = clampi(i + 1,0,(int)n - 1);

        if (whole >= 0) {
            memcpy(d,src.line(first + ((unsigned int)whole * step)),line_bytes);
            continue;
        }

        interp_weights(w,f,interp);

        // lines above and below the picture repeat its top and bottom lines
        for (int k=0;k < 4;k++)
            sl[k] = (const T*)src.line(first + ((unsigned int)clampi(i - 1 + k,0,(int)n - 1) * step));

        for (unsigned int x=0;x < src.width;x++) {
            int v = (w[0] * (int)sl[0][x]) + (w[1] * (int)sl[1][x]) + (w[2] * (int)sl[2][x]) + (w[3] * (int)sl[3][x]);
            v = (v < 0) ? 0 : ((v + 8192) >> 14);
            d[x] = (T)(v > maxval ? maxval : v);
        }
    }
}

void shift_lines(const plane_t &dst,const plane_t &src,unsigned int first,unsigned int step,double shift,interp_t interp,unsigned int fill) {
    if (src.bytes == 2)
        shift_lines_t<uint16_t>(dst,src,first,step,shift,interp,fill);
    else
        shift_lines_t<uint8_t>(dst,src,first,step,shift,interp,fill);
}
