
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

static char*                    src_file = NULL;
static int                      src_fd = -1;

#if defined(LINUX) || defined(__FLAT__)
# define relocentmax            8192
#elif defined(__LARGE__)
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
    printf("                                  ^  x  = %lu x 512 = %lu\n",
        (unsigned long)exehdr.exe_file_blocks,
        (unsigned long)exehdr.exe_file_blocks * 512UL);
    if (exehdr.last_block_bytes != 0U && exehdr.exe_file_blocks != 0U) {
        printf("                                  ^ (x -= 512) = %lu, last block not full 512 bytes\n",
            (unsigned long)exehdr.exe_file_blocks * 512UL - 512UL);
        printf("                                  ^ (x += %lu) = %lu, add last block bytes\n",
            (unsigned long)exehdr.last_block_bytes,
            ((unsigned long)exehdr.exe_file_blocks * 512UL) + (unsigned long)exehdr.last_block_bytes - 512UL);
    }
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
        unsigned long t;

        printf("  * resident portion (runtime):   %lu - %lu bytes (inclusive) = %lu bytes\n",
            (unsigned long)exelayout.run_resident.start,
            (unsigned long)exelayout.run_resident.end,
            (unsigned long)exe_dos_layout_range_get_length(&exelayout.run_resident));
        printf("                             from base_seg+0x%04X:0x%04X (inclusive)\n",
            0,0);
        t = exelayout.run_resident.end - exelayout.run_resident.start;
        printf("                               to base_seg+0x%04X:0x%04X\n",
            (unsigned int)(t >> 4UL),(unsigned int)(t & 0xFUL));
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
    if (exelayout.min_mem_footprint != 0UL) {
        unsigned long t,t2;

        t = exe_dos_layout_range_get_length(&exelayout.run_resident);
        t2 = exelayout.min_mem_footprint - 1UL;
        if (t2 > 0xFFFFFUL) t2 = 0xFFFFFUL;
        printf("                             from base_seg+0x%04X:0x%04X (inclusive)\n",
            (unsigned int)(t >> 4UL),(unsigned int)(t & 0xFUL));
        printf("                               to base_seg+0x%04X:0x%04X\n",
            (unsigned int)(t2 >> 4UL),(unsigned int)(t2 & 0xFUL));
    }

    printf("  * maximum memory footprint:     %lu bytes\n",
        (unsigned long)exelayout.max_mem_footprint);
    if (exelayout.max_mem_footprint != 0UL) {
        unsigned long t,t2;

        t = exe_dos_layout_range_get_length(&exelayout.run_resident);
        t2 = exelayout.max_mem_footprint - 1UL;
        if (t2 > 0xFFFFFUL) t2 = 0xFFFFFUL;
        printf("                             from base_seg+0x%04X:0x%04X (inclusive)\n",
            (unsigned int)(t >> 4UL),(unsigned int)(t & 0xFUL));
        printf("                               to base_seg+0x%04X:0x%04X\n",
            (unsigned int)(t2 >> 4UL),(unsigned int)(t2 & 0xFUL));
    }

    printf("  * code pointer file offset:     %lu(resident) + %lu = %lu(file) bytes\n",
        (unsigned long)exelayout.code_entry_point_res,
        (unsigned long)exelayout.run_resident.start,
        (unsigned long)exelayout.code_entry_point_file);
    printf("                                  base_seg+0x%04X:0x%04X\n",
        (unsigned int)(exelayout.code_entry_point_res>>4UL),
        (unsigned int)(exelayout.code_entry_point_res&0xFUL));

    if (exelayout.code_entry_point_res > exelayout.run_resident.end)
        printf("  ! CS:IP points outside resident portion (%lu > %lu(last byte))\n",
            (unsigned long)exelayout.code_entry_point_res,
            (unsigned long)exelayout.run_resident.end);

    if (exelayout.stack_entry_point_res > exelayout.run_resident.end) {
        /* most common */
        printf("  * stack pointer file offset:    %lu(resident) bytes (BSS beyond resident)\n",
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
            printf("    but not far enough to be a problem unless low memory pressure.\n");
        }
        else {
            printf("  ! SS:SP points outside minimum memory footprint (%lu > %lu(bytes))\n",
                (unsigned long)exelayout.stack_entry_point_res,
                (unsigned long)exelayout.min_mem_footprint);
        }
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

                if ((resof+2UL) > exelayout.min_mem_footprint) {
                    /* Fatal! */
                    printf("  ! relocation points outside minimum memory rootprint\n");
                }
                else if ((resof+2UL) > exe_dos_layout_range_get_length(&exelayout.run_resident)) {
                    /* Not fatal, but unusual and noteworthy */
                    printf("  * relocation points outside resident image into BSS area\n");
                }
            }

            left -= relocentcount;
        }
    }

    close(src_fd);
    return 0;
}
