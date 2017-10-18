
/* generic */
static inline int resample_interpolate8(const unsigned int channel) {
    return (int)resample_interpolate_generic(channel);
}

static inline int resample_interpolate16(const unsigned int channel) {
    return (int)resample_interpolate_generic(channel);
}

