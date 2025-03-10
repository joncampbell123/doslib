
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/dos/dos.h>
#include <hw/utty/utty.h>

struct utty_con_t               utty_con = { .refch = UTTY_BLANK_CHATTR };

void utty_con_lf(void) {
    if (utty_con.y < utty_con.bottom)
        utty_con.y++;
    else if (!(utty_con.flags & UTTY_CON_FLAG_NOSCROLL)) {
        utty_funcs.scroll(
            /*dofs*/utty_offset_getofs(utty_con.top   ,utty_con.left),
            /*sofs*/utty_offset_getofs(utty_con.top+1u,utty_con.left),
            /*w*/utty_con.right - utty_con.left + 1u,
            /*h*/utty_con.bottom - utty_con.top);
        utty_funcs.fill(
            /*ofs*/utty_offset_getofs(utty_con.bottom,utty_con.left),
            /*count*/utty_con.right - utty_con.left + 1u,
            /*ch*/utty_con.refch);
    }
}

void utty_con_write(const char *msg) {
    utty_offset_t o;
    unsigned int i,j,js;

    o = utty_con_to_offset();
    do {
        i = utty_string2ac(utty_tmp,UTTY_ARRAY_LEN(utty_tmp),&msg,utty_con.refch);
        if (i == 0) break; // assume it's *msg == 0

        js = 0;
        for (j=0;j < i;) {
            const UTTY_ALPHA_CHAR ch = utty_tmp[j];

            // control chars should be passed through as-is.
            // also if we're about to print outside the box on the right, wrap around.
            // TODO: Or in that case, just overprint on the side if NOWRAP
            if (ch.f.ch == '\n') {
                utty_funcs.setcharblock(o,utty_tmp+js,j-js);
                utty_con_cr();
                utty_con_lf();
                o = utty_con_to_offset();
                js = ++j; // start from next
            }
            else if (ch.f.ch == '\r') {
                js = ++j; // start from next
            }
            else {
#if defined(TARGET_PC98)
                const unsigned int cw = vram_pc98_doublewide(ch.f.ch) ? 2u : 1u;
#else
                const unsigned int cw = 1u;
#endif

                if ((cw+utty_con.x-1u) > utty_con.right) {
                    o = utty_funcs.setcharblock(o,utty_tmp+js,j-js);
                    if (cw != 1u) utty_funcs.setchar(o,utty_con.refch);

                    utty_con_cr();
                    utty_con_lf();
                    o = utty_con_to_offset();
                    js = j; // restart from current
                }
                else {
                    // NTS: On PC-98 encodings, if double-byte encoding, this should be the decomposed doublewide, copy as-is.
                    //      To allow wraparound to work, advance by character width. string2ac upon DBCS encoding will either
                    //      write two of the same char (for VRAM) or will not write anything at all (i.e. not enough space).
                    utty_con.x += cw;
                    j += cw;
                }
            }
        }

        if (js < j)
            o = utty_funcs.setcharblock(o,utty_tmp+js,j-js);
    } while (1);
}

void utty_con_home(void) {
    utty_con.x = utty_con.left;
    utty_con.y = utty_con.top;
}

void utty_con_poscurs(const uint8_t y,const uint8_t x) {
    utty_con.x = x + utty_con.left;
    utty_con.y = y + utty_con.top;
    if (utty_con.x > utty_con.right)
        utty_con.x = utty_con.right;
    if (utty_con.y > utty_con.bottom)
        utty_con.y = utty_con.bottom;
}

void utty_con_update_from_dev(void) {
    utty_con.top = 0;
    utty_con.left = 0;
    utty_con.right = utty_funcs.w - 1u;
    utty_con.bottom = utty_funcs.h - 1u;
    utty_con_home();
}

utty_offset_t utty_con_to_offset(void) {
    return utty_con_to_offset_inline();
}

/* must call after utty_init() and utty driver init */
int utty_con_init(void) {
    utty_con_update_from_dev();
    return 1;
}

