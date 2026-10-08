
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "bmpio.h"

// BITMAPINFOHEADER biCompression values
#define BI_RGB                      0u
#define BI_BITFIELDS                3u
#define BI_ALPHABITFIELDS           6u

// no more pixels than this in one image, so that the sizes cannot overflow and memory use stays sane
#define MAX_PIXELS                  (1ul << 28ul)

// little endian values at any offset in the file
static inline unsigned int le16(const uint8_t *p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8u);
}

static inline uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8u) | ((uint32_t)p[2] << 16u) | ((uint32_t)p[3] << 24u);
}

static inline void put_le16(uint8_t *p,const unsigned int v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8u);
}

static inline void put_le32(uint8_t *p,const uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8u);
    p[2] = (uint8_t)(v >> 16u);
    p[3] = (uint8_t)(v >> 24u);
}

static bool read_file(const std::string &path,std::vector<uint8_t> &data,std::string &err) {
    FILE *fp = fopen(path.c_str(),"rb");
    if (fp == NULL) {
        err = strerror(errno);
        return false;
    }

    data.clear();
    uint8_t buf[65536];
    size_t rd;
    while ((rd=fread(buf,1,sizeof(buf),fp)) > 0)
        data.insert(data.end(),buf,buf+rd);

    const bool failed = ferror(fp) != 0;
    fclose(fp);
    if (failed) {
        err = "read error";
        return false;
    }

    return true;
}

// a color channel of a 32bpp pixel, from its BI_BITFIELDS mask
struct channel_t {
    uint32_t                    mask = 0;
    unsigned int                shift = 0;
    uint32_t                    max = 0;            // largest value, mask >> shift. 0 if the channel is not there.
};

// returns false if the bits of the mask are not all together
static bool make_channel(channel_t &ch,uint32_t mask) {
    ch.mask = mask;
    ch.shift = 0;
    ch.max = 0;
    if (mask == 0)
        return true;

    while (!(mask & 1u)) {
        mask >>= 1u;
        ch.shift++;
    }

    ch.max = mask;
    return (mask & (mask + 1u)) == 0;
}

// the channel's value in a pixel, scaled to 0-255
static inline uint8_t channel_value(const channel_t &ch,const uint32_t px) {
    if (ch.max == 0)
        return 0;

    const uint32_t v = (px & ch.mask) >> ch.shift;
    if (ch.max == 255u)
        return (uint8_t)v;

    return (uint8_t)(((uint64_t)v * 255u + (ch.max / 2u)) / ch.max);
}

bool read_bmp_truecolor(const std::string &path,rgb_image_t &img,std::string &err) {
    std::vector<uint8_t> f;
    if (!read_file(path,f,err))
        return false;

    if (f.size() < (14u + 12u) || f[0] != 'B' || f[1] != 'M') {
        err = "not a BMP file";
        return false;
    }

    const uint32_t data_ofs = le32(&f[10]);
    const uint32_t hdr_size = le32(&f[14]);
    int64_t width,height;
    unsigned int bpp;
    uint32_t compression = BI_RGB;
    bool topdown = false;

    if (hdr_size == 12u) {
        // OS/2 BITMAPCOREHEADER
        width = le16(&f[18]);
        height = le16(&f[20]);
        bpp = le16(&f[24]);
    }
    else if (hdr_size >= 40u && (uint64_t)hdr_size + 14u <= f.size()) {
        // BITMAPINFOHEADER, or a larger one that starts with it. A negative height means the top row is first.
        width = (int32_t)le32(&f[18]);
        height = (int32_t)le32(&f[22]);
        bpp = le16(&f[28]);
        compression = le32(&f[30]);
        if (height < 0) {
            topdown = true;
            height = -height;
        }
    }
    else {
        err = "unsupported BMP header (" + std::to_string(hdr_size) + " bytes)";
        return false;
    }

    if (bpp != 24u && bpp != 32u) {
        err = std::to_string(bpp) + " bits per pixel, only 24 and 32 bits per pixel are supported";
        return false;
    }

    channel_t red,green,blue;
    if (compression == BI_RGB) {
        make_channel(red,0x00FF0000u);
        make_channel(green,0x0000FF00u);
        make_channel(blue,0x000000FFu);
    }
    else if ((compression == BI_BITFIELDS || compression == BI_ALPHABITFIELDS) && bpp == 32u) {
        // the red, green and blue masks follow a 40-byte BITMAPINFOHEADER, or are the next
        // fields of a larger header. Either way, they are at the same place in the file.
        if (f.size() < (14u + 40u + 12u)) {
            err = "file is too short for the color masks";
            return false;
        }

        if (!make_channel(red,le32(&f[54])) || !make_channel(green,le32(&f[58])) || !make_channel(blue,le32(&f[62]))) {
            err = "color masks with bits that are not all together are not supported";
            return false;
        }
    }
    else {
        err = "compressed BMP (compression " + std::to_string(compression) + "), only uncompressed BMP files are supported";
        return false;
    }

    if (width <= 0 || height <= 0) {
        err = "image has no width or no height";
        return false;
    }
    if ((uint64_t)width * (uint64_t)height > MAX_PIXELS) {
        err = "image is too large";
        return false;
    }

    // rows are padded to a multiple of 4 bytes. Allow the last row's padding to be missing.
    const uint64_t stride = (((uint64_t)width * bpp + 31u) / 32u) * 4u;
    const uint64_t rowbytes = (uint64_t)width * (bpp / 8u);
    if (data_ofs > f.size() || (f.size() - data_ofs) < (stride * (uint64_t)(height - 1) + rowbytes)) {
        err = "file is too short for the image";
        return false;
    }

    img.width = (unsigned int)width;
    img.height = (unsigned int)height;
    img.bpp = bpp;
    img.pixels.resize((size_t)width * (size_t)height);

    for (unsigned int y=0;y < img.height;y++) {
        const uint8_t *row = &f[data_ofs + stride * (topdown ? y : (img.height - 1u - y))];
        rgb_t *dst = &img.pixels[(size_t)y * img.width];

        if (bpp == 24u) {
            for (unsigned int x=0;x < img.width;x++,row += 3) {
                dst[x].b = row[0];
                dst[x].g = row[1];
                dst[x].r = row[2];
            }
        }
        else {
            for (unsigned int x=0;x < img.width;x++,row += 4) {
                const uint32_t px = le32(row);
                dst[x].r = channel_value(red,px);
                dst[x].g = channel_value(green,px);
                dst[x].b = channel_value(blue,px);
            }
        }
    }

    return true;
}

unsigned int bpp_for_colors(size_t colors) {
    if (colors <= 2u)
        return 1u;
    if (colors <= 16u)
        return 4u;

    return 8u;
}

bool write_bmp_paletted(const std::string &path,const paletted_image_t &img,unsigned int bpp,std::string &err) {
    const size_t colors = img.palette.size();
    if (colors == 0u || colors > 256u) {
        err = "palette must have 1 to 256 colors";
        return false;
    }

    if (bpp == 0u)
        bpp = bpp_for_colors(colors);
    if ((bpp != 1u && bpp != 4u && bpp != 8u) || colors > (1u << bpp)) {
        err = std::to_string(colors) + " colors do not fit in " + std::to_string(bpp) + " bits per pixel";
        return false;
    }

    // BITMAPFILEHEADER (14 bytes), BITMAPINFOHEADER (40 bytes), the palette, then the rows, bottom row first
    const uint64_t stride = (((uint64_t)img.width * bpp + 31u) / 32u) * 4u;
    const uint64_t pal_ofs = 14u + 40u;
    const uint64_t data_ofs = pal_ofs + (4u * colors);
    const uint64_t file_size = data_ofs + (stride * img.height);
    if (file_size > 0xFFFFFFFFul) {
        err = "image is too large for a BMP file";
        return false;
    }

    std::vector<uint8_t> f((size_t)file_size,0);
    f[0] = 'B';
    f[1] = 'M';
    put_le32(&f[2],(uint32_t)file_size);
    put_le32(&f[10],(uint32_t)data_ofs);

    put_le32(&f[14],40u);                           // biSize
    put_le32(&f[18],img.width);                     // biWidth
    put_le32(&f[22],img.height);                    // biHeight (positive, bottom row first)
    put_le16(&f[26],1u);                            // biPlanes
    put_le16(&f[28],bpp);                           // biBitCount
    put_le32(&f[30],BI_RGB);                        // biCompression
    put_le32(&f[34],(uint32_t)(stride * img.height));// biSizeImage
    put_le32(&f[46],(uint32_t)colors);              // biClrUsed

    for (size_t i=0;i < colors;i++) {
        uint8_t *p = &f[pal_ofs + (i * 4u)];
        p[0] = img.palette[i].b;
        p[1] = img.palette[i].g;
        p[2] = img.palette[i].r;
    }

    for (unsigned int y=0;y < img.height;y++) {
        uint8_t *row = &f[data_ofs + stride * (img.height - 1u - y)];
        const uint8_t *src = &img.pixels[(size_t)y * img.width];

        if (bpp == 8u) {
            memcpy(row,src,img.width);
        }
        else if (bpp == 4u) {
            for (unsigned int x=0;x < img.width;x++)
                row[x >> 1u] |= (uint8_t)(src[x] << ((x & 1u) ? 0u : 4u));
        }
        else {
            for (unsigned int x=0;x < img.width;x++)
                row[x >> 3u] |= (uint8_t)(src[x] << (7u - (x & 7u)));
        }
    }

    FILE *fp = fopen(path.c_str(),"wb");
    if (fp == NULL) {
        err = strerror(errno);
        return false;
    }

    const bool wrote = fwrite(&f[0],1,f.size(),fp) == f.size();
    if (fclose(fp) != 0 || !wrote) {
        err = "write error";
        remove(path.c_str());
        return false;
    }

    return true;
}

