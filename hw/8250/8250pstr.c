 
#include <hw/8250/8250.h>

const char *type_8250_parity(unsigned char parity) {
    if (parity & 1) {
        switch (parity >> 1) {
            case 0: return "odd parity";
            case 1: return "even parity";
            case 2: return "odd sticky parity";
            case 3: return "even sticky parity";
        };
    }

    return "no parity";
}

/* vim: set tabstop=4 softtabstop=4 shiftwidth=4 expandtab */
