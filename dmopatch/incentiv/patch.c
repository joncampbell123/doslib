
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

#ifndef O_BINARY
#define O_BINARY (0)
#endif

struct patch_info {
    const char*             name;

    const size_t            match_offset;
    const unsigned char*    match_what;
    const size_t            match_what_size;

    const size_t            patch_offset;
    const unsigned char*    patch_what;
    const size_t            patch_what_size;
};

/* ================================= GUS POP DS */

const unsigned char gus_pop_ds_bug_match[] = {
    /*+0000:2D63*/ 0x07,                        /* pop  es */
    /*+0000:2D64*/ 0x1F,                        /* pop  ds */
    /*+0000:2D65*/ 0x8B,0x16,0xFE,0x38,         /* mov  dx,[38FE] */
    /*+0000:2D69*/ 0xB0,0x8F,                   /* mov  al,8F */
    /*+0000:2D6B*/ 0xEE,                        /* out  dx,al */
    /*+0000:2D6C*/ 0x80,0xC2,0x02,              /* add  dl,02 */
    /*+0000:2D6F*/ 0xEC,                        /* in   al,dx */
    /*+0000:2D70*/ 0x61,                        /* popa */
    /*+0000:2D71*/ 0xCF                         /* iret */
};

const unsigned char gus_pop_ds_bug_patch[] = {
    /*+0000:2D63*/ 0x8B,0x16,0xFE,0x38,         /* mov  dx,[38FE]              ds:[38FE]=0000 */
    /*+0000:2D67*/ 0xB0,0x8F,                   /* mov  al,8F */
    /*+0000:2D69*/ 0xEE,                        /* out  dx,al */
    /*+0000:2D6A*/ 0x80,0xC2,0x02,              /* add  dl,02 */
    /*+0000:2D6D*/ 0xEC,                        /* in   al,dx */
    /*+0000:2D6E*/ 0x07,                        /* pop  es */
    /*+0000:2D6F*/ 0x1F,                        /* pop  ds */
    /*+0000:2D70*/ 0x61,                        /* popa */
    /*+0000:2D71*/ 0xCF                         /* iret */
};

/* ============================================ */

const struct patch_info gus_pop_ds_bug = {
    "Gravis Ultrasound IRQ handler POP DS bug",     /* name */

    /* match */
    0x2D63,                                         /* offset */
    gus_pop_ds_bug_match,
    sizeof(gus_pop_ds_bug_match),

    /* patch */
    0x2D63,                                         /* offset */
    gus_pop_ds_bug_patch,
    sizeof(gus_pop_ds_bug_patch)
};

static unsigned char temp[4096];

int apply_patch(int fd,const struct patch_info *p,long base) {
    assert(p->name != NULL);
    assert(p->match_what != NULL);
    assert(p->match_what_size != 0);
    assert(p->patch_what != NULL);
    assert(p->patch_what_size != 0);
    assert(p->match_what_size <= sizeof(temp));
    assert(p->patch_what_size <= sizeof(temp));

    if (lseek(fd,p->match_offset + base,SEEK_SET) != (p->match_offset + base))
        return 0;
    if (read(fd,temp,p->match_what_size) != p->match_what_size)
        return 0;

    if (!memcmp(temp,p->match_what,p->match_what_size)) {
        printf("Code match at 0x%lx '%s'\n",(unsigned long)p->match_offset,p->name);
    }

    if (lseek(fd,p->patch_offset + base,SEEK_SET) != (p->patch_offset + base))
        return 0;
    if (write(fd,p->patch_what,p->patch_what_size) != p->patch_what_size) {
        printf("ERROR: Unable to write patch\n");
        return -1;
    }

    return 0;
}

int openrw_file(const char *path) {
    int fd;

    fd = open(path,O_RDWR|O_BINARY);
    if (fd < 0) {
        fprintf(stderr,"Unable to open for rw, file %s\n",path);
        return -1;
    }

    return fd;
}

void execom_get_base(long *base,int fd) {
    *base = 0;

    if (lseek(fd,0,SEEK_SET) != 0)
        return;
    if (read(fd,temp,0x20) != 0x20)
        return;

    if (!memcmp(temp,"MZ",2)) {
        *base = *((uint16_t*)(temp + 0x08)) << 4L; /* EXE header size in paragraphs -> base */
        printf("EXE header size: %lu\n",*base);
    }
}

int main() {
    long patch_base = 0;
    int fd;

    printf("DOSLIB/DOSBox-X patch for DID INCENTIV (the party 1994)\n");
    printf("This patch fixes:\n");
    printf(" - Random I/O access while playing music on Gravis Ultrasound.\n");

    /* ======================================= INCENTIV.EXE ================================ */
    if ((fd=openrw_file("INCENTIV.EXE")) < 0) return 1;
    execom_get_base(&patch_base,fd);
    if (apply_patch(fd,&gus_pop_ds_bug,patch_base) < 0) return 1;
    close(fd);
    /* ===================================================================================== */

    return 0;
}

