
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/dos/dos.h>
#include <hw/utty/utty.h>

utty_offset_t utty_printat(utty_offset_t o,const char **msg,UTTY_ALPHA_CHAR uch) {
    unsigned int i;

    while (**msg != 0) {
        i = utty_string2ac(utty_tmp,UTTY_ARRAY_LEN(utty_tmp),msg,uch);
        assert(i <= UTTY_ARRAY_LEN(utty_tmp));
        o = utty_funcs.setcharblock(o,utty_tmp,i);
    }

    return o;
}

utty_offset_t utty_printat_const(utty_offset_t o,const char *msg,UTTY_ALPHA_CHAR uch) {
    return utty_printat(o,&msg,uch);
}

