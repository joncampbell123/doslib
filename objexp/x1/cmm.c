
/* does NOT use C runtime */

#include <conio.h>
#include <stdint.h>

unsigned int __cdecl callmemaybe(void) {
    return 0x1234U;
}

unsigned int __cdecl callmenever(void) {
    return 0xBADDU;
}

unsigned int __cdecl callmethis(unsigned int a,unsigned int b) {
    unsigned int c = a + b;
    return c - 0x55;
}

unsigned char __cdecl readtimer(void) {
    return inp(0x43);
}

unsigned char __cdecl readvga(void) {
    return inp(0x3DA);
}

unsigned char __cdecl readaport(unsigned int p) {
    return inp(p);
}

