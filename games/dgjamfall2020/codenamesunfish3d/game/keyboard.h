
#define KBDOWN_MAXBITS          (0x102u)
#define KBDOWN_BYTES            ((KBDOWN_MAXBITS+7u)>>3u)

#define KBD_BUF_SIZE            16u/*must be power of 2*/
#define KBD_BUF_MASK            (KBD_BUF_SIZE - 1u)

/* keyboard controller looks/acts like AT keyboard */
#define KBDF_AT                 (1u << 0u)
/* keyboard controller looks/acts like XT keyboard */
#define KBDF_XT                 (1u << 1u)

extern unsigned char            kbdown[KBDOWN_BYTES]; /* bitfield */
extern unsigned char            kbd_flags;

static inline void kbdown_clear(const unsigned int k) {
    kbdown[k>>3u] &= ~(1u << (k & 7u));
}

static inline void kbdown_set(const unsigned int k) {
    kbdown[k>>3u] |= (1u << (k & 7u));
}

int kbd_buf_read(void);
void detect_keyboard();
void init_keyboard_irq();
void restore_keyboard_irq();

