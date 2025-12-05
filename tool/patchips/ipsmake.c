
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
char*                   from_file = NULL;
char*                   to_file = NULL;

static void help(void) {
    fprintf(stderr,"ipsmake <from file> <to file> <output IPS file>\n");
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
                    from_file = a;
                    break;
                case 1:
                    to_file = a;
                    break;
                case 2:
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

    if (!from_file) {
        fprintf(stderr,"From file required\n");
        return 1;
    }
    if (stat(from_file,&st)) {
        fprintf(stderr,"Cannot stat from file\n");
        return 1;
    }
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr,"Not a file\n");
        return 1;
    }

    if (!to_file) {
        fprintf(stderr,"To file required\n");
        return 1;
    }
    if (stat(to_file,&st)) {
        fprintf(stderr,"cannot stat to file\n");
        return 1;
    }
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr,"Not a file\n");
        return 1;
    }

    return 0;
}

#if TARGET_MSDOS == 32
# define CMPSZ (256u*1024u)
#else
# define CMPSZ 4096u
#endif

static unsigned char fbuf[CMPSZ];
static unsigned char tbuf[CMPSZ];
static unsigned char obuf[CMPSZ];
static unsigned char hbuf[16];
static unsigned char obufdb=0;
static unsigned long obufpos=0;
static unsigned int obufp=0;

int flush_obuf(int fd) {
    if (obufp != 0u) {
#if 1//CHECK
        if (obufdb) {
            unsigned int i;

            for (i=1;i < obufp;i++) {
                if (obuf[0] != obuf[i]) {
                    fprintf(stderr,"Run detect fail\n");
                    abort();
                }
            }
        }
#endif

        hbuf[0] = (unsigned char)(obufpos >> 16ul);
        hbuf[1] = (unsigned char)(obufpos >>  8ul);
        hbuf[2] = (unsigned char)(obufpos        );

        if (obufdb && obufp >= 4u) {
            hbuf[3] = 0;
            hbuf[4] = 0;
            hbuf[5] = (unsigned char)(obufp >> 8u);
            hbuf[6] = (unsigned char)(obufp      );
            hbuf[7] = obuf[0];
            if (write(fd,hbuf,8) != 8) return -1;
        }
        else {
            hbuf[3] = (unsigned char)(obufp >> 8u);
            hbuf[4] = (unsigned char)(obufp      );
            if (write(fd,hbuf,5) != 5) return -1;
            if (write(fd,obuf,obufp) != obufp) return -1;
        }

        obufpos = 0;
        obufdb = 0;
        obufp = 0;
    }

    return 0;
}

int main(int argc,char **argv) {
    int f_fd,t_fd,ips_fd,frd,trd,prd;
    unsigned long pos=0;
    unsigned int i;

    if (parse(argc,argv))
        return 1;

    f_fd = open(from_file,O_RDONLY|O_BINARY);
    if (f_fd < 0)
        return 1;

    t_fd = open(to_file,O_RDONLY|O_BINARY);
    if (t_fd < 0)
        return 1;

    ips_fd = open(ips_file,O_WRONLY|O_BINARY|O_CREAT|O_TRUNC,0644);
    if (ips_fd < 0)
        return 1;

    if (write(ips_fd,"PATCH",5) != 5)
        return 1;

    while (1) {
        frd = read(f_fd,fbuf,CMPSZ); if (frd < 0) frd = 0;
        trd = read(t_fd,tbuf,CMPSZ); if (trd < 0) trd = 0;
        prd = frd; if (prd < trd) prd = trd;
        if (prd == 0) break;

        for (i=0;i < (unsigned int)prd;i++) {
            if (fbuf[i] == tbuf[i]) {
                if (flush_obuf(ips_fd) < 0)
                    return 1;
            }
            else {
                if (obufp >= CMPSZ) {
                   if (flush_obuf(ips_fd) < 0)
                       return 1;
                }

                if (obufp == 0) {
                    obufpos = pos;
                    obufdb = 1;
                }
                else {
                    if (obufdb && obuf[0] != tbuf[i])
                        obufdb = 0;
                }

                obuf[obufp++] = tbuf[i];
            }

            pos++;
        }

        for (;i < (unsigned int)trd;i++) {
            if (obufp >= CMPSZ)
                flush_obuf(ips_fd);

            if (obufp == 0) {
                obufdb = 0;
                obuf[obufdb++] = tbuf[i];
            }
        }
    }

    if (flush_obuf(ips_fd) < 0)
        return 1;

    if (write(ips_fd,"EOF",3) != 3)
        return 1;

    close(ips_fd);
    close(f_fd);
    close(t_fd);
    return 0;
}

