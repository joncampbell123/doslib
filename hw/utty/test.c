
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/dos/dos.h>
#include <hw/utty/utty.h>

#if defined(TARGET_PC98)
// There is no background on PC-98, only foreground, and no intensity control either.
# define UTTY_COLOR_FG(c)               (PC98_TATTR_COLOR(fg))
# define UTTY_COLOR_BG(c)               (0)
# define UTTY_COLOR_FB(fg,bg)           (UTTY_COLOR_FG(fg))
# define UTTY_COLOR_INTENSITY_FG        (0u)
# define UTTY_COLOR_INTENSITY_BG        (0u)
# define UTTY_COLOR_MAKE_REVERSE(x)     ((x) | PC98_TATTR_REVERSE)
# define UTTY_COLOR_MAKE_BLINK(x)       ((x) | PC98_TATTR_BLINK)
# define UTTY_COLOR_MAKE_UNDERLINE(x)   ((x) | PC98_TATTR_UNDERLINE)
#else
// MDA/CGA/EGA/VGA/etc. have foreground in lower 4 bits, background in upper 4 bits.
// background may be limited to 7 colors (3 bits) when the 4th background bit is
// interpreted as "blink", foreground may be limited to 7 colors (3 bits) when the
// 4th foreground bit is interpreted as bit 8 of the character code.
//
// underline is available ONLY if the foreground color is blue and the background is
// black (MDA compat).
# define UTTY_COLOR_FG(c)               ((fg) & 0xFu)
# define UTTY_COLOR_BG(c)               (((bg) & 0xFu) << 4u)
# define UTTY_COLOR_FB(fg,bg)           (UTTY_COLOR_FG(fg) + UTTY_COLOR_BG(bg))
# define UTTY_COLOR_INTENSITY_FG        (0x08u)
# define UTTY_COLOR_INTENSITY_BG        (0x80u)
# define UTTY_COLOR_MAKE_REVERSE(x)     ((((x) & 0xF0u) >> 4u) + (((x) & 0x0Fu) << 4u))
# define UTTY_COLOR_MAKE_BLINK(x)       ((x) | 0x80u)
# define UTTY_COLOR_MAKE_UNDERLINE(x)   (((x) & 0x88u) | 0x01u)
#endif

int main(int argc,char **argv) {
	probe_dos();

#ifdef TARGET_PC98
	if (!probe_nec_pc98()) {
		printf("Sorry, your system is not PC-98\n");
		return 1;
	}
#else
	if (!probe_vga()) {
        printf("VGA probe failed\n");
		return 1;
	}
#endif

    if (!utty_init()) {
        printf("utty init fail\n");
        return 1;
    }
#ifdef TARGET_PC98
    if (!utty_init_pc98()) {
        printf("utty init vga fail\n");
        return 1;
    }
#else
    if (!utty_init_vgalib()) {
        printf("utty init vga fail\n");
        return 1;
    }
#endif

#if TARGET_MSDOS == 32
    printf("Alpha ptr: %p\n",utty_funcs.vram);
#else
    printf("Alpha ptr: %Fp\n",utty_funcs.vram);
#endif
    printf("Size: %u x %u (stride=%u)\n",utty_funcs.w,utty_funcs.h,utty_funcs.stride);

    {
        const char *msg = "Hello world";
        utty_offset_t o = utty_offset_getofs(4/*row*/,4/*col*/);
        UTTY_ALPHA_CHAR uch;
        unsigned char c;

#ifdef TARGET_PC98
        uch.f.at = 0x0041u;         // red
#else
        uch.f.at = 0x0Cu;           // bright red
#endif

        while ((c=(*msg++)) != 0) {
            uch.f.ch = c;
            o = utty_funcs.setchar(o,uch);
        }
    }

    {
        const char *msg = "Hello world";
        utty_offset_t o = utty_offset_getofs(5/*row*/,4/*col*/);
        UTTY_ALPHA_CHAR uach[12],uch;
        unsigned int i=0;
        unsigned char c;

#ifdef TARGET_PC98
        uch.f.at = 0x0081u;         // red
#else
        uch.f.at = 0x0Au;           // bright green
#endif

        while ((c=(*msg++)) != 0) {
            uch.f.ch = c;
            uach[i++] = uch;
        }
        assert(i < 12);
        utty_funcs.setcharblock(o,uach,i);
    }

    {
#ifdef TARGET_PC98
        const char *msg = "Hello world \x82\xb1\x82\xea\x82\xcd\x93\xfa\x96\x7b\x8c\xea\x82\xc5\x82\xb7";
#else
        const char *msg = "Hello world";
#endif
        utty_offset_t o = utty_offset_getofs(6/*row*/,4/*col*/);
        UTTY_ALPHA_CHAR uach[40],uch = UTTY_BLANK_CHAR;
        unsigned int i=0;

#ifdef TARGET_PC98
        uch.f.at = 0x0021u;         // blue
#else
        uch.f.at = 0x09u;           // bright blue
#endif

        i = utty_string2ac_const(uach,UTTY_ARRAY_LEN(uach),msg,uch);
        assert(i < 40);
        utty_funcs.setcharblock(o,uach,i);
    }

    {
#ifdef TARGET_PC98
        const char *msg = "Hello world \x82\xb1\x82\xea\x82\xcd\x93\xfa\x96\x7b\x8c\xea\x82\xc5\x82\xb7";
#else
        const char *msg = "Hello world";
#endif
        utty_offset_t o = utty_offset_getofs(7/*row*/,4/*col*/);
        UTTY_ALPHA_CHAR uach[UTTY_MAX_CHAR_WIDTH],uch = UTTY_BLANK_CHAR;
        const char *scan = msg,*scanfence = msg + strlen(msg);
        unsigned int i=0;

#ifdef TARGET_PC98
        uch.f.at = 0x00E1u;         // white
#else
        uch.f.at = 0x0Fu;           // white
#endif

        while (*scan != 0) {
            i = utty_string2ac(uach,UTTY_ARRAY_LEN(uach),&scan,uch);
            assert(i <= UTTY_MAX_CHAR_WIDTH);
            utty_funcs.setcharblock(o,uach,i);
            o = utty_offset_advance(o,i);
            assert(scan <= scanfence);
        }
    }

    {
#ifdef TARGET_PC98
        const char *msg = "Hello world \x82\xb1\x82\xea\x82\xcd\x93\xfa\x96\x7b\x8c\xea\x82\xc5\x82\xb7";
#else
        const char *msg = "Hello world";
#endif
        utty_offset_t o = utty_offset_getofs(8/*row*/,4/*col*/);
        UTTY_ALPHA_CHAR uch = UTTY_BLANK_CHAR;

#ifdef TARGET_PC98
        uch.f.at = 0x00C1u;         // yellow
#else
        uch.f.at = 0x0Eu;           // yellow
#endif

        utty_printat_const(o,msg,uch);
    }

    {
#ifdef TARGET_PC98
        const char *msg = "Hello world \x82\xb1\x82\xea\x82\xcd\x93\xfa\x96\x7b\x8c\xea\x82\xc5\x82\xb7";
#else
        const char *msg = "Hello world";
#endif
        utty_offset_t o = utty_offset_getofs(9/*row*/,4/*col*/),ostp;
        UTTY_ALPHA_CHAR uch = UTTY_BLANK_CHAR;

#ifdef TARGET_PC98
        uch.f.at = 0x00C5u;         // inverse yellow
#else
        uch.f.at = 0x60u;           // inverse yellow
#endif

        ostp = utty_offset_advance(o,32);
        o = utty_printat_const(o,msg,uch);
        while (o < ostp) o = utty_funcs.setchar(o,uch);
    }

    return 0;
}

