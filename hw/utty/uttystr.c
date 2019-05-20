
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/dos/dos.h>
#include <hw/utty/utty.h>

unsigned int utty_string2ac(UTTY_ALPHA_CHAR *dst,unsigned int dst_max,const char **msgp,UTTY_ALPHA_CHAR refch) {
#ifdef TARGET_PC98
    struct ShiftJISDecoder sjis;
    unsigned char c;
#endif
    const char *msg = *msgp;
    unsigned int r = 0;

    while (r < dst_max) {
#ifdef TARGET_PC98
        c = msg[0];
        if (c == 0) break;

        if (pc98_sjis_dec1(&sjis,c)) { // double byte char. either advance msg for the entire char or do not advance at all and exit
            if ((r+2) > dst_max) break;

            c = msg[1];
            if (c != 0) {
                msg += 2;

                if (pc98_sjis_dec2(&sjis,c)) {
                    refch.f.ch = (sjis.b1 - 0x20u) + (sjis.b2 << 8u);
                    *(dst++) = refch;
                    r++;
                    *(dst++) = refch;
                    r++;
                }
            }
            else {
                msg++; // msg[1] == 0 therefore stop at NUL
                assert(*msg == 0);
                break;
            }
        }
        else {
            refch.f.ch = c;
            *(dst++) = refch;
            msg++; // single byte char
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

