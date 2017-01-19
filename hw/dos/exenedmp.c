
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

static unsigned char            opt_sort_ordinal = 0;
static unsigned char            opt_sort_names = 0;

static char*                    src_file = NULL;
static int                      src_fd = -1;

static struct exe_dos_header    exehdr;
static struct exe_dos_layout    exelayout;

static void help(void) {
    fprintf(stderr,"EXENEDMP -i <exe file>\n");
    fprintf(stderr," -sn        Sort names\n");
    fprintf(stderr," -so        Sort by ordinal\n");
}

//////////////////

#pragma pack(push,1)
struct exe_ne_header_name_entry {
    uint8_t         length;
    uint16_t        offset;             // 16-bit because resident/nonresident name table is limited to 64KB
    uint16_t        ordinal;
};
#pragma pack(pop)

static size_t ne_name_entry_sort_by_name_raw_length = 0;
static unsigned char *ne_name_entry_sort_by_name_raw = NULL;
int ne_name_entry_sort_by_name(const void *a,const void *b) {
    struct exe_ne_header_name_entry *ea = (struct exe_ne_header_name_entry *)a;
    struct exe_ne_header_name_entry *eb = (struct exe_ne_header_name_entry *)b;
    unsigned char *pa,*pb;
    unsigned int i;

    pa = (unsigned char*)(ne_name_entry_sort_by_name_raw+ea->offset);
    pb = (unsigned char*)(ne_name_entry_sort_by_name_raw+eb->offset);

    for (i=0;i < ea->length && i < eb->length;i++) {
        int diff = (int)pa[i] - (int)pb[i];
        if (diff != 0) return diff;
    }

    if (ea->length == eb->length)
        return 0;
    else if (i < ea->length)
        return (int)pa[i];
    else /*if (i < eb->length)*/
        return 0 - (int)pb[i];
}

int ne_name_entry_sort_by_ordinal(const void *a,const void *b) {
    struct exe_ne_header_name_entry *ea = (struct exe_ne_header_name_entry *)a;
    struct exe_ne_header_name_entry *eb = (struct exe_ne_header_name_entry *)b;
    return ea->ordinal - eb->ordinal;
}

///////////////////

int main(int argc,char **argv) {
    unsigned int ne_nonresname_length = 0;
    unsigned char *ne_nonresname_raw = NULL;
    struct exe_ne_header_name_entry *ne_nonresname = NULL;

    unsigned int ne_resname_length = 0;
    unsigned char *ne_resname_raw = NULL;
    unsigned short ne_resname_raw_length = 0;
    struct exe_ne_header_name_entry *ne_resname = NULL;

    struct exe_ne_header_segment_entry *ne_segments = NULL;
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
            else if (!strcmp(a,"sn")) {
                opt_sort_names = 1;
            }
            else if (!strcmp(a,"so")) {
                opt_sort_ordinal = 1;
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

    if (ne_header.sector_shift == 0U) {
        // NTS: One reference suggests that sector_shift == 0 means sector_shift == 9
        printf("* ERROR: Sector shift is zero\n");
        return 1;
    }
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

    /* this code makes a few assumptions about the header that are used to improve
     * performance when reading the header. if the header violates those assumptions,
     * say so NOW */
    if (ne_header.segment_table_offset < 0x40) { // if the segment table collides with the NE header we want the user to know
        printf("! WARNING: Segment table collides with the NE header (offset %u < 0x40)\n",
            ne_header.segment_table_offset);
    }
    /* even though the NE header specifies key structures by offset from NE header,
     * they seem to follow a strict ordering as described in Windows NE notes.txt.
     * some fields have an offset, not a size, and we assume that we can determine
     * the size of those fields by the difference between offsets. */
    /* RESIDENT_NAME_TABLE_SIZE = module_reference_table_offset - resident_name_table_offset */
    if (ne_header.module_reference_table_offset < ne_header.resident_name_table_offset)
        printf("! WARNING: Module ref. table offset < Resident name table offset\n");
    /* IMPORTED_NAME_TABLE_SIZE = entry_table_offset - imported_name_table_offset */
    if (ne_header.entry_table_offset < ne_header.imported_name_table_offset)
        printf("! WARNING: Entry table offset < Imported name table offset\n");
    /* and finally, we assume we can determine the size of the NE header + resident structures by:
     * 
     * NE_RESIDENT_PORTION_HEADER_SIZE = non_resident_name_table_offset - ne_header_offset
     *
     * and therefore, the NE-relative offsets listed (adjusted to absolute file offset) will be less than the non-resident name table offset.
     *
     * I'm guessing that the reason some offsets are NE-relative and some are absolute, is because Microsoft probably
     * intended to be able to read all the resident portion NE structures at once, before concerning itself with
     * nonresident portions like the nonresident name table.
     *
     * FIXME: Do you suppose some clever NE packing tool might stick the non-resident name table in the MS-DOS stub?
     *        What does Windows do if you do that? Does Windows make the same assumption/optimization when loading NE executables? */
    if (ne_header.nonresident_name_table_offset < (ne_header_offset + 0x40UL))
        printf("! WARNING: Non-resident name table offset too small (would overlap NE header)\n");
    else {
        unsigned long min_offset = ne_header.segment_table_offset + (sizeof(struct exe_ne_header_segment_entry) * ne_header.segment_table_entries);
        if (min_offset < ne_header.resource_table_offset)
            min_offset = ne_header.resource_table_offset;
        if (min_offset < ne_header.resident_name_table_offset)
            min_offset = ne_header.resident_name_table_offset;
        if (min_offset < (ne_header.module_reference_table_offset+(2UL * ne_header.module_reftable_entries)))
            min_offset = (ne_header.module_reference_table_offset+(2UL * ne_header.module_reftable_entries));
        if (min_offset < ne_header.imported_name_table_offset)
            min_offset = ne_header.imported_name_table_offset;
        if (min_offset < (ne_header.entry_table_offset+ne_header.entry_table_length))
            min_offset = (ne_header.entry_table_offset+ne_header.entry_table_length);

        min_offset += ne_header_offset;
        if (ne_header.nonresident_name_table_offset < min_offset) {
            printf("! WARNING: Non-resident name table offset overlaps NE resident tables (%lu < %lu)\n",
                (unsigned long)ne_header.nonresident_name_table_offset,min_offset);
        }
    }

    if (ne_header.segment_table_offset > ne_header.resource_table_offset)
        printf("! WARNING: segment table offset > resource table offset");
    if (ne_header.resource_table_offset > ne_header.resident_name_table_offset)
        printf("! WARNING: resource table offset > resident name table offset");
    if (ne_header.resident_name_table_offset > ne_header.module_reference_table_offset)
        printf("! WARNING: resident name table offset > module reference table offset");
    if (ne_header.module_reference_table_offset > ne_header.imported_name_table_offset)
        printf("! WARNING: module reference table offset > imported name table offset");
    if (ne_header.imported_name_table_offset > ne_header.entry_table_offset)
        printf("! WARNING: imported name table offset > entry table offset");

    /* RESIDENT_NAME_TABLE_SIZE = module_reference_table_offset - resident_name_table_offset */
    if (ne_header.module_reference_table_offset > ne_header.resident_name_table_offset)
        ne_resname_raw_length = (unsigned short)(ne_header.module_reference_table_offset - ne_header.resident_name_table_offset);
    else
        ne_resname_raw_length = 0;

    printf("  * Resident name table length: %u\n",ne_resname_raw_length);

    /* load segment table */
    if (ne_header.segment_table_entries != 0 && ne_header.segment_table_offset != 0 &&
        (unsigned long)lseek(src_fd,ne_header.segment_table_offset + ne_header_offset,SEEK_SET) == ((unsigned long)ne_header.segment_table_offset + ne_header_offset)) {
        assert(sizeof(*ne_segments) == 8);
        ne_segments = (struct exe_ne_header_segment_entry*)malloc(sizeof(*ne_segments) * ne_header.segment_table_entries);
        if (ne_segments != NULL) {
            if ((size_t)read(src_fd,ne_segments,sizeof(*ne_segments) * ne_header.segment_table_entries) !=
                (sizeof(*ne_segments) * ne_header.segment_table_entries)) {
                printf("    ! Unable to read segment table\n");
                free(ne_segments);
                ne_segments = NULL;
            }
        }
    }

    /* load nonresident name table */
    if (ne_header.nonresident_name_table_offset != 0 && ne_header.nonresident_name_table_length != 0 &&
        (unsigned long)lseek(src_fd,ne_header.nonresident_name_table_offset,SEEK_SET) == ne_header.nonresident_name_table_offset) {
        unsigned char *base,*scan,*fence;

        base = malloc(ne_header.nonresident_name_table_length);
        if (base != NULL) {
            if ((unsigned long)read(src_fd,base,ne_header.nonresident_name_table_length) != ne_header.nonresident_name_table_length) {
                free(base);
                base = NULL;
            }
        }

        if (base != NULL) {
            unsigned int entries = 0;

            fence = base + ne_header.nonresident_name_table_length;
            for (scan=base;scan < fence;) {
                /* uint8_t         LENGTH
                 * char[length]    TEXT
                 * uint16_t        ORDINAL */
                unsigned char len = *scan++; // LENGTH
                if (len == 0) break;
                if ((scan+len+2) > fence) break;

                scan += len + 2; // TEXT + ORDINAL
                entries++;
            }

            ne_nonresname_length = entries;
            if (entries != 0) {
                printf("  * Non-resident name table has %u entries\n",entries);

                ne_nonresname = (struct exe_ne_header_name_entry*)malloc(entries * sizeof(*ne_nonresname));
                if (ne_nonresname != NULL) {
                    ne_nonresname_length = entries;
                    ne_nonresname_raw = base;

                    entries = 0;
                    for (scan=base;scan < fence && entries < ne_nonresname_length;) {
                        /* uint8_t         LENGTH
                         * char[length]    TEXT
                         * uint16_t        ORDINAL */
                        unsigned char len = *scan++; // LENGTH
                        if (len == 0) break;
                        if ((scan+len+2) > fence) break;

                        ne_nonresname[entries].length = len;

                        ne_nonresname[entries].offset = (uint16_t)(scan - base);
                        scan += len;    // TEXT

                        ne_nonresname[entries].ordinal = *((uint16_t*)scan);
                        scan += 2;      // ORDINAL

                        entries++;
                    }

                    if ((scan + 1) < fence)
                        printf("      ! Non-resident name table stopped %u bytes early\n",
                            (unsigned int)(fence - scan));

                    if (entries < ne_nonresname_length) {
                        fprintf(stderr,"WARNING: Names parsed are less than expected %u < %u\n",
                            (unsigned int)entries,
                            (unsigned int)ne_nonresname_length);

                        ne_nonresname_length = entries;
                    }

                    if (opt_sort_ordinal && ne_nonresname_length > 1) {
                        /* NTS: Do not sort the module name in entry 0 */
                        qsort(ne_nonresname+1,ne_nonresname_length-1,sizeof(*ne_nonresname),ne_name_entry_sort_by_ordinal);
                    }
                    else if (opt_sort_names && ne_nonresname_length > 1) {
                        ne_name_entry_sort_by_name_raw_length = ne_nonresname_length;
                        ne_name_entry_sort_by_name_raw = ne_nonresname_raw;
                        /* NTS: Do not sort the module name in entry 0 */
                        qsort(ne_nonresname+1,ne_nonresname_length-1,sizeof(*ne_nonresname),ne_name_entry_sort_by_name);
                    }
                }
                else {
                    free(base);
                    base = NULL;
                }
            }
            else {
                free(base);
                base = NULL;
            }
        }
    }

    /* load resident name table */
    if (ne_header.resident_name_table_offset != 0 && ne_resname_raw_length != 0 &&
        (unsigned long)lseek(src_fd,ne_header.resident_name_table_offset + ne_header_offset,SEEK_SET) == (ne_header.resident_name_table_offset + ne_header_offset)) {
        unsigned char *base,*scan,*fence;

        base = malloc(ne_resname_raw_length);
        if (base != NULL) {
            if ((unsigned long)read(src_fd,base,ne_resname_raw_length) != ne_resname_raw_length) {
                free(base);
                base = NULL;
            }
        }

        if (base != NULL) {
            unsigned int entries = 0;

            fence = base + ne_resname_raw_length;
            for (scan=base;scan < fence;) {
                /* uint8_t         LENGTH
                 * char[length]    TEXT
                 * uint16_t        ORDINAL */
                unsigned char len = *scan++; // LENGTH
                if (len == 0) break;
                if ((scan+len+2) > fence) break;

                scan += len + 2; // TEXT + ORDINAL
                entries++;
            }

            ne_resname_length = entries;
            if (entries != 0) {
                printf("  * Resident name table has %u entries\n",entries);

                ne_resname = (struct exe_ne_header_name_entry*)malloc(entries * sizeof(*ne_resname));
                if (ne_resname != NULL) {
                    ne_resname_length = entries;
                    ne_resname_raw = base;

                    entries = 0;
                    for (scan=base;scan < fence && entries < ne_resname_length;) {
                        /* uint8_t         LENGTH
                         * char[length]    TEXT
                         * uint16_t        ORDINAL */
                        unsigned char len = *scan++; // LENGTH
                        if (len == 0) break;
                        if ((scan+len+2) > fence) break;

                        ne_resname[entries].length = len;

                        ne_resname[entries].offset = (uint16_t)(scan - base);
                        scan += len;    // TEXT

                        ne_resname[entries].ordinal = *((uint16_t*)scan);
                        scan += 2;      // ORDINAL

                        entries++;
                    }

                    if ((scan + 1) < fence)
                        printf("      ! Resident name table stopped %u bytes early\n",
                            (unsigned int)(fence - scan));

                    if (entries < ne_resname_length) {
                        fprintf(stderr,"WARNING: Names parsed are less than expected %u < %u\n",
                            (unsigned int)entries,
                            (unsigned int)ne_resname_length);

                        ne_resname_length = entries;
                    }

                    if (opt_sort_ordinal && ne_resname_length > 1) {
                        /* NTS: Do not sort the module name in entry 0 */
                        qsort(ne_resname+1,ne_resname_length-1,sizeof(*ne_resname),ne_name_entry_sort_by_ordinal);
                    }
                    else if (opt_sort_names && ne_resname_length > 1) {
                        ne_name_entry_sort_by_name_raw_length = ne_resname_length;
                        ne_name_entry_sort_by_name_raw = ne_resname_raw;
                        /* NTS: Do not sort the module name in entry 0 */
                        qsort(ne_resname+1,ne_resname_length-1,sizeof(*ne_resname),ne_name_entry_sort_by_name);
                    }
                }
                else {
                    free(base);
                    base = NULL;
                }
            }
            else {
                free(base);
                base = NULL;
            }
        }
    }

    /* non-resident name table */
    if (ne_nonresname != NULL) {
        struct exe_ne_header_name_entry *ent = NULL;
        char tmp[255+1];
        unsigned int i;

        printf("    Non-resident name table, %u entries\n",
            (unsigned int)ne_nonresname_length);

        for (i=0;i < ne_nonresname_length;i++) {
            ent = ne_nonresname + i;

            assert((ent->offset+ent->length) <= ne_header.nonresident_name_table_length);

            tmp[ent->length] = 0;
            if (ent->length != 0)
                memcpy(tmp,ne_nonresname_raw+ent->offset,ent->length);

            if (i == 0) {
                printf("        Module name:    '%s'\n",tmp);
                if (ent->ordinal != 0)
                    printf("        ! WARNING: Module name with ordinal != 0\n");
            }
            else
                printf("        Ordinal #%-5d: '%s'\n",ent->ordinal,tmp);
        }
    }

    /* resident name table */
    if (ne_resname != NULL) {
        struct exe_ne_header_name_entry *ent = NULL;
        char tmp[255+1];
        unsigned int i;

        printf("    Resident name table, %u entries\n",
            (unsigned int)ne_resname_length);

        for (i=0;i < ne_resname_length;i++) {
            ent = ne_resname + i;

            assert((ent->offset+ent->length) <= ne_resname_raw_length);

            tmp[ent->length] = 0;
            if (ent->length != 0)
                memcpy(tmp,ne_resname_raw+ent->offset,ent->length);

            if (i == 0) {
                printf("        Module name:    '%s'\n",tmp);
                if (ent->ordinal != 0)
                    printf("        ! WARNING: Module name with ordinal != 0\n");
            }
            else
                printf("        Ordinal #%-5d: '%s'\n",ent->ordinal,tmp);
        }
    }

    /* dump segment table */
    if (ne_segments != NULL) {
        struct exe_ne_header_segment_entry *segent;
        unsigned int i;

        printf("    Segment table, %u entries:\n",ne_header.segment_table_entries);
        for (i=0;i < ne_header.segment_table_entries;i++) {
            segent = ne_segments + i; /* C pointer math, becomes (char*)ne_segments + (i * sizeof(*ne_segments)) */

            printf("        Segment #%d:\n",i+1);
            if (segent->offset_in_segments != 0U) {
                printf("            Offset: %u segments = %u << %u = %lu bytes\n",
                    segent->offset_in_segments,
                    segent->offset_in_segments,
                    ne_header.sector_shift,
                    (unsigned long)segent->offset_in_segments << (unsigned long)ne_header.sector_shift);
            }
            else {
                printf("            Offset: None (no data)\n");
            }

            printf("            Length: %lu bytes\n",
                (segent->offset_in_segments != 0 && segent->length == 0) ? 0x10000UL : (unsigned long)segent->length);
            printf("            Flags: 0x%04x",
                segent->flags);

            if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_DATA)
                printf(" DATA");
            else
                printf(" CODE");
            if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_ALLOCATED)
                printf(" ALLOCATED");
            if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_LOADED)
                printf(" LOADED");
            if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_MOVEABLE)
                printf(" MOVEABLE");
            else
                printf(" FIXED");
            if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_SHAREABLE)
                printf(" SHAREABLE");
            else
                printf(" NONSHAREABLE");
            if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_PRELOAD)
                printf(" PRELOAD");
            else
                printf(" LOADONCALL");
            if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_RELOCATIONS)
                printf(" HAS_RELOCATIONS");
            if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_DISCARDABLE)
                printf(" DISCARDABLE");

            if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_DATA) {
                if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_RO_EX)
                    printf(" READONLY");
                else
                    printf(" READWRITE");
            }
            else {
                if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_RO_EX)
                    printf(" EXECUTEONLY");
                else
                    printf(" READONLY");
            }

            printf("\n");

            printf("            Minimum allocation size: %lu\n",
                (segent->minimum_allocation_size == 0) ? 0x10000UL : (unsigned long)segent->minimum_allocation_size);
        }
    }

    /* dump segment relocation table */
    if (ne_segments != NULL) {
        struct exe_ne_header_segment_entry *segent;
        unsigned int i;

        printf("    Segment relocations:\n");
        for (i=0;i < ne_header.segment_table_entries;i++) {
            segent = ne_segments + i; /* C pointer math, becomes (char*)ne_segments + (i * sizeof(*ne_segments)) */

            if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_RELOCATIONS) {
                if (segent->offset_in_segments != 0 && segent->length != 0) {
                    union exe_ne_header_segment_relocation_entry relocent;
                    unsigned long reloc_offset =
                        ((unsigned long)segent->offset_in_segments << (unsigned long)ne_header.sector_shift) +
                        (unsigned long)segent->length;
                    unsigned short reloc_entries;
                    unsigned int relent;

                    assert(sizeof(relocent) == 8);
                    if ((unsigned long)lseek(src_fd,reloc_offset,SEEK_SET) != reloc_offset)
                        break;
                    if (read(src_fd,&reloc_entries,2) != 2)
                        break;

                    printf("        Segment #%d:\n",i+1);
                    printf("            Relocation table at: %lu, %u entries\n",reloc_offset,reloc_entries);
                    for (relent=0;relent < reloc_entries;relent++) {
                        if (read(src_fd,&relocent,sizeof(relocent)) != sizeof(relocent))
                            break;

                        printf("                At 0x%04x: ",relocent.r.seg_offset);
                        switch (relocent.r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_MASK) {
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_INTERNAL_REFERENCE:
                                printf("Internal ref");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_ORDINAL:
                                printf("Import by ordinal");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_NAME:
                                printf("Import by name");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_OSFIXUP:
                                printf("OSFIXUP");
                                break;
                        }
                        if (relocent.r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_ADDITIVE)
                            printf(" (ADDITIVE)");
                        printf(" ");

                        switch (relocent.r.reloc_address_type&EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_MASK) {
                            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_OFFSET_LOBYTE:
                                printf("addr=OFFSET_LOBYTE");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_SEGMENT:
                                printf("addr=SEGMENT");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_FAR_POINTER:
                                printf("addr=FAR_POINTER(16:16)");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_OFFSET:
                                printf("addr=OFFSET");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_FAR48_POINTER:
                                printf("addr=FAR_POINTER(16:32)");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_OFFSET32:
                                printf("addr=OFFSET32");
                                break;
                            default:
                                printf("addr=0x%02x",relocent.r.reloc_address_type);
                                break;
                        }
                        printf("\n");

                        switch (relocent.r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_MASK) {
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_INTERNAL_REFERENCE:
                                if (relocent.intref.segment_index == 0xFF) {
                                    printf("                    Refers to movable segment, entry ordinal #%d\n",
                                        relocent.movintref.entry_ordinal);
                                }
                                else {
                                    printf("                    Refers to segment #%d : 0x%04x\n",
                                        relocent.intref.segment_index,
                                        relocent.intref.seg_offset);
                                }
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_ORDINAL:
                                printf("                    Refers to module reference #%d, ordinal %d\n",
                                    relocent.ordinal.module_reference_index,
                                    relocent.ordinal.ordinal);
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_NAME:
                                printf("                    Refers to module reference #%d, imp name offset %d\n",
                                    relocent.name.module_reference_index,
                                    relocent.name.imported_name_offset);
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_OSFIXUP:
                                printf("                    OSFIXUP type=0x%04x\n",
                                    relocent.osfixup.fixup);
                                break;
                        }
                    }
                }
            }
        }
    }

    /* dump the entry table */
    if (ne_header.entry_table_offset != 0 && ne_header.entry_table_length != 0 &&
        lseek(src_fd,ne_header.entry_table_offset + ne_header_offset,SEEK_SET) == (ne_header.entry_table_offset + ne_header_offset)) {
        unsigned char *scan,*fence,*buf;

        buf = malloc(ne_header.entry_table_length);
        if (buf != NULL) {
            if (read(src_fd,buf,ne_header.entry_table_length) != ne_header.entry_table_length) {
                printf("    ! ERROR. Cannot read entry table\n");
                free(buf);
                buf = NULL;
            }
        }

        if (buf != NULL) {
            unsigned char segid;
            unsigned char flags;
            unsigned char nument,ent;
            unsigned short seg_offs;
            unsigned short int3f;
            unsigned short ordinal=1;

            printf("    Entry table:\n");

            scan = buf;
            fence = buf + ne_header.entry_table_length;

            while (scan < fence) {
                nument = *scan++;
                if (nument == 0) break;
                if (scan == fence) break;
                segid = *scan++;

                printf("        %u entries referring to ",nument);
                if (segid == 0xFF) {
                    printf("MOVEABLE segments\n");

                    for (ent=0;ent < nument;ent++) {
                        if ((scan+6) > fence) break;

                        flags = *scan++;
                        int3f = *((uint16_t*)scan); scan += 2;
                        segid = *scan++;
                        seg_offs = *((uint16_t*)scan); scan += 2;

                        printf("            Ordinal #%d ",ordinal++);
                        if (flags & 1) printf("EXPORTED ");
                        if (flags & 2) printf("USES_GLOBAL_SHARED_DATA_SEGMENT ");
                        if (flags & 0xF8) printf("RING_TRANSITION_STACK_WORDS=%u ",flags >> 3);
                        printf("\n");

                        printf("                INT 3Fh = 0x%02x 0x%02x\n",int3f & 0xFF,int3f >> 8);

                        printf("                segment #%u : 0x%04x\n",segid,seg_offs);
                    }
                }
                else if (segid == 0xFE) {
                    printf("CONSTANTS\n");

                    for (ent=0;ent < nument;ent++) {
                        if ((scan+3) > fence) break;

                        flags = *scan++;
                        seg_offs = *((uint16_t*)scan); scan += 2;

                        printf("            Ordinal #%d ",ordinal++);
                        if (flags & 1) printf("EXPORTED ");
                        if (flags & 2) printf("USES_GLOBAL_SHARED_DATA_SEGMENT ");
                        if (flags & 0xF8) printf("RING_TRANSITION_STACK_WORDS=%u ",flags >> 3);
                        printf("\n");

                        printf("                CONSTANT = 0x%04x\n",seg_offs);
                    }
                }
                else if (segid == 0x00) {
                    printf("UNUSED entry\n");

                    for (ent=0;ent < nument;ent++) {
                        printf("            Ordinal #%d EMPTY\n",ordinal++);
                    }
                }
                else {
                    printf("FIXED segment #%u\n",segid);

                    for (ent=0;ent < nument;ent++) {
                        if ((scan+3) > fence) break;

                        flags = *scan++;
                        seg_offs = *((uint16_t*)scan); scan += 2;

                        printf("            Ordinal #%d ",ordinal++);
                        if (flags & 1) printf("EXPORTED ");
                        if (flags & 2) printf("USES_GLOBAL_SHARED_DATA_SEGMENT ");
                        if (flags & 0xF8) printf("RING_TRANSITION_STACK_WORDS=%u ",flags >> 3);
                        printf("\n");

                        printf("                OFFSET = 0x%04x\n",seg_offs);
                    }
                }

                if (scan == fence) break;
            }

            free(buf);
        }
        else {
            printf("    ! ERROR. Cannot allocate memory to read entry table\n");
        }
    }

    if (ne_nonresname_raw) {
        free(ne_nonresname_raw);
        ne_nonresname_raw = NULL;
    }
    if (ne_nonresname) {
        free(ne_nonresname);
        ne_nonresname = NULL;
    }

    if (ne_resname_raw) {
        free(ne_resname_raw);
        ne_resname_raw = NULL;
    }
    if (ne_resname) {
        free(ne_resname);
        ne_resname = NULL;
    }

    if (ne_segments) {
        free(ne_segments);
        ne_segments = NULL;
    }

    close(src_fd);
    return 0;
}
