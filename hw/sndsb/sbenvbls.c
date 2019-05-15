
#include <dos.h>

#include <hw/sndsb/sndsb.h>

struct sndsb_ctx *sndsb_try_blaster_var(void) {
    signed char D=-1,H=-1,I=-1;
    struct sndsb_ctx *e;
    int A=-1,P=-1;
    char *s;

    if (sndsb_card_blaster != NULL)
        return sndsb_card_blaster;

    s = getenv("BLASTER");
    if (s == NULL) return NULL;

    while (*s != 0) {
        if (*s == ' ') {
            s++;
            continue;
        }

        if (*s == 'A')
            A = strtol(++s,&s,16);
        else if (*s == 'P')
            P = strtol(++s,&s,16);
        else if (*s == 'I')
            I = strtol(++s,&s,10);
        else if (*s == 'D')
            D = strtol(++s,&s,10);
#if !defined(TARGET_PC98)
        else if (*s == 'H')
            H = strtol(++s,&s,10);
#endif
        else {
            while (*s && *s != ' ') s++;
        }
    }

    if (A < 0 || I < 0 || D < 0 || I > 15 || D > 7)
        return NULL;

#if defined(TARGET_PC98)
    H = D;
#endif

    if (sndsb_by_base(A) != NULL)
        return 0;

    e = sndsb_alloc_card(); /* init and free functions zero this struct */
    if (e == NULL) return NULL;
    e->baseio = (uint16_t)A;
    e->mpuio = (uint16_t)(P > 0 ? P : 0);
    e->dma8 = (int8_t)D;
    e->dma16 = (int8_t)H;
    e->irq = (int8_t)I;
    return (sndsb_card_blaster=e);
}

