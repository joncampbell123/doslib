
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#include <ext/zlib/zlib.h>

/* PNG signature [https://www.w3.org/TR/PNG/#5PNG-file-signature] */
const uint8_t minipng_sig[8] = { 137u, 80u, 78u, 71u, 13u, 10u, 26u, 10u };

/* PNG chunk structure [https://www.w3.org/TR/PNG/#5DataRep]. This structure only holds the first two fields */
/* [length]                 4 bytes big endian
 * [chunk type]             4 bytes
 * [chunk data]             length bytes
 * [crc]                    4 bytes */
struct minipng_chunk {
    uint32_t                length;
    uint32_t                type;           /* compare against minipng_chunk_fourcc() */
};

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

int main(int argc,char **argv) {
    if (argc < 2) {
        fprintf(stderr,"Name the 8-bit paletted PNG file\n");
        return 1;
    }

    probe_dos();
    if (!probe_vga()) {
        printf("VGA probe failed\n");
        return 1;
    }
    int10_setmode(19);
    update_state_from_vga();

    getch();

    int10_setmode(3);
    return 0;
}

