#ifndef VHSSTABILIZE_PLANE_H
#define VHSSTABILIZE_PLANE_H

#include <stddef.h>
#include <stdint.h>

// one plane of a picture, one component per sample, 1 byte per sample (8 bits)
// or 2 bytes per sample in host byte order (9 to 16 bits)
struct plane_t {
    uint8_t*                    data = NULL;
    ptrdiff_t                   stride = 0;         // bytes from one line to the next
    unsigned int                width = 0;          // samples per line
    unsigned int                height = 0;         // lines
    unsigned int                bytes = 1;          // bytes per sample
    unsigned int                depth = 8;          // bits per sample

    unsigned int maxval(void) const {
        return (1u << depth) - 1u;
    }
    uint8_t *line(unsigned int y) const {
        return data + ((ptrdiff_t)y * stride);
    }
};

// the number of lines first, first+step, first+(2*step), ... in a plane of that height
static inline unsigned int count_lines(unsigned int height,unsigned int first,unsigned int step) {
    return height > first ? ((height - first + step - 1u) / step) : 0u;
}

#endif //VHSSTABILIZE_PLANE_H
