
#include <stdint.h>

#include <ext/zlib/zlib.h>

inline uint32_t minipng_byteswap32(uint32_t t) {
    t = ((t & 0xFF00FF00UL) >>  8ul) + ((t & 0x00FF00FFUL) <<  8ul);
    t = ((t & 0xFFFF0000UL) >> 16ul) + ((t & 0x0000FFFFUL) << 16ul);
    return t;
}

/* this code is intended for DOS applications and little endian platforms */
#define minipng_chunk_fourcc_shf(a,s)       ( ((uint32_t)((unsigned char)(a))) << ((uint32_t)(s)) )
#define minipng_chunk_fourcc(a,b,c,d)       ( minipng_chunk_fourcc_shf(a,0) + minipng_chunk_fourcc_shf(b,8) + minipng_chunk_fourcc_shf(c,16) + minipng_chunk_fourcc_shf(d,24) )

/* PNG IHDR [https://www.w3.org/TR/PNG/#11IHDR]. Fields are big endian. */
#pragma pack(push,1)
struct minipng_IHDR {
    uint32_t        width;                  /* +0x00 */
    uint32_t        height;                 /* +0x04 */
    uint8_t         bit_depth;              /* +0x08 */
    uint8_t         color_type;             /* +0x09 */
    uint8_t         compression_method;     /* +0x0A */
    uint8_t         filter_method;          /* +0x0B */
    uint8_t         interlace_method;       /* +0x0C */
};                                          /* =0x0D */
#pragma pack(pop)

/* PNG PLTE [https://www.w3.org/TR/PNG/#11PLTE] */
#pragma pack(push,1)
struct minipng_PLTE_color {
    uint8_t         red,green,blue;         /* +0x00,+0x01,+0x02 */
};                                          /* =0x03 */
#pragma pack(pop)

/* PNG chunk structure [https://www.w3.org/TR/PNG/#5DataRep]. This structure only holds the first two fields */
/* [length]                 4 bytes big endian
 * [chunk type]             4 bytes
 * [chunk data]             length bytes
 * [crc]                    4 bytes */
struct minipng_chunk {
    uint32_t                length;
    uint32_t                type;           /* compare against minipng_chunk_fourcc() */
};

struct minipng_reader {
    char*                   err_msg;
    off_t                   chunk_data_offset;
    off_t                   next_chunk_start;
    struct minipng_chunk    chunk_data_header;
    int                     fd;

    struct minipng_IHDR     ihdr;

    struct minipng_PLTE_color*  plte;
    unsigned int                plte_count;

    unsigned char*              trns;
    size_t                      trns_size;

    uint32_t                    idat_rem;
    unsigned char*              compr;
    size_t                      compr_size;
    z_stream                    compr_zlib;

    unsigned int                ungetch;
};

extern const uint8_t minipng_sig[8];

/* WARNING: This function will expand bytes to a multiple of 8 pixels rounded up. Allocate your buffer accordingly. */
void minipng_expand1to8(unsigned char *buf,unsigned int pixels);
struct minipng_reader *minipng_reader_open(const char *path);
int minipng_reader_rewind(struct minipng_reader *rdr);
int minipng_reader_next_chunk(struct minipng_reader *rdr);
void minipng_reader_close(struct minipng_reader **rdr);
int minipng_reader_parse_head(struct minipng_reader *rdr);
int minipng_reader_read_idat(struct minipng_reader *rdr,unsigned char FAR *dst,size_t count);
size_t minipng_rowsize_bytes(struct minipng_reader *rdr);

