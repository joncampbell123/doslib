
#if !defined(TARGET_OS2) && !defined(TARGET_WINDOWS) && TARGET_MSDOS == 16

#include <dos.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <hw/dos/dos.h>
#include <hw/dos/exeload.h>

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
        const unsigned int maxload = 0x8000U; // 32KB (must be multiple of 16)
        unsigned int load_seg = exe->base_seg;
        unsigned long rem = img_sz - hdr_sz;
        unsigned int doload;

        while (rem > 0UL) {
            if (rem > (unsigned long)maxload)
                doload = maxload;
            else
                doload = (unsigned int)rem;

            if (_dos_xread(fd,(void far*)MK_FP(load_seg,0),doload) != doload)
                goto fail;

            load_seg += doload >> 4UL;
            rem -= doload;
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

#endif

