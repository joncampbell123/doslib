
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <conio.h>
#include <bios.h>
#include <dos.h>
#include <i86.h>    /* for MK_FP */

int main(int argc,char **argv) {
    unsigned long long count=0;
    char *zeros;
    size_t len;
    FILE *fp;

    fp = fopen("_zero_.bin","ab+");
    if (fp == NULL) fp = fopen("_zero_.bin","wb");
    if (fp == NULL) return 1;

    len = 32768;
    zeros = (char*)malloc(len);
    if (zeros == NULL) return 2;
    memset(zeros,0,len);

    while (1) {
        count += (unsigned long long)fwrite(zeros,1,len,fp);
        printf("\x0D" "%llu   ",count);
        fflush(stdout);
    }

#if 0 // unreachable due to while(1)
    free(zeros);
    fclose(fp);
#endif
    return 0;
}

