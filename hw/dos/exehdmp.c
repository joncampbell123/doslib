
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

#include <hw/dos/exehdr.h>

#ifndef O_BINARY
#define O_BINARY (0)
#endif

struct exe_dos_layout_range {
    uint32_t                    start,end;      // NTS: inclusive byte range, does not exist if start > end
};

struct exe_dos_layout {
    struct exe_dos_layout_range     header;             // EXE header. if start == end then struct not initialized.
    struct exe_dos_layout_range     load_resident;      // EXE portion loaded into memory (resident + EXE header)
    struct exe_dos_layout_range     run_resident;       // EXE portion left in memory after EXE loading
    struct exe_dos_layout_range     relocation_table;   // EXE relocation table

    uint32_t                        min_mem_footprint;
    uint32_t                        max_mem_footprint;

    uint32_t                        code_entry_point_res;   // offset into resident image of CS:IP
    uint32_t                        code_entry_point_file;  // offset into file of CS:IP
                                                            // ^ NOTE: 0 if not exist

    uint32_t                        stack_entry_point_res;  // offset into resident image of SS:SP
                                                            // ^ NOTE: This can (and usually does) point beyond the resident image
                                                            //         into the "bss" area (defined by min_additional_paragraphs).
    uint32_t                        stack_entry_point_file; // offset into file of SS:SP
                                                            // ^ NOTE: 0 if not exist
                                                            //         For most EXEs this may be zero because SS:SP points beyond
                                                            //         the resident image into the "bss" area (defined by
                                                            //         min_additional_paragraphs)
};

static inline uint32_t exe_dos_layout_range_get_length(const struct exe_dos_layout_range * const r) {
    if (r->start <= r->end)
        return r->end + (uint32_t)1UL - r->start;

    return 0;
}

static inline unsigned int exe_dos_layout_range_is_init(const struct exe_dos_layout_range * const r) {
    return (r->start <= r->end) ? 1U : 0U;
}

static inline void exe_dos_layout_range_deinit(struct exe_dos_layout_range * const r) {
    r->start = (uint32_t)0xFFFFFFFFUL;
    r->end = 0;
}

static inline void exe_dos_layout_range_set(struct exe_dos_layout_range * const r,const uint32_t start,const uint32_t end) {
    r->start = start;
    r->end = end;
}

int exe_dos_header_to_layout(struct exe_dos_layout * const lay,const struct exe_dos_header * const hdr) {
    unsigned long t;

    lay->min_mem_footprint = 0;
    lay->max_mem_footprint = 0;
    lay->code_entry_point_res = 0;
    lay->code_entry_point_file = 0;
    lay->stack_entry_point_res = 0;
    lay->stack_entry_point_file = 0;
    exe_dos_layout_range_deinit(&lay->header);
    exe_dos_layout_range_deinit(&lay->run_resident);
    exe_dos_layout_range_deinit(&lay->load_resident);
    exe_dos_layout_range_deinit(&lay->relocation_table);
    if (hdr->magic != 0x5A4DU/*MZ*/)
        return -1;

    t = exe_dos_header_file_header_size(hdr);
    if (t == 0UL) return -1;
    exe_dos_layout_range_set(&lay->header,0,t - 1UL);

    /* exit now if EXE header denies it's own size */
    if (t < 0x1C) return 0;

    /* resident section follows EXE header.
     * EXE header expresses resident section as EXE header + resident section.
     * I'm guessing this made the DOS kernel's job easier since it helps dictate
     * how much to load into memory before beginning to parse the EXE header and
     * relocation table. */
    t = exe_dos_header_file_resident_size(hdr);
    if (t == 0UL) return 0;
    exe_dos_layout_range_set(&lay->load_resident,0,t - 1UL);

    /* once loaded, the resident portion no longer includes EXE header.
     * if the resident area is smaller than EXE header then bail now. */
    if (t <= exe_dos_header_file_header_size(hdr)) return 0;
    exe_dos_layout_range_set(&lay->run_resident,exe_dos_header_file_header_size(hdr),lay->load_resident.end);

    /* finally, if there is a relocation table, note it.
     * each relocation is 4 bytes long (32 bits), and stores a (relative SEG):OFF address of a 16-bit WORD that DOS adds the segment base to.
     * if the EXE header somehow puts the relocation table out of the resident range, well, that's the EXE's stupidity. */
    if (hdr->number_of_relocations != 0U) {
        exe_dos_layout_range_set(&lay->relocation_table,
            hdr->relocation_table_offset,
            hdr->relocation_table_offset + (hdr->number_of_relocations * 4UL) - 1UL);
    }

    /* we can determine runtime memory footprint too */
    lay->min_mem_footprint =
        exe_dos_layout_range_get_length(&lay->run_resident) +
        ((uint32_t)(hdr->min_additional_paragraphs) << 4UL);
    lay->max_mem_footprint =
        exe_dos_layout_range_get_length(&lay->run_resident) +
        ((uint32_t)(hdr->max_additional_paragraphs) << 4UL);

    /* we can also pinpoint CS:IP and SS:IP */
    /* NTS: we mask by 0xFFFFFUL for 16-bit real mode 1MB aliasing.
     *      some early EXE files are in fact .COM converted to .EXE
     *      and then the instruction pointer in the header set to
     *      0xFFEF:0x0100 to set CS:IP up like a .COM executable.
     *      that only works because of 1MB wraparound and because
     *      of 16-bit segment registers. */
    lay->code_entry_point_res =
        ((uint32_t)((hdr->init_code_segment << 4UL) +
         (uint32_t)(hdr->init_instruction_pointer))) & 0xFFFFFUL;
    if (lay->code_entry_point_res <= lay->run_resident.end) {
        lay->code_entry_point_file =
            lay->code_entry_point_res + lay->run_resident.start;
    }

    /* NTS: it doesn't happen often, but the EXE header can define
     *      SS:SP to point within the resident image at startup. */
    lay->stack_entry_point_res =
        ((uint32_t)((hdr->init_stack_segment << 4UL) +
         (uint32_t)(hdr->init_stack_pointer))) & 0xFFFFFUL;
    if (lay->stack_entry_point_res <= lay->run_resident.end) {
        lay->stack_entry_point_file =
            lay->stack_entry_point_res + lay->run_resident.start;
    }

    return 0;
}

static char*                    src_file = NULL;
static int                      src_fd = -1;

#if defined(LINUX)
# define relocentmax            8192
#elif defined(__LARGE__) || defined(__FLAT__)
# define relocentmax            4096
#else
# define relocentmax            64
#endif

static struct exe_dos_header    exehdr;
static struct exe_dos_layout    exelayout;
static uint32_t                 relocent[relocentmax];
static unsigned int             relocentcount;

static void help(void) {
    fprintf(stderr,"EXEHDMP -i <exe file>\n");
}

int main(int argc,char **argv) {
    char *a;
    int i;

    memset(&exehdr,0,sizeof(exehdr));

    for (i=1;i < argc;) {
        a = argv[i++];

        if (*a == '-') {
            do { a++; } while (*a == '-');

            if (!strcmp(a,"h") || !strcmp(a,"help")) {
                help();
                return 1;
            }
            else if (!strcmp(a,"i")) {
                src_file = argv[i++];
                if (src_file == NULL) return 1;
            }
            else {
                fprintf(stderr,"Unknown switch %s\n",a);
                return 1;
            }
        }
        else {
            fprintf(stderr,"Unknown switch %s\n",a);
            return 1;
        }
    }

    assert(sizeof(exehdr) == 0x1C);

    if (src_file == NULL) {
        fprintf(stderr,"No source file specified\n");
        return 1;
    }

    src_fd = open(src_file,O_RDONLY|O_BINARY);
    if (src_fd < 0) {
        fprintf(stderr,"Unable to open '%s', %s\n",src_file,strerror(errno));
        return 1;
    }

    if (read(src_fd,&exehdr,sizeof(exehdr)) != (int)sizeof(exehdr)) {
        fprintf(stderr,"EXE header read error\n");
        return 1;
    }

    if (exehdr.magic != 0x5A4DU/*MZ*/) {
        fprintf(stderr,"EXE header signature missing\n");
        return 1;
    }

    printf("MS-DOS EXE header:\n");
    printf("    last_block_bytes:             %u bytes\n",
        exehdr.last_block_bytes);
    printf("    exe_file_blocks:              %u bytes\n",
        exehdr.exe_file_blocks);
    printf("  * exe resident size (blocks):   %lu bytes\n",
        (unsigned long)exe_dos_header_file_resident_size(&exehdr));
    printf("    number_of_relocations:        %u entries\n",
        exehdr.number_of_relocations);
    printf("  * size of relocation table:     %lu bytes\n",
        (unsigned long)exehdr.number_of_relocations * 4UL);
    printf("    header_size:                  %u paragraphs\n",
        exehdr.header_size_paragraphs);
    printf("  * header_size:                  %lu bytes\n",
        (unsigned long)exe_dos_header_file_header_size(&exehdr));
    printf("    min_additional_paragraphs:    %u paragraphs\n",
        exehdr.min_additional_paragraphs);
    printf("  * min_additional:               %lu bytes\n",
        (unsigned long)exe_dos_header_bss_size(&exehdr));
    printf("    max_additional_paragraphs:    %u paragraphs\n",
        exehdr.max_additional_paragraphs);
    printf("  * max_additional:               %lu bytes\n",
        (unsigned long)exe_dos_header_bss_max_size(&exehdr));
    printf("    init stack pointer:           base_seg+0x%04X:0x%04X\n",
        exehdr.init_stack_segment,
        exehdr.init_stack_pointer);
    printf("    checksum:                     0x%04X\n",
        exehdr.checksum);
    printf("    init instruction pointer:     base_seg+0x%04X:0x%04X\n",
        exehdr.init_code_segment,
        exehdr.init_instruction_pointer);
    printf("    relocation_table_offset:      %u bytes\n",
        exehdr.relocation_table_offset);
    printf("    overlay number:               %u\n",
        exehdr.overlay_number);

    if (exe_dos_header_to_layout(&exelayout,&exehdr) < 0) {
        fprintf(stderr,"Unable to determine EXE layout\n");
        return 1;
    }

    if (exe_dos_layout_range_is_init(&exelayout.header)) {
        printf("  * exe header portion:           %lu - %lu bytes (inclusive) = %lu bytes\n",
            (unsigned long)exelayout.header.start,
            (unsigned long)exelayout.header.end,
            (unsigned long)exe_dos_layout_range_get_length(&exelayout.header));
    }
    else {
        printf("  ! exe header range not defined\n");
    }

    if (exe_dos_layout_range_is_init(&exelayout.load_resident)) {
        printf("  * resident portion (load):      %lu - %lu bytes (inclusive) = %lu bytes\n",
            (unsigned long)exelayout.load_resident.start,
            (unsigned long)exelayout.load_resident.end,
            (unsigned long)exe_dos_layout_range_get_length(&exelayout.load_resident));
    }
    else {
        printf("  ! resident load range not defined\n");
    }

    if (exe_dos_layout_range_is_init(&exelayout.run_resident)) {
        printf("  * resident portion (runtime):   %lu - %lu bytes (inclusive) = %lu bytes\n",
            (unsigned long)exelayout.run_resident.start,
            (unsigned long)exelayout.run_resident.end,
            (unsigned long)exe_dos_layout_range_get_length(&exelayout.run_resident));
    }
    else {
        printf("  ! resident load range not defined\n");
    }

    if (exe_dos_layout_range_is_init(&exelayout.relocation_table)) {
        printf("  * relocation table:             %lu - %lu bytes (inclusive) = %lu bytes\n",
            (unsigned long)exelayout.relocation_table.start,
            (unsigned long)exelayout.relocation_table.end,
            (unsigned long)exe_dos_layout_range_get_length(&exelayout.relocation_table));
    }

    printf("  * minimum memory footprint:     %lu bytes\n",
        (unsigned long)exelayout.min_mem_footprint);

    printf("  * maximum memory footprint:     %lu bytes\n",
        (unsigned long)exelayout.max_mem_footprint);

    printf("  * code pointer file offset:     %lu(resident) + %lu = %lu(file) bytes\n",
        (unsigned long)exelayout.code_entry_point_res,
        (unsigned long)exelayout.run_resident.start,
       (unsigned long) exelayout.code_entry_point_file);
    printf("                                  base_seg+0x%04X:0x%04X\n",
        (unsigned int)(exelayout.code_entry_point_res>>4UL),
        (unsigned int)(exelayout.code_entry_point_res&0xFUL));

    if (exelayout.code_entry_point_res > exelayout.run_resident.end)
        printf("  ! CS:IP points outside resident portion (%lu > %lu(last byte))\n",
            (unsigned long)exelayout.code_entry_point_res,
            (unsigned long)exelayout.run_resident.end);

    if (exelayout.stack_entry_point_res > exelayout.run_resident.end) {
        /* most common */
        printf("  * stack pointer file offset:    %lu(resident) bytes (BSS area beyond resident)\n",
            (unsigned long)exelayout.stack_entry_point_res);
    }
    else {
        /* less common */
        printf("  * stack pointer file offset:    %lu(resident) + %lu = %lu(file) bytes\n",
            (unsigned long)exelayout.stack_entry_point_res,
            (unsigned long)exelayout.run_resident.start,
            (unsigned long)exelayout.stack_entry_point_file);
    }
    printf("                                  base_seg+0x%04X:0x%04X\n",
        (unsigned int)(exelayout.stack_entry_point_res>>4UL),
        (unsigned int)(exelayout.stack_entry_point_res&0xFUL));

    /* we compare > min not >= min because programs typically issue PUSH instructions
     * at startup which decrement SS:SP then write to stack.
     *
     * however, many Windows EXE stubs set min_paragraphs == 0 but SS:SP about 128
     * bytes past end of resident area, so show some forgiveness if min_paragraphs == 0 */
    if (exelayout.stack_entry_point_res > exelayout.min_mem_footprint) {
        if (exehdr.min_additional_paragraphs == 0 && exelayout.stack_entry_point_res < (256+exe_dos_layout_range_get_length(&exelayout.run_resident))) {
            printf("  * stack pointer past resident area with no BSS section following it,\n");
            printf("    but not far enough to be a problem.\n");
        }
        else {
            printf("  ! SS:SP points outside minimum memory footprint (%lu > %lu(bytes))\n",
                (unsigned long)exelayout.stack_entry_point_res,
                (unsigned long)exelayout.min_mem_footprint);
        }
    }

#if 0
    if (exe_dos_header_file_resident_size(&exehdr) > exe_dos_header_file_header_size(&exehdr)) {
        unsigned long start,end,resend;

        start =
            0;
        end =
            (unsigned long)exe_dos_header_file_resident_size(&exehdr) -
            (unsigned long)exe_dos_header_file_header_size(&exehdr) - 1UL;
        printf("  * resident segment range:       from base_seg+0x%04X:0x%04X\n",
            (unsigned int)(start>>4UL),(unsigned int)(start&0xFUL));
        printf("                                    to base_seg+0x%04X:0x%04X\n",
            (unsigned int)(end>>4UL),(unsigned int)(end&0xFUL));

        start =
            (unsigned long)exe_dos_header_file_resident_size(&exehdr) -
            (unsigned long)exe_dos_header_file_header_size(&exehdr);
        if (exe_dos_header_bss_size(&exehdr) != 0UL) {
            end =
                start +
                (unsigned long)exe_dos_header_bss_size(&exehdr) - 1UL;
            printf("  * resident bss range (min):     from base_seg+0x%04X:0x%04X\n",
                (unsigned int)(start>>4UL),(unsigned int)(start&0xFUL));
            printf("                                    to base_seg+0x%04X:0x%04X\n",
                (unsigned int)(end>>4UL),(unsigned int)(end&0xFUL));
        }
        if (exe_dos_header_bss_max_size(&exehdr) != 0UL) {
            end =
                start +
                (unsigned long)exe_dos_header_bss_max_size(&exehdr) - 1UL;
            printf("  * resident bss range (max):     from base_seg+0x%04X:0x%04X\n",
                (unsigned int)(start>>4UL),(unsigned int)(start&0xFUL));
            printf("                                    to base_seg+0x%04X:0x%04X\n",
                (unsigned int)(end>>4UL),(unsigned int)(end&0xFUL));
        }

        // NTS: convert seg:off to offset, mask by 0xFFFFFUL to support COM to EXE conversions
        //      or somesuch that set the entry point to 0xFFEF:0x100. if you convert that such
        //      it tricks EXE loading to set CS:IP like a .COM executable.
        start =
            (((unsigned long)exehdr.init_code_segment << 4UL) +
              (unsigned long)exehdr.init_instruction_pointer) & 0xFFFFFUL;
        printf("  * code pointer file offset:     %lu + %lu = %lu bytes\n",
            start,
            (unsigned long)exe_dos_header_file_header_size(&exehdr),
            (unsigned long)exe_dos_header_file_header_size(&exehdr) + start);
        printf("                                  base_seg+0x%04X:0x%04X\n",
            (unsigned int)(start>>4UL),(unsigned int)(start&0xFUL));

        end =
            (unsigned long)exe_dos_header_file_resident_size(&exehdr) -
            (unsigned long)exe_dos_header_file_header_size(&exehdr);
        resend = end;
 
        // user may want to know if CS:IP points outside resident portion
        if (start >= end)
            printf("  ! CS:IP points outside resident portion (%lu >= %lu)\n",start,end);

        start =
            (((unsigned long)exehdr.init_stack_segment << 4UL) +
              (unsigned long)exehdr.init_stack_pointer) & 0xFFFFFUL;
        if (start < resend) {
            printf("  * stack pointer file offset:    %lu + %lu = %lu bytes\n",
                start,
                (unsigned long)exe_dos_header_file_header_size(&exehdr),
                (unsigned long)exe_dos_header_file_header_size(&exehdr) + start);
        }
        printf("  * stack pointer offset:         base_seg+0x%04X:0x%04X\n",
            (unsigned int)(start>>4UL),(unsigned int)(start&0xFUL));

        end =
            (unsigned long)exe_dos_header_file_resident_size(&exehdr) +
            (unsigned long)exe_dos_header_bss_max_size(&exehdr) -
            (unsigned long)exe_dos_header_file_header_size(&exehdr);
 
        // user may want to know if CS:IP points outside resident portion
        if (start >= end)
            printf("  ! SS:SP points outside resident+MAX BSS portion (%lu >= %lu)\n",start,end);

        // if the EXE header says min == 0, act as if min == 4KB.
        // some Watcom windows stubs set min == 0 then set SS:SP about 64 bytes past min BSS.
        end =
            (unsigned long)exe_dos_header_file_resident_size(&exehdr) +
            (unsigned long)exe_dos_header_bss_size(&exehdr) -
            (unsigned long)exe_dos_header_file_header_size(&exehdr);
        if ((unsigned long)exe_dos_header_bss_size(&exehdr) == 0)
            end += 4096UL;
 
        // user may want to know if SS:SP points outside resident+BSS portion.
        // we allow SS:SP to sit right at the first byte past BSS, because then a push
        // operation brings it within range (some Windows stubs set SS:SP that).
        if (start > end)
            printf("  ! SS:SP points outside resident+MIN BSS portion (%lu > %lu)\n",start,end);
    }
    else {
        printf("  * no resident portion\n");
    }

    if (exehdr.number_of_relocations != 0U) {
        unsigned int left = exehdr.number_of_relocations;
        unsigned int i;

        if (lseek(src_fd,exehdr.relocation_table_offset,SEEK_SET) != (off_t)exehdr.relocation_table_offset)
            return 1;

        printf("  * Relocation table:\n");
        while (left > 0) {
            relocentcount = left;
            if (relocentcount > relocentmax) relocentcount = relocentmax;

            assert((relocentcount*4) <= sizeof(relocent));
            if (read(src_fd,relocent,relocentcount*4) != (int)(relocentcount*4))
                return 1;

            for (i=0;i < relocentcount;i++) {
                uint32_t ent = relocent[i];
                uint16_t segv = (uint16_t)(ent >> 16UL);
                uint16_t ofsv = (uint16_t)(ent & 0xFFFFUL);
                unsigned long resof = (((unsigned long)segv << 4UL) + (unsigned long)ofsv) & 0xFFFFFUL;

                printf("    base_seg+0x%04X:0x%04X (base_loc+%lu, file_ofs=%lu)\n",
                    segv,ofsv,resof,
                    (unsigned long)exe_dos_header_file_header_size(&exehdr) + resof);
            }

            left -= relocentcount;
        }
    }
#endif

    close(src_fd);
    return 0;
}
