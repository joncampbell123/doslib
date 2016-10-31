
/* does NOT use C runtime */

#include <conio.h>
#include <stdint.h>

#define EXPORTPROC __cdecl far

unsigned int EXPORTPROC callmemaybe(void) {
    return 0x1234U;
}

unsigned int EXPORTPROC callmenever(void) {
    return 0xBADDU;
}

unsigned int EXPORTPROC callmethis(unsigned int a,unsigned int b) {
    unsigned int c = a + b;
    return c - 0x55;
}

unsigned char EXPORTPROC readtimer(void) {
    return inp(0x43);
}

unsigned char EXPORTPROC readvga(void) {
    return inp(0x3DA);
}

unsigned char EXPORTPROC readaport(unsigned int p) {
    return inp(p);
}

