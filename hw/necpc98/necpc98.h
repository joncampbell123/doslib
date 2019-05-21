
#ifndef __HW_NECPC98_NECPC98_H
#define __HW_NECPC98_NECPC98_H

#pragma pack(push,1)
typedef union {
    struct {                            /* 0x00-0x0F formatted display shortcut */
        char        dmark;              /* 0x00-0x00 0xFE if disp text */
        char        disp[5];            /* 0x01-0x05 display text */
        char        skey[10];           /* 0x06-0x0F raw shortcut */
    } d;
    char            skey[16];           /* 0x00-0x0F raw shortcut */
} nec_pc98_intdc_funckey;

typedef struct {
    char            skey[6];            /* 0x00-0x05 raw shortcut */
} nec_pc98_intdc_editkey;

// Key state, INT DCh CL=0Ch/CL=0Dh AX=00h
typedef struct {
    nec_pc98_intdc_funckey          func[10];           /* F1-F10 */
    nec_pc98_intdc_funckey          shift_func[10];     /* shift + F1-F10 */
    nec_pc98_intdc_editkey          editkey[11];        /* editor keys */
} nec_pc98_intdc_keycode_state;

// Key state, INT DCh CL=0Ch/CL=0Dh AX=FFh
typedef struct {
    nec_pc98_intdc_funckey          func[10];           /* F1-F10 */
    nec_pc98_intdc_funckey          vfunc[5];           /* VF1-VF5 */
    nec_pc98_intdc_funckey          shift_func[10];     /* shift + F1-F10 */
    nec_pc98_intdc_funckey          shift_vfunc[5];     /* shift + VF1-VF10 */
    nec_pc98_intdc_editkey          editkey[11];        /* editor keys */
    nec_pc98_intdc_funckey          ctrl_func[10];      /* control + F1-F10 */
    nec_pc98_intdc_funckey          ctrl_vfunc[5];      /* control + VF1-VF5 */
} nec_pc98_intdc_keycode_state_ext;
#pragma pack(pop)

// indices into intdc_state editkey[]
enum pc98_intdc_editkey {
    PC98_INTDC_EK_ROLL_UP=0,
    PC98_INTDC_EK_ROLL_DOWN=1,
    PC98_INTDC_EK_INS=2,
    PC98_INTDC_EK_DEL=3,
    PC98_INTDC_EK_UP_ARROW=4,
    PC98_INTDC_EK_LEFT_ARROW=5,
    PC98_INTDC_EK_RIGHT_ARROW=6,
    PC98_INTDC_EK_DOWN_ARROW=7,
    PC98_INTDC_EK_HOME_CLR=8,
    PC98_INTDC_EK_HELP=9,
    PC98_INTDC_EK_KEYPAD_MINUS=10
};

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

