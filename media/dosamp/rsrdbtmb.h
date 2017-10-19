#define bytes_per_sample (sizeof(sample_type_t) * sample_channels)

#define LOAD() { { register unsigned int i; for (i=0;i < sample_channels;i++) resample_state.c[i] = src[i]; }; convert_rdbuf.pos += bytes_per_sample; src += sample_channels; }

#define SWAP() { register unsigned int i; for (i=0;i < sample_channels;i++) resample_state.p[i] = resample_state.c[i]; }

#define INTERPOLATE() { register unsigned int i; for (i=0;i < sample_channels;i++) tmp[i] = (signed long)resample_interpolate_func(i) << 8L; }

#define AVERAGE() { register unsigned int i; for (i=0;i < sample_channels;i++) resample_state.f[i] += ((((signed long)tmp[i] - (signed long)resample_state.f[i]) * resample_state.f_best) >> 8L); }

#define STORE() { { register unsigned int i; for (i=0;i < sample_channels;i++) dst[i] = (sample_type_t)(resample_state.f[i] >> 8L); }; dst += sample_channels; samples--; r++; }

    /* NTS: Open Watcom is smart enough to turn for (i=0;i < constant;i++) into unrolled loop for small values of constant. Good! This code relies on it! */

    sample_type_t dosamp_FAR *src = (sample_type_t dosamp_FAR*)dosamp_ptr_add_normalize(convert_rdbuf.buffer,convert_rdbuf.pos);
    signed long tmp[resample_max_channels];
    uint32_t r = 0;

    if (resample_state.init == 0) {
        if ((convert_rdbuf.pos+bytes_per_sample+bytes_per_sample) > convert_rdbuf.len) return r;
        resample_state.frac += resample_100;
        resample_state.init = 1;

        LOAD();
    }

    while (samples > 0) {
        if (resample_state.frac >= resample_100) {
            if ((convert_rdbuf.pos+bytes_per_sample) > convert_rdbuf.len) return r;
            resample_state.frac -= resample_100;

            SWAP();
            LOAD();
        }
        else {
            INTERPOLATE();
            AVERAGE();
            STORE();

            resample_state.frac += resample_state.step;
        }
    }

    return r;

#undef SWAP
#undef LOAD
#undef INTERPOLATE
#undef sample_type_t
#undef sample_channels
#undef bytes_per_sample
#undef resample_interpolate_func

