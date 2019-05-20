
#ifndef __HW_NECPC98_NECPC98_H
#define __HW_NECPC98_NECPC98_H

#pragma pack(push,1)
struct nec_pc98_state_t {
	uint8_t			probed:1;
	uint8_t			is_pc98:1;
};
#pragma pack(pop)

#pragma pack(push,1)
struct ShiftJISDecoder {
    unsigned char       b1,b2;
};
#pragma pack(pop)

#define PC98_TATTR_NOTSECRET            (1u << 0u)
#define PC98_TATTR_BLINK                (1u << 1u)
#define PC98_TATTR_REVERSE              (1u << 2u)
#define PC98_TATTR_UNDERLINE            (1u << 3u)
#define PC98_TATTR_VLINE                (1u << 4u)
#define PC98_TATTR_SGRAPHICS            (1u << 4u)
#define PC98_TATTR_COLOR(x)             (((x) & 7u) << 5u)

extern struct nec_pc98_state_t		nec_pc98;

int probe_nec_pc98();

unsigned int pc98_sjis_dec1(struct ShiftJISDecoder *j,const unsigned char c);
unsigned int pc98_sjis_dec2(struct ShiftJISDecoder *j,const unsigned char c);

static inline unsigned char vram_pc98_doublewide(const uint16_t chcode) {
    if (chcode & 0xFF00u) {
        if ((chcode & 0x7Cu) != 0x08u)
            return 1;
    }

    return 0;
}

#endif //__HW_NECPC98_NECPC98_H

