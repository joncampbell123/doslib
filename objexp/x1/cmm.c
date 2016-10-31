
/* does NOT use C runtime */

#include <conio.h>
#include <stdint.h>

const char message[] = "Hello world message";

unsigned int someval = 0x1234;
unsigned int bssval;

#define EXPORTPROC __cdecl far

const char far * EXPORTPROC get_message(void) {
    return message;
}

unsigned int EXPORTPROC callmemaybe(void) {
    return bssval=0x1234U;
}

unsigned int EXPORTPROC callmenever(void) {
    return someval + bssval;
}

unsigned int EXPORTPROC callmethis(unsigned int a,unsigned int b) {
    unsigned int c;

    c = a + b;
    return someval=(c - 0x55);
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

