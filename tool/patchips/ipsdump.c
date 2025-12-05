
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

static void help(void) {
    fprintf(stderr,"ipsdump <ips patch file>\n");
}

static int parse(int argc,char **argv) {
    struct stat st;
    int i,nosw=0;
    char *a;

    if (argc <= 1) {
        help();
        return 1;
    }

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

    return 0;
}

int main(int argc,char **argv) {
    unsigned char tmp[16],rle_byte;
    uint32_t offset,ipsoff;
    uint16_t len;
    int fd,rd,i;

    if (parse(argc,argv))
        return 1;

    fd = open(ips_file,O_RDONLY|O_BINARY);
    if (fd < 0)
        return 1;

    if (read(fd,tmp,5) != 5)
        return 1;
    if (memcmp(tmp,"PATCH",5))
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

        if (len != 0u) {
            printf("IPS@0x%lx: Patch at %lu(0x%lx), %u(0x%x) bytes:\n",
                (unsigned long)ipsoff,
                (unsigned long)offset,
                (unsigned long)offset,
                (unsigned int)len,
                (unsigned int)len);

            while (len >= 16u) {
                rd = read(fd,tmp,16);
                if (rd <= 0) return 1;

                printf("  0x%08lx:",(unsigned long)offset);
                for (i=0;i < rd;i++) printf(" %02X",tmp[i]);
                printf("\n");
                if (rd < 16) return 1;
                len -= (unsigned int)rd;
                offset += (unsigned long)rd;
            }

            if (len >= 16)
                return 1;

            while (len > 0u) {
                rd = read(fd,tmp,(int)len);
                if (rd <= 0) return 1;

                printf("  0x%08lx:",(unsigned long)offset);
                for (i=0;i < rd;i++) printf(" %02X",tmp[i]);
                printf("\n");
                if (rd < (int)len) return 1;
                len -= (unsigned int)rd;
                offset += (unsigned long)rd;
            }
        }
        else {
            if (read(fd,tmp,3) != 3) break;

            len =
                ((uint16_t)tmp[0] << (uint16_t) 8) +
                ((uint16_t)tmp[1] << (uint16_t) 0);
            rle_byte =
                tmp[2];

            printf("IPS@0x%lx: Patch at %lu(0x%lx), %u(0x%x) copies of byte %u(0x%x):\n",
                (unsigned long)ipsoff,
                (unsigned long)offset,
                (unsigned long)offset,
                (unsigned int)len,
                (unsigned int)len,
                (unsigned int)rle_byte,
                (unsigned int)rle_byte);
        }
    }

    close(fd);
    return 0;
}

