
#include <stdint.h>

#pragma pack(push,1)
struct font_bmp_chardef {
/* char id=32   x=178   y=65    width=3     height=1     xoffset=-1    yoffset=15    xadvance=4     page=0  chnl=15 */
    uint16_t        id;             /* unicode code value */
    uint8_t         x,y;            /* starting x and y (upper left corner). These images are 256x256 so it's fine to use uint8_t */
    uint8_t         w,h;            /* width and height at x,y */
    int8_t          xoffset,yoffset;/* offset from draw position to draw */
    int8_t          xadvance;       /* how many to advance */
    /* ignored: page */
    /* ignored: chnl */
};
#pragma pack(pop)

#pragma pack(push,1)
struct font_bmp_kerndef {
/* kerning first=32  second=65  amount=-1 */
    uint16_t        first;
    uint16_t        second;
    int8_t          amount;
};
#pragma pack(pop)

struct font_bmp {
    unsigned char*                  fontbmp;        /* 1-bit font */
    unsigned int                    stride;         /* bytes per row */
    unsigned int                    height;         /* height of bitmap */
    unsigned int                    chardef_count;  /* number of chardefs */
    unsigned int                    kerndef_count;  /* number of kerndefs */
    struct font_bmp_chardef*        chardef;        /* array of chardef */
    struct font_bmp_kerndef*        kerndef;        /* array of kerndef */
};

unsigned int font_bmp_draw_chardef_vga8u(struct font_bmp *fnt,unsigned int cdef,unsigned int x,unsigned int y,unsigned char color);

int font_bmp_unicode_to_chardef(struct font_bmp *fnt,uint32_t c);
int font_bmp_do_load(struct font_bmp **fnt,const char *path);
struct font_bmp *font_bmp_load(const char *path);
void font_bmp_free(struct font_bmp **fnt);

