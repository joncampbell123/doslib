
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/dos/dos.h>
#include <hw/utty/utty.h>

struct utty_funcs_t                 utty_funcs;

int utty_init(void) {
    return 1;
}

unsigned int utty_string2ac(UTTY_ALPHA_CHAR *dst,unsigned int dst_max,const char **msgp,UTTY_ALPHA_CHAR refch) {
#ifdef TARGET_PC98
    struct ShiftJISDecoder sjis;
    unsigned char c;
#endif
    const char *msg = *msgp;
    unsigned int r = 0;

    while (r < dst_max) {
#ifdef TARGET_PC98
        c = *msg;
        if (c == 0) break;
        msg++;

        if (pc98_sjis_dec1(&sjis,c)) {
            if ((r+2) > dst_max) break;

            c = *msg;
            if (c == 0) break;
            msg++;

            pc98_sjis_dec2(&sjis,c);

            refch.f.ch = (sjis.b1 - 0x20u) + (sjis.b2 << 8u);
            *(dst++) = refch;
            r++;
            *(dst++) = refch;
            r++;
        }
        else { 
            refch.f.ch = c;
            *(dst++) = refch;
            r++;
        }
#else
        refch.f.ch = *msg;
        if (refch.f.ch == 0) break;
        msg++;

        *(dst++) = refch;
        r++;
#endif
    }

    *msgp = msg;
    return r;
}

unsigned int utty_string2ac_const(UTTY_ALPHA_CHAR *dst,unsigned int dst_max,const char *msg,UTTY_ALPHA_CHAR refch) {
    return utty_string2ac(dst,dst_max,&msg,refch);
}

