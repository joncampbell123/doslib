
#include <dos.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define CLSG_EXPORT_PROC __cdecl far

unsigned int exe_seg = 0;
unsigned int exe_len = 0;

void free_exe(void);

static inline unsigned short exe_func_count(void) {
    return *((unsigned short*)MK_FP(exe_seg,4));
}

static inline unsigned short exe_func_ento(const unsigned int i) {
    return *((unsigned short*)MK_FP(exe_seg,4+2+(i*2U)));
}

/* NTS: You're supposed to typecast return value into function pointer prototype */
static inline void far *exe_func_ent(const unsigned int i) {
    return (void far*)MK_FP(exe_seg,exe_func_ento(i));
}

int load_exe(const char *path) {
    unsigned char *hdr=NULL;
    unsigned long img_sz=0;
    unsigned long hdr_sz=0;
    unsigned long add_sz=0; // BSS segment
    int fd;

    free_exe();

    fd = open(path,O_RDONLY|O_BINARY);
    if (fd < 0) return 0;

    {
        char tmp[0x1C];
        if (read(fd,tmp,0x1C) < 0x1C) goto fail;
        if (*((unsigned short*)(tmp+0)) != 0x5A4D) goto fail;

        /* img_sz = number of blocks part of EXE file */
        img_sz = ((unsigned long) *((unsigned short*)(tmp+4))) << 9UL; /* *512 */
        {
            /* x = number of bytes in the last block that are actually used. 0 means whole block */
            unsigned short x = *((unsigned short*)(tmp+2));
            if (x != 0) img_sz += (unsigned long)x - 512UL; /* <- important: unsigned long to ensure x - 512 subtracts due to carry */
        }

        /* EXE header size */
        hdr_sz = ((unsigned long) *((unsigned short*)(tmp+8))) << 4UL; /* *16 */
        if (hdr_sz < 0x20UL) goto fail; /* header must be at least 32 bytes */
        if (hdr_sz > 0x8000UL) goto fail; /* header up to 32KB supported */
        if (hdr_sz >= img_sz) goto fail; /* header cannot completely consume EXE file */

        /* additional resident set (BSS segment) */
        add_sz = ((unsigned long) *((unsigned short*)(tmp+10))) << 4UL; /* *16 */
        if (add_sz > 0xFFFF0UL) goto fail;

        /* this loader only supports EXE images (resident part) up to 64KB */
        if ((img_sz+add_sz-hdr_sz) >= 0xFFF0UL) goto fail;

        /* finish loading EXE header */
        hdr = malloc((unsigned short)hdr_sz);
        if (hdr == NULL) goto fail;
        memcpy(hdr,tmp,0x1C); /* this is why header must be 20 bytes, also to make valid structure */
    }

    /* finish loading EXE header */
    {
        unsigned short more = (unsigned short)(hdr_sz - 0x1C); /* hdr_sz >= 0x20 therefore this cannot be zero */
        if (read(fd,hdr+0x1C,more) < more) goto fail;
    }

    /* allocate resident section.
     * resident section is resident part of EXE on disk plus BSS segment in memory (additional paragraphs) */
    {
        unsigned short paras = (unsigned short)((img_sz+add_sz+0xFUL/*round up*/-hdr_sz) >> 4UL); /* NTS: should NOT exceed 64KB we checked farther up! */

        if (_dos_allocmem(paras,&exe_seg) != 0) goto fail;
    }

    /* file pointer is just past EXE header, load contents */
    {
        unsigned int rd = 0;

        /* NTS: _dos_read() could be useful in small memory model if it took far pointer! */
        if (_dos_read(fd,(void far*)MK_FP(exe_seg,0),(unsigned short)(img_sz-hdr_sz),&rd) != 0 || rd != (unsigned short)(img_sz-hdr_sz)) goto fail;
        exe_len = (unsigned short)(img_sz+add_sz-hdr_sz);
    }

    /* need to zero additional size */
    if (add_sz != 0) {
        unsigned char far *p = (unsigned char far*)MK_FP(exe_seg,img_sz-hdr_sz); /* point at region between end of img and end of segment */
        _fmemset(p,0,(unsigned short)add_sz);
    }

    /* apply relocation table */
    {
        unsigned short offs = *((unsigned short*)(hdr+0x18));
        unsigned short count = *((unsigned short*)(hdr+0x06));
        unsigned short *sopairs = ((unsigned short*)(hdr+offs));
        unsigned int i;

        if (count != 0) {
            if (count > 0x4000) goto fail;
            if (((unsigned long)offs + ((unsigned long)count*4UL)) > hdr_sz) goto fail;

            for (i=0;i < count;i++,sopairs += 2) {
                unsigned short o = sopairs[0];
                unsigned short s = sopairs[1];
                unsigned long of = ((unsigned long)s << 4UL) + (unsigned long)o;

                /* range check.
                 * we'll allow relocations in the BSS segment though */
                if ((of+2UL) > (unsigned long)exe_len) goto fail;

                /* patch */
                *((unsigned short far*)MK_FP(exe_seg+s,o)) += exe_seg;
            }
        }
    }

    /* final check: CLSG at the beginning of the resident image and a valid function call table */
    {
        unsigned char far *p = MK_FP(exe_seg,0);
        unsigned short far *functbl = (unsigned short far*)(p+4UL+2UL);
        unsigned short numfunc;
        unsigned int i;

        if (*((unsigned long far*)(p+0)) != 0x47534C43UL) goto fail; // seg:0 = CLSG

        numfunc = *((unsigned short far*)(p+4UL));
        if (((numfunc*2UL)+4UL+2UL) > exe_len) goto fail;

        /* none of the functions can point outside the EXE resident image. */
        for (i=0;i < numfunc;i++) {
            unsigned short fo = functbl[i];
            if (fo >= exe_len) goto fail;
        }

        /* the last entry after function table must be 0xFFFF */
        if (functbl[numfunc] != 0xFFFFU) goto fail;
    }

    if (hdr != NULL) free(hdr);
    close(fd);
    return 1;
fail:
    if (hdr != NULL) free(hdr);
    close(fd);
    free_exe();
    return 0;
}

void free_exe(void) {
    if (exe_seg != 0) {
        _dos_freemem(exe_seg);
        exe_seg = 0;
    }
}

int main() {
    if (!load_exe("final.exe")) {
        fprintf(stderr,"Load failed\n");
        return 1;
    }

    fprintf(stderr,"EXE image loaded to %04x:0000 residentlen=%u\n",exe_seg,exe_len);
    {
        unsigned int i,m=exe_func_count();

        fprintf(stderr,"%u functions:\n",m);
        for (i=0;i < m;i++)
            fprintf(stderr,"  [%u]: %04x (%Fp)\n",i,exe_func_ento(i),exe_func_ent(i));
    }

    /* let's call some! */
    {
        /* index 0:
           const char far * CLSG_EXPORT_PROC get_message(void); */
        const char far * (CLSG_EXPORT_PROC *get_message)(void) = exe_func_ent(0);
        const char far *msg;

        fprintf(stderr,"Calling entry 0 (get_message) now.\n");
        msg = get_message();
        fprintf(stderr,"Result: %Fp = %Fs\n",msg,msg);
    }

    {
        /* index 1:
           unsigned int CLSG_EXPORT_PROC callmemaybe(void); */
        unsigned int (CLSG_EXPORT_PROC *callmemaybe)(void) = exe_func_ent(1);
        unsigned int val;

        fprintf(stderr,"Calling entry 1 (callmemaybe) now.\n");
        val = callmemaybe();
        fprintf(stderr,"Result: %04x\n",val);
    }

    free_exe();
    return 0;
}

