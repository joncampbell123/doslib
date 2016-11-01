
#include <dos.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <hw/dos/exehdr.h>

#define CLSG_EXPORT_PROC __cdecl far

struct exeload_ctx {
    unsigned int                base_seg;           // base segment
    unsigned int                len_seg;            // length in segments (paragraphs)
    struct exe_dos_header*      header;             // allocated copy of EXE header. you can free to conserve memory.
};

#define exeload_ctx_INIT {0,0,NULL}

struct exeload_ctx      final_exe = exeload_ctx_INIT;

void exeload_free(struct exeload_ctx *exe);
int exeload_load(struct exeload_ctx *exe,const char * const path);
int exeload_load_fd(struct exeload_ctx *exe,const int fd/*must be open, you must lseek to EXE header*/);

static inline unsigned long exeload_resident_length(const struct exeload_ctx * const exe) {
    return (unsigned long)exe->len_seg << 4UL;
}

void exeload_free_header(struct exeload_ctx *exe) {
    if (exe->header != NULL) {
        free(exe->header);
        exe->header = NULL;
    }
}

int exeload_load(struct exeload_ctx *exe,const char * const path) {
    int fd,ret;

    if (exe == NULL) return 0;
    exeload_free(exe);

    fd = open(path,O_RDONLY|O_BINARY);
    if (fd < 0) return 0;

    ret = exeload_load_fd(exe,fd);

    close(fd);
    return ret;
}

int exeload_load_fd(struct exeload_ctx *exe,const int fd/*must be open, you must lseek to EXE header*/) {
    struct exe_dos_header *header;
    unsigned char *hdr=NULL;
    unsigned long img_sz=0;
    unsigned long hdr_sz=0;
    unsigned long add_sz=0; // BSS segment

    if (exe == NULL) return 0;
    exeload_free(exe);
    if (fd < 0) return 0;

    // keep a local copy so the C compiler doesn't have to keep dereferencing exe->header
    header = exe->header = malloc(sizeof(*header));
    if (header == NULL) goto fail;

    {
        if (read(fd,header,sizeof(*header)) < sizeof(*header))
            goto fail;

        if (header->magic != 0x5A4D) goto fail;

        /* img_sz = number of blocks part of EXE file */
        img_sz = exe_dos_header_file_resident_size(header);

        /* EXE header size */
        hdr_sz = exe_dos_header_file_header_size(header);
        if (hdr_sz < 0x20UL) goto fail; /* header must be at least 32 bytes */
        if (hdr_sz > 0x8000UL) goto fail; /* header up to 32KB supported */
        if (hdr_sz >= img_sz) goto fail; /* header cannot completely consume EXE file */

        /* additional resident set (BSS segment) */
        add_sz = exe_dos_header_bss_size(header);
        if (add_sz > 0xA0000UL) goto fail; /* 640KB limit */

        /* this loader only supports EXE images (resident part) up to 640KB */
        if ((img_sz+add_sz-hdr_sz) >= 0xA0000UL) goto fail;

        /* finish loading EXE header */
        hdr = malloc((unsigned short)hdr_sz);
        if (hdr == NULL) goto fail;
        memcpy(hdr,header,sizeof(*header)); /* this is why header must be 20 bytes, also to make valid structure */
    }

    /* finish loading EXE header */
    {
        unsigned short more = (unsigned short)(hdr_sz - sizeof(*header)); /* hdr_sz >= 0x20 therefore this cannot be zero */
        if (read(fd,hdr+sizeof(*header),more) < more) goto fail;
    }

    /* allocate resident section.
     * resident section is resident part of EXE on disk plus BSS segment in memory (additional paragraphs) */
    {
        unsigned short paras = (unsigned short)((img_sz+add_sz+0xFUL/*round up*/-hdr_sz) >> 4UL); /* NTS: should never exceed 640KB */
        if (_dos_allocmem(paras,&exe->base_seg) != 0) goto fail;
        exe->len_seg = paras;
    }

    /* file pointer is just past EXE header, load contents */
    {
        unsigned int load_seg = exe->base_seg;
        unsigned long rem = img_sz - hdr_sz;
        unsigned int rd = 0;

        {
            const unsigned int maxload = 0x8000U; // 32KB (must be multiple of 16)
            unsigned int doload;

            while (rem > 0UL) {
                if (rem > (unsigned long)maxload)
                    doload = maxload;
                else
                    doload = (unsigned int)rem;

                /* NTS: Do not compile the _dos_read() call in small model, because the param is not a far pointer */
                if (_dos_read(fd,(void far*)MK_FP(load_seg,0),doload,&rd) != 0)
                    goto fail;
                if (rd != doload)
                    goto fail;

                load_seg += doload >> 4UL;
                rem -= doload;
            }
        }
    }

    /* need to zero additional size */
    if (add_sz != 0) {
        unsigned long rem = add_sz;
        unsigned long ao = img_sz-hdr_sz;
        unsigned int zero_seg = exe->base_seg+(ao>>4UL);
        const unsigned int maxz = 0x8000U;
        unsigned int doz;

        while (rem > 0UL) {
            if (rem > (unsigned long)maxz)
                doz = maxz;
            else
                doz = (unsigned int)rem;

            _fmemset(MK_FP(zero_seg,(unsigned int)ao&0xFU),0,doz);
            zero_seg += doz >> 4UL;
            rem -= doz;
        }
    }

    /* apply relocation table */
    {
        const unsigned short count = header->number_of_relocations;
        const unsigned short offs = header->relocation_table_offset;
        const unsigned short *sopairs = ((const unsigned short*)(hdr+offs));
        unsigned int i;

        if (count != 0) {
            if (count >= 0x4000U) goto fail; /* if table would exceed 64KB */
            if (((unsigned long)offs + ((unsigned long)count*4UL)) > hdr_sz) goto fail; /* if relocation table is outside header (invalid) */

            for (i=0;i < count;i++,sopairs += 2) {
                unsigned short o = sopairs[0];
                unsigned short s = sopairs[1];
                unsigned long of = ((unsigned long)s << 4UL) + (unsigned long)o;

                /* range check, to avoid corrupting memory around the EXE image.
                 * we'll allow relocations in the BSS segment though */
                if ((of+2UL) > (img_sz+add_sz-hdr_sz)) goto fail;

                /* patch */
                *((unsigned short far*)MK_FP(exe->base_seg+s,o)) += exe->base_seg;
            }
        }
    }

    if (hdr != NULL) free(hdr);
    return 1;
fail:
    if (hdr != NULL) free(hdr);
    exeload_free(exe);
    return 0;
}

void exeload_free(struct exeload_ctx *exe) {
    if (exe == NULL) return;

    if (exe->base_seg != 0) {
        _dos_freemem(exe->base_seg);
        exe->base_seg = 0;
    }

    exeload_free_header(exe);
    exe->len_seg = 0;
}

int exeload_clsg_validate_header(const struct exeload_ctx * const exe) {
    if (exe == NULL) return 0;

    {
        /* CLSG at the beginning of the resident image and a valid function call table */
        unsigned char far *p = MK_FP(exe->base_seg,0);
        unsigned short far *functbl = (unsigned short far*)(p+4UL+2UL);
        unsigned long exe_len = exeload_resident_length(exe);
        unsigned short numfunc;
        unsigned int i;

        if (*((unsigned long far*)(p+0)) != 0x47534C43UL) return 0; // seg:0 = CLSG

        numfunc = *((unsigned short far*)(p+4UL));
        if (((numfunc*2UL)+4UL+2UL) > exe_len) return 0;

        /* none of the functions can point outside the EXE resident image. */
        for (i=0;i < numfunc;i++) {
            unsigned short fo = functbl[i];
            if ((unsigned long)fo >= exe_len) return 0;
        }

        /* the last entry after function table must be 0xFFFF */
        if (functbl[numfunc] != 0xFFFFU) return 0;
    }

    return 1;
}

static inline unsigned short exeload_clsg_function_count(const struct exeload_ctx * const exe) {
    return *((unsigned short*)MK_FP(exe->base_seg,4));
}

static inline unsigned short exeload_clsg_function_offset(const struct exeload_ctx * const exe,const unsigned int i) {
    return *((unsigned short*)MK_FP(exe->base_seg,4+2+(i*2U)));
}

/* NTS: You're supposed to typecast return value into function pointer prototype */
static inline void far *exeload_clsg_function_ptr(const struct exeload_ctx * const exe,const unsigned int i) {
    return (void far*)MK_FP(exe->base_seg,exeload_clsg_function_offset(exe,i));
}

int main() {
    if (!exeload_load(&final_exe,"final.exe")) {
        fprintf(stderr,"Load failed\n");
        return 1;
    }
    if (!exeload_clsg_validate_header(&final_exe)) {
        fprintf(stderr,"EXE is not valid CLSG\n");
        exeload_free(&final_exe);
        return 1;
    }

    fprintf(stderr,"EXE image loaded to %04x:0000 residentlen=%lu\n",final_exe.base_seg,(unsigned long)final_exe.len_seg << 4UL);
    {
        unsigned int i,m=exeload_clsg_function_count(&final_exe);

        fprintf(stderr,"%u functions:\n",m);
        for (i=0;i < m;i++)
            fprintf(stderr,"  [%u]: %04x (%Fp)\n",i,exeload_clsg_function_offset(&final_exe,i),exeload_clsg_function_ptr(&final_exe,i));
    }

    /* let's call some! */
    {
        /* index 0:
           const char far * CLSG_EXPORT_PROC get_message(void); */
        const char far * (CLSG_EXPORT_PROC *get_message)(void) = exeload_clsg_function_ptr(&final_exe,0);
        const char far *msg;

        fprintf(stderr,"Calling entry 0 (get_message) now.\n");
        msg = get_message();
        fprintf(stderr,"Result: %Fp = %Fs\n",msg,msg);
    }

    {
        /* index 1:
           unsigned int CLSG_EXPORT_PROC callmemaybe(void); */
        unsigned int (CLSG_EXPORT_PROC *callmemaybe)(void) = exeload_clsg_function_ptr(&final_exe,1);
        unsigned int val;

        fprintf(stderr,"Calling entry 1 (callmemaybe) now.\n");
        val = callmemaybe();
        fprintf(stderr,"Result: %04x\n",val);
    }

    exeload_free(&final_exe);
    return 0;
}

