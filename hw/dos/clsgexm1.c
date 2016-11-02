
#if !defined(TARGET_OS2) && !defined(TARGET_WINDOWS) && TARGET_MSDOS == 16

/* does NOT use C runtime */

#include <conio.h>
#include <stdint.h>

#include <hw/dos/execlsg.h>

const char message[] = "Hello world message";

unsigned int someval = 0x1234;
unsigned int bssval;

const char far * CLSG_EXPORT_PROC get_message(void) {
    return message;
}

unsigned int CLSG_EXPORT_PROC get_value(void) {
    return someval + bssval;
}

void CLSG_EXPORT_PROC set_value1(const unsigned int v) {
    someval = v;
}

void CLSG_EXPORT_PROC set_value2(const unsigned int v) {
    bssval = v;
}

#else

# error this target is for 16-bit MS-DOS

#endif

