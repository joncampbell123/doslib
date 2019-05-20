
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/dos/dos.h>
#include <hw/utty/utty.h>

struct utty_con_t               utty_con = {0};

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

