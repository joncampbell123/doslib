
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

static struct exe_dos_header    exehdr;

static void help(void) {
    fprintf(stderr,"EXEHDMP -i <exe file>\n");
}

int main(int argc,char **argv) {
    char *a;
    int i;

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

    if (sizeof(exehdr) != 0x1C) {
        fprintf(stderr,"EXE header sizeof error\n");
        return 1;
    }

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

    printf("  * exe header portion:           %lu - %lu bytes (inclusive) = %lu bytes\n",
        0UL,
        (unsigned long)exe_dos_header_file_header_size(&exehdr) - 1UL,
        (unsigned long)exe_dos_header_file_header_size(&exehdr));

    if (exehdr.number_of_relocations != 0U) {
        unsigned long start,end;

        start = (unsigned long)exehdr.relocation_table_offset;
        end = (unsigned long)exehdr.relocation_table_offset + (exehdr.number_of_relocations * 4UL) - 1UL;

        printf("  * exe relocation table:         %lu - %lu bytes (inclusive) = %lu bytes\n",
            start,
            end,
            end + 1UL - start);

        if (start < 0x1CUL || (end+1UL) > exe_dos_header_file_header_size(&exehdr))
            printf("  ! WARNING: EXE relocation table lies out of range\n");
    }

    if (exe_dos_header_file_resident_size(&exehdr) > exe_dos_header_file_header_size(&exehdr)) {
        unsigned long start,end;

        printf("  * resident size:                %lu(blk) - %lu(hdr) bytes = %lu bytes\n",
            (unsigned long)exe_dos_header_file_resident_size(&exehdr),
            (unsigned long)exe_dos_header_file_header_size(&exehdr),
            (unsigned long)exe_dos_header_file_resident_size(&exehdr) -
            (unsigned long)exe_dos_header_file_header_size(&exehdr));
        printf("  * resident portion:             %lu - %lu bytes (inclusive) = %lu bytes\n",
            (unsigned long)exe_dos_header_file_header_size(&exehdr),
            (unsigned long)exe_dos_header_file_resident_size(&exehdr) - 1UL,
            (unsigned long)exe_dos_header_file_resident_size(&exehdr) -
            (unsigned long)exe_dos_header_file_header_size(&exehdr));
        printf("  * minimum memory footprint:     %lu bytes\n",
            (unsigned long)exe_dos_header_file_resident_size(&exehdr) +
            (unsigned long)exe_dos_header_bss_size(&exehdr) -
            (unsigned long)exe_dos_header_file_header_size(&exehdr));
        printf("  * maximum memory footprint:     %lu bytes\n",
            (unsigned long)exe_dos_header_file_resident_size(&exehdr) +
            (unsigned long)exe_dos_header_bss_max_size(&exehdr) -
            (unsigned long)exe_dos_header_file_header_size(&exehdr));

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
 
        // user may want to know if CS:IP points outside resident portion
        if (start >= end)
            printf("  ! CS:IP points outside resident portion (%lu >= %lu)\n",start,end);

        start =
            (((unsigned long)exehdr.init_stack_segment << 4UL) +
              (unsigned long)exehdr.init_stack_pointer) & 0xFFFFFUL;
        printf("  * stack pointer file offset:    %lu + %lu = %lu bytes\n",
            start,
            (unsigned long)exe_dos_header_file_header_size(&exehdr),
            (unsigned long)exe_dos_header_file_header_size(&exehdr) + start);
        printf("                                  base_seg+0x%04X:0x%04X\n",
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

    close(src_fd);
    return 0;
}
