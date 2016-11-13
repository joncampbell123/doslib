#ifndef TARGET_OS2
# error This is OS-2 code, not DOS or Windows
#endif

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <i86.h>
#include <dos.h>

int main(int argc,char **argv) {
    printf("Hello OS/2\n");
    return 0;
}

