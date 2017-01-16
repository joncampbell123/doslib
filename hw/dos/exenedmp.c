
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

#include <hw/dos/exehdr.h>
#include <hw/dos/exenehdr.h>

#ifndef O_BINARY
#define O_BINARY (0)
#endif

static char*                    src_file = NULL;
static int                      src_fd = -1;

static struct exe_dos_header    exehdr;
static struct exe_dos_layout    exelayout;

static void help(void) {
    fprintf(stderr,"EXENEDMP -i <exe file>\n");
}

int main(int argc,char **argv) {
    struct exe_ne_header ne_header;
    uint32_t ne_header_offset;
    uint32_t file_size;
    char *a;
    int i;

    assert(sizeof(ne_header) == 0x40);
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

    file_size = lseek(src_fd,0,SEEK_END);
    lseek(src_fd,0,SEEK_SET);

    if (read(src_fd,&exehdr,sizeof(exehdr)) != (int)sizeof(exehdr)) {
        fprintf(stderr,"EXE header read error\n");
        return 1;
    }

    if (exehdr.magic != 0x5A4DU/*MZ*/) {
        fprintf(stderr,"EXE header signature missing\n");
        return 1;
    }

    printf("File size:                        %lu bytes\n",
        (unsigned long)file_size);
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
        fprintf(stderr,"EXE layout not appropriate for Windows NE\n");
        return 1;
    }

    if (!exe_header_can_contain_exe_extension(&exehdr)) {
        fprintf(stderr,"EXE header cannot contain extension\n");
        return 1;
    }

    /* go read the extension */
    if (lseek(src_fd,EXE_HEADER_EXTENSION_OFFSET,SEEK_SET) != EXE_HEADER_EXTENSION_OFFSET ||
        read(src_fd,&ne_header_offset,4) != 4) {
        fprintf(stderr,"Cannot read extension\n");
        return 1;
    }
    printf("    EXE extension (if exists) at: %lu\n",(unsigned long)ne_header_offset);
    if ((ne_header_offset+EXE_HEADER_NE_HEADER_SIZE) >= file_size) {
        printf("! NE header not present (offset out of range)\n");
        return 0;
    }

    /* go read the extended header */
    if (lseek(src_fd,ne_header_offset,SEEK_SET) != ne_header_offset ||
        read(src_fd,&ne_header,sizeof(ne_header)) != sizeof(ne_header)) {
        fprintf(stderr,"Cannot read NE header\n");
        return 1;
    }
    if (ne_header.signature != EXE_NE_SIGNATURE) {
        fprintf(stderr,"Not an NE executable\n");
        return 1;
    }

    printf("Windows or OS/2 NE header:\n");
    printf("    Linker version:               %u.%u\n",
        ne_header.linker_version,
        ne_header.linker_revision);
    printf("    Entry table offset:           %u NE-rel (abs=%lu) bytes\n",
        ne_header.entry_table_offset,
        (unsigned long)ne_header.entry_table_offset + (unsigned long)ne_header_offset);
    printf("    Entry table length:           %lu bytes\n",
        (unsigned long)ne_header.entry_table_length);
    printf("    32-bit file CRC:              0x%08lx\n",
        (unsigned long)ne_header.file_crc);
    printf("    EXE content flags:            0x%04x\n",
        (unsigned int)ne_header.flags);

    printf("        DGROUP type:              %u",
        (unsigned int)ne_header.flags & EXE_NE_HEADER_FLAGS_DGROUPTYPE_MASK);
    switch (ne_header.flags & EXE_NE_HEADER_FLAGS_DGROUPTYPE_MASK) {
        case EXE_NE_HEADER_FLAGS_DGROUPTYPE_NOAUTODATA:
            printf(" NOAUTODATA\n");
            break;
        case EXE_NE_HEADER_FLAGS_DGROUPTYPE_SINGLEDATA:
            printf(" SINGLEDATA\n");
            break;
        case EXE_NE_HEADER_FLAGS_DGROUPTYPE_MULTIPLEDATA:
            printf(" MULTIPLEDATA\n");
            break;
        default:
            printf(" ?\n");
            break;
    };

    if (ne_header.flags & EXE_NE_HEADER_FLAGS_GLOBAL_INIT)
        printf("        GLOBAL_INIT\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_PROTECTED_MODE_ONLY)
        printf("        PROTECTED_MODE_ONLY\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_8086)
        printf("        Has 8086 instructions\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_80286)
        printf("        Has 80286 instructions\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_80386)
        printf("        Has 80386 instructions\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_80x87)
        printf("        Has 80x87 (FPU) instructions\n");

    printf("        Application type:         %u",
        ((unsigned int)ne_header.flags & EXE_NE_HEADER_FLAGS_APPTYPE_MASK) >> EXE_NE_HEADER_FLAGS_APPTYPE_SHIFT);
    switch (ne_header.flags & EXE_NE_HEADER_FLAGS_APPTYPE_MASK) {
        case EXE_NE_HEADER_FLAGS_APPTYPE_NONE:
            printf(" NONE\n");
            break;
        case EXE_NE_HEADER_FLAGS_APPTYPE_FULLSCREEN:
            printf(" FULLSCREEN\n");
            break;
        case EXE_NE_HEADER_FLAGS_APPTYPE_WINPM_COMPAT:
            printf(" WINPM_COMPAT\n");
            break;
        case EXE_NE_HEADER_FLAGS_APPTYPE_WINPM_USES:
            printf(" WINPM_USES\n");
            break;
        default:
            printf(" ?\n");
            break;
    };

    if (ne_header.flags & EXE_NE_HEADER_FLAGS_FIRST_SEGMENT_CODE_APP_LOAD)
        printf("        FIRST_SEGMENT_CODE_APP_LOAD / OS2_FAMILY_APP\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_LINK_ERRORS)
        printf("        Link errors\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_NON_CONFORMING)
        printf("        Non-conforming\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_DLL)
        printf("        DLL or driver\n");

    printf("    AUTODATA segment index:       %u\n",
        ne_header.auto_data_segment_number);
    printf("    Initial heap size:            %u\n",
        ne_header.init_local_heap);
    printf("    Initial stack size:           %u\n",
        ne_header.init_stack_size);
    printf("    CS:IP                         segment #%u : 0x%04x\n",
        ne_header.entry_cs,
        ne_header.entry_ip);
    if (ne_header.entry_ss == 0 && ne_header.entry_sp == 0) {
        printf("    SS:SP                         segment AUTODATA : sizeof(AUTODATA) + sizeof(stack)\n");
    }
    else {
        printf("    SS:SP                         segment #%u : 0x%04x\n",
            ne_header.entry_ss,
            ne_header.entry_sp);
    }
    printf("    Segment table entries:        %u\n",
        ne_header.segment_table_entries);
    printf("    Module ref. table entries:    %u\n",
        ne_header.module_reftable_entries);
    printf("    Non-resident name table len:  %u bytes\n",
        ne_header.nonresident_name_table_length);
    printf("    Segment table offset:         %u NE-rel (abs %lu) bytes\n",
        ne_header.segment_table_offset,
        (unsigned long)ne_header.segment_table_offset + (unsigned long)ne_header_offset);
    printf("    Resource table offset:        %u NE-rel (abs %lu) bytes\n",
        ne_header.resource_table_offset,
        (unsigned long)ne_header.resource_table_offset + (unsigned long)ne_header_offset);
    printf("    Resident name table offset:   %u NE-rel (abs %lu) bytes\n",
        ne_header.resident_name_table_offset,
        (unsigned long)ne_header.resident_name_table_offset + (unsigned long)ne_header_offset);
    printf("    Module ref. table offset:     %u NE-rel (abs %lu) bytes\n",
        ne_header.module_reference_table_offset,
        (unsigned long)ne_header.module_reference_table_offset + (unsigned long)ne_header_offset);
    printf("    Imported name table offset:   %u NE-rel (abs %lu) bytes\n",
        ne_header.imported_name_table_offset,
        (unsigned long)ne_header.imported_name_table_offset + (unsigned long)ne_header_offset);
    printf("    Non-resident name table offset:%lu bytes\n",
        (unsigned long)ne_header.nonresident_name_table_offset);
    printf("    Movable entry points:         %u\n",
        ne_header.movable_entry_points);
    printf("    Sector shift:                 %u (1 sector << %u = %lu bytes)\n",
        ne_header.sector_shift,
        ne_header.sector_shift,
        1UL << (unsigned long)ne_header.sector_shift);
    printf("    Number of resource segments:  %u\n",
        ne_header.resource_segments);
    printf("    Target OS:                    0x%02x ",ne_header.target_os);
    switch (ne_header.target_os) {
        case EXE_NE_HEADER_TARGET_OS_NONE:
            printf("None / Windows 2.x or earlier");
            break;
        case EXE_NE_HEADER_TARGET_OS_OS2:
            printf("OS/2");
            break;
        case EXE_NE_HEADER_TARGET_OS_WINDOWS:
            printf("Windows (3.x and later)");
            break;
        case EXE_NE_HEADER_TARGET_OS_EURO_MSDOS_4:
            printf("European MS-DOS 4.0");
            break;
        case EXE_NE_HEADER_TARGET_OS_WINDOWS_386:
            printf("Windows 386");
            break;
        case EXE_NE_HEADER_TARGET_OS_BOSS:
            printf("Boss (Borland)");
            break;
        default:
            printf("?");
            break;
    }
    printf("\n");

    printf("    Other flags:                  0x%04x\n",ne_header.other_flags);
    if (ne_header.other_flags & EXE_NE_HEADER_OTHER_FLAGS_WIN_WIN2X_IN_3X)
        printf("        Windows 2.x can run in Windows 3.x protected mode\n");
    if (ne_header.other_flags & EXE_NE_HEADER_OTHER_FLAGS_WIN_WIN2X_PROP_FONTS)
        printf("        Windows 2.x / OS/2 app supports proportional fonts\n");
    if (ne_header.other_flags & EXE_NE_HEADER_OTHER_FLAGS_WIN_HAS_FASTLOAD)
        printf("        Has a fast-load / gang-load area\n");

    if (ne_header.sector_shift > 16U) {
        printf("* ERROR: Sector shift is too large\n");
        return 1;
    }

    printf("    Fastload offset:              %u sectors (%lu bytes)\n",
        ne_header.fastload_offset_sectors,
        (unsigned long)ne_header.fastload_offset_sectors << (unsigned long)ne_header.sector_shift);
    printf("    Fastload length:              %u sectors (%lu bytes)\n",
        ne_header.fastload_length_sectors,
        (unsigned long)ne_header.fastload_length_sectors << (unsigned long)ne_header.sector_shift);
    printf("    Minimum code swap area size:  %u\n", // unknown units
        ne_header.minimum_code_swap_area_size);
    printf("    Minimum Windows version:      %u.%u\n",
        (ne_header.minimum_windows_version >> 8U),
        ne_header.minimum_windows_version & 0xFFU);
    if (ne_header.minimum_windows_version == 0x30A)
        printf("        * Microsoft Windows 3.1\n");
    else if (ne_header.minimum_windows_version == 0x300)
        printf("        * Microsoft Windows 3.0\n");

    close(src_fd);
    return 0;
}
