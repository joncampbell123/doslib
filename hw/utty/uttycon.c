
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/dos/dos.h>
#include <hw/utty/utty.h>

struct utty_con_t               utty_con = { .refch = UTTY_BLANK_CHATTR };

void utty_con_cr(void) {
    utty_con.x = utty_con.left;
}

void utty_con_lf(void) {
    if (utty_con.y < utty_con.bottom)
        utty_con.y++;
    // TODO scrolling
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
            if (ch.f.ch == '\n' || utty_con.x > utty_con.right) {
                utty_funcs.setcharblock(o,utty_tmp+js,j-js);
                utty_con_cr();
                utty_con_lf();
                o = utty_con_to_offset();
                js = ++j; // start from next
            }
            else if (ch.f.ch == '\r') {
                js = ++j; // start from next
            }
            else { // NTS: On PC-98 encodings, if double-byte encoding, this should be the decomposed doublewide, copy as-is
                utty_con.x++;
                ++j;
            }
        }

        if (js < j) {
            i = utty_funcs.setcharblock(o,utty_tmp+js,j-js);
            o = utty_offset_advance(o,i);
        }
    } while (1);
}

void utty_con_home(void) {
    utty_con.x = utty_con.left;
    utty_con.y = utty_con.top;
}

void utty_con_xadj(const int8_t x) {
    const uint8_t nx = x + utty_con.x;
    if (nx < utty_con.left || (nx & (~0x7Fu)/*became negative*/))
        utty_con.x = utty_con.left;
    else if (nx > utty_con.right)
        utty_con.x = utty_con.right;
    else
        utty_con.x = (uint8_t)x;
}

void utty_con_yadj(const int8_t y) {
    const uint8_t ny = y + utty_con.y;
    if (ny < utty_con.top || (ny & (~0x7Fu)/*became negative*/))
        utty_con.y = utty_con.top;
    else if (ny > utty_con.bottom)
        utty_con.y = utty_con.bottom;
    else
        utty_con.y = (uint8_t)y;
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

