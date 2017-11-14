
#include <string.h>

#include <hw/8251/8251.h>

static unsigned char inited_8251 = 0;

int init_8251() {
    if (!inited_8251) {
        inited_8251 = 1;
    }

    return 1;
}

/* vim: set tabstop=4 softtabstop=4 shiftwidth=4 expandtab */
