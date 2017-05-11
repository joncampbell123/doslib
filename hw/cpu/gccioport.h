
static inline uint8_t inp(const uint16_t port) {
    register uint8_t data;

    __asm__ __volatile__ ("inb %%dx,%%al" : /* outputs */ "=a" (data) : /* inputs */ "d" (port));

    return data;
}

static inline void outp(const uint16_t port,const uint8_t data) {
    __asm__ __volatile__ ("outb %%al,%%dx" : /* no output */ : /* inputs */ "d" (port), "a" (data));
}

static inline uint16_t inpw(const uint16_t port) {
    register uint16_t data;

    __asm__ __volatile__ ("inw %%dx,%%ax" : /* outputs */ "=a" (data) : /* inputs */ "d" (port));

    return data;
}

static inline void outpw(const uint16_t port,const uint16_t data) {
    __asm__ __volatile__ ("outw %%ax,%%dx" : /* no output */ : /* inputs */ "d" (port), "a" (data));
}

static inline uint32_t inpd(const uint16_t port) {
    register uint32_t data;

    __asm__ __volatile__ ("inl %%dx,%%eax" : /* outputs */ "=a" (data) : /* inputs */ "d" (port));

    return data;
}

static inline void outpd(const uint16_t port,const uint32_t data) {
    __asm__ __volatile__ ("outl %%eax,%%dx" : /* no output */ : /* inputs */ "d" (port), "a" (data));
}

