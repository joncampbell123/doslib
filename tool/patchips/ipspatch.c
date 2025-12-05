
#include <sys/types.h>
#include <sys/stat.h>

#include <time.h>
#include <stdint.h>
#include <endian.h>
#include <dirent.h>
#include <strings.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>

#if defined(TARGET_MSDOS)
#include <dos.h>
#include <conio.h>
#endif

#ifndef O_BINARY
#define O_BINARY (0)
#endif

char*                   ips_file = NULL;
char*                   to_file = NULL;

static void help(void) {
    fprintf(stderr,"ipspatch <ips patch file> <file to patch>\n");
}

static int parse(int argc,char **argv) {
    struct stat st;
    int i,nosw=0;
    char *a;

    for (i=1;i < argc;) {
        a = argv[i++];

        if (*a == '-') {
            do { a++; } while (*a == '-');

            if (!strcmp(a,"h") || !strcmp(a,"help")) {
                help();
                return 1;
            }
            else {
                fprintf(stderr,"Unknown switch %s\n",a);
                return 1;
            }
        }
        else {
            switch (nosw++) {
                case 0:
                    ips_file = a;
                    break;
                case 1:
                    to_file = a;
                    break;
                default:
                    fprintf(stderr,"Unexpected arg\n");
                    return 1;
            }
        }
    }

    if (!ips_file) {
        fprintf(stderr,"IPS file required\n");
        return 1;
    }
    if (stat(ips_file,&st)) {
        fprintf(stderr,"Cannot stat IPS file\n");
        return 1;
    }
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr,"Not a file\n");
        return 1;
    }

    if (!to_file) {
        fprintf(stderr,"File to patch required\n");
        return 1;
    }
    if (stat(to_file,&st)) {
        fprintf(stderr,"Cannot stat file to patch\n");
        return 1;
    }
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr,"Not a file\n");
        return 1;
    }

    return 0;
}

static unsigned char tmp[4096];

int main(int argc,char **argv) {
    unsigned char rle_byte;
    uint32_t offset;
    uint32_t ipsoff;
    uint16_t len;
    int out_fd;
    int fd,rd;

    (void)ipsoff;

    if (parse(argc,argv))
        return 1;

    fd = open(ips_file,O_RDONLY|O_BINARY);
    if (fd < 0)
        return 1;

    if (read(fd,tmp,5) != 5)
        return 1;
    if (memcmp(tmp,"PATCH",5))
        return 1;

    out_fd = open(to_file,O_RDWR|O_BINARY);
    if (out_fd < 0)
        return 1;

    while (1) {
        ipsoff = (uint32_t)lseek(fd,0,SEEK_CUR);
        rd = read(fd,tmp,5);
        if (rd == 3) {
            if (memcmp(tmp,"EOF",3))
                fprintf(stderr,"No EOF signature at end\n");

            break;
        }
        else if (rd != 5) {
            break;
        }

        /* offset          24-bit big endian
         * size            16-bit big endian
         * data            unsigned char[size]
         *
         * or
         *
         * offset          24-bit big endian
         * 0               16-bit big endian
         * RLE length      16-bit big endian
         * RLE byte        unsigned char */
        offset =
            ((uint32_t)tmp[0] << (uint32_t)16) +
            ((uint32_t)tmp[1] << (uint32_t) 8) +
            ((uint32_t)tmp[2] << (uint32_t) 0);
        len =
            ((uint16_t)tmp[3] << (uint16_t) 8) +
            ((uint16_t)tmp[4] << (uint16_t) 0);

        if (lseek(out_fd,(off_t)offset,SEEK_SET) != (off_t)offset)
            return 1;

//      printf("IPS@0x%lx\n",(unsigned long)ipsoff);

        if (len != 0u) {
            while (len >= (unsigned long)sizeof(tmp)) {
                rd = read(fd,tmp,sizeof(tmp));
                if (rd != (int)sizeof(tmp)) return 1;
                rd = write(out_fd,tmp,sizeof(tmp));
                if (rd != (int)sizeof(tmp)) return 1;
                len -= (unsigned int)rd;
            }

            if (len >= (unsigned long)sizeof(tmp))
                return 1;

            if (len > 0u) {
                rd = read(fd,tmp,(int)len);
                if (rd != (int)len) return 1;
                rd = write(out_fd,tmp,(int)len);
                if (rd != (int)len) return 1;
                len -= (unsigned int)rd;
            }

            if (len != 0u)
                return 1;
        }
        else {
            if (read(fd,tmp,3) != 3) break;

            len =
                ((uint16_t)tmp[0] << (uint16_t) 8) +
                ((uint16_t)tmp[1] << (uint16_t) 0);
            rle_byte =
                tmp[2];

            memset(tmp,rle_byte,sizeof(tmp));
            while (len >= (unsigned long)sizeof(tmp)) {
                rd = write(out_fd,tmp,sizeof(tmp));
                if (rd != (int)sizeof(tmp)) return 1;
                len -= (unsigned int)rd;
            }

            if (len >= (unsigned long)sizeof(tmp))
                return 1;

            if (len > 0u) {
                rd = write(out_fd,tmp,(int)len);
                if (rd != (int)len) return 1;
                len -= (unsigned int)rd;
            }

            if (len != 0u)
                return 1;
        }
    }

    close(fd);
    return 0;
}

