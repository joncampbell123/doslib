
#define KBDOWN_MAXBITS          (0x102u)
#define KBDOWN_BYTES            ((KBDOWN_MAXBITS+7u)>>3u)

#define KBD_BUF_SIZE            16u/*must be power of 2*/
#define KBD_BUF_MASK            (KBD_BUF_SIZE - 1u)

/* keyboard controller looks/acts like AT keyboard */
#define KBDF_AT                 (1u << 0u)
/* keyboard controller looks/acts like XT keyboard */
#define KBDF_XT                 (1u << 1u)

/* keyboard codes in this game. These are XT compatible codes.
 * AT extended codes (0xE0 <xx>) are 0x80+(xx&0x7F). Have fun. */
enum {
    KBDS_ESCAPE=                0x001u,
    KBDS_LCTRL=                 0x01Du,
    KBDS_LSHIFT=                0x02Au,
    KBDS_RSHIFT=                0x036u,
    KBDS_LALT=                  0x038u,
    KBDS_SPACEBAR=              0x039u,
    KBDS_KEYPAD_8=              0x048u, /* up arrow if not numlock and compatible with XT */
    KBDS_KEYPAD_4=              0x04Bu, /* left arrow if not numlock and compatible with XT */
    KBDS_KEYPAD_6=              0x04Du, /* right arrow if not numlock and compatible with XT */
    KBDS_KEYPAD_2=              0x050u, /* down arrow if not numlock and compatible with XT */
    KBDS_RCTRL=                 0x09Du,
    KBDS_RALT=                  0x0B8u,
    KBDS_UP_ARROW=              0x0C8u,
    KBDS_LEFT_ARROW=            0x0CBu,
    KBDS_RIGHT_ARROW=           0x0CDu,
    KBDS_DOWN_ARROW=            0x0D0u,
    KBDS_PAUSE=                 0x100u,
    KBDS_UNKNOWN=               0x101u
};

extern unsigned char            kbdown[KBDOWN_BYTES]; /* bitfield */
extern unsigned char            kbd_flags;

static inline void kbdown_clear(const unsigned int k) {
    kbdown[k>>3u] &= ~(1u << (k & 7u));
}

static inline void kbdown_set(const unsigned int k) {
    kbdown[k>>3u] |= (1u << (k & 7u));
}

static inline unsigned int kbdown_test(const unsigned int k) {
    return !!(kbdown[k>>3u] & (1u << (k & 7u)));
}

int kbd_buf_read(void);
void detect_keyboard();
void init_keyboard_irq();
void restore_keyboard_irq();

