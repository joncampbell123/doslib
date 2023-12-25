
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
#include <hw/dos/exenepar.h>

#ifndef O_BINARY
#define O_BINARY (0)
#endif

static char*                    src_file = NULL;
static int                      src_fd = -1;

static unsigned char            opt_pric = 0;

static struct exe_dos_header    exehdr;
static struct exe_dos_layout    exelayout;

static void help(void) {
    fprintf(stderr,"EXENERDM -i <exe file>\n");
    fprintf(stderr,"  -pric      Pre-pend a directory structure to ICON and CURSOR resources.\n");
    fprintf(stderr,"             .ico and .cur files are expected to contain this directory.\n");
}

int main(int argc,char **argv) {
    struct exe_ne_header_resource_table_t ne_resources;
    struct exe_ne_header ne_header;
    uint32_t ne_header_offset;
    uint32_t file_size;
    char *a;
    int i;

    assert(sizeof(ne_header) == 0x40);
    memset(&exehdr,0,sizeof(exehdr));
    exe_ne_header_resource_table_init(&ne_resources);

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
            else if (!strcmp(a,"pric")) {
                opt_pric = 1;
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

    /* resource table */
    if (ne_header.resource_table_offset != 0 && ne_header.resident_name_table_offset > ne_header.resource_table_offset &&
        (unsigned long)lseek(src_fd,ne_header.resource_table_offset + ne_header_offset,SEEK_SET) == (ne_header.resource_table_offset + ne_header_offset)) {
        unsigned int raw_length;
        unsigned char *base;

        /* RESOURCE_TABLE_SIZE = resident_name_table_offset - resource_table_offset         (header does not report size, "number of segments" is worthless) */
        raw_length = (unsigned short)(ne_header.resident_name_table_offset - ne_header.resource_table_offset);
        printf("  * Resource table length: %u\n",raw_length);

        base = exe_ne_header_resource_table_alloc_raw(&ne_resources,raw_length);
        if (base != NULL) {
            if ((unsigned int)read(src_fd,base,raw_length) != raw_length)
                exe_ne_header_resource_table_free_raw(&ne_resources);
        }

        exe_ne_header_resource_table_parse(&ne_resources);
    }

    printf("    Resource table, 1 << %u = %lu byte alignment:\n",
        exe_ne_header_resource_table_get_shift(&ne_resources),
        1UL << (unsigned long)exe_ne_header_resource_table_get_shift(&ne_resources));
    printf("        %u TYPEINFO entries\n",ne_resources.typeinfo_length);
    {
        const struct exe_ne_header_resource_table_nameinfo *ninfo;
        const struct exe_ne_header_resource_table_typeinfo *tinfo;
        const char *rtTypeIDintstr;
        const char *file_ext;
        unsigned long foff;
        unsigned long fcpy;
        unsigned long fcnt;
        char tmp[255+1];
        unsigned int ti;
        unsigned int ni;
        int fd;

        for (ti=0;ti < ne_resources.typeinfo_length;ti++) {
            printf("        Typeinfo entry #%d\n",ti+1);

            tinfo = exe_ne_header_resource_table_get_typeinfo_entry(&ne_resources,ti);
            if (tinfo == NULL) {
                printf("            NULL\n");
                continue;
            }

            /* choose file extension by resource type, or .BIN otherwise.
             * Note that some resources, like RT_ICON and RT_CURSOR, are
             * not stored using the familiar .ico and .cur extensions because
             * the file format is expected to carry a "directory" structure
             * that allows Windows to select one of multiple icons for the
             * display, unless the user has explicitly instructed us to
             * prepend a fake directory to the raw data to make it valid. */
            switch (tinfo->rtTypeID) {
                case exe_ne_header_RT_CURSOR:           file_ext = opt_pric?".cur":".rcu"; break;
                case exe_ne_header_RT_BITMAP:           file_ext = ".bmp"; break;
                case exe_ne_header_RT_ICON:             file_ext = opt_pric?".ico":".ric"; break;
                case exe_ne_header_RT_MENU:             file_ext = ".men"; break;
                case exe_ne_header_RT_DIALOG:           file_ext = ".dlg"; break;
                case exe_ne_header_RT_STRING:           file_ext = ".str"; break;
                case exe_ne_header_RT_FONTDIR:          file_ext = ".fdr"; break;
                case exe_ne_header_RT_FONT:             file_ext = ".fon"; break;
                case exe_ne_header_RT_ACCELERATOR:      file_ext = ".acc"; break;
                case exe_ne_header_RT_RCDATA:           file_ext = ".rcd"; break;
                case exe_ne_header_RT_MESSAGETABLE:     file_ext = ".msg"; break;
                case exe_ne_header_RT_GROUP_CURSOR:     file_ext = ".gcr"; break;
                case exe_ne_header_RT_GROUP_ICON:       file_ext = ".gic"; break;
                case exe_ne_header_RT_NAME_TABLE:       file_ext = ".ntb"; break;
                case exe_ne_header_RT_VERSION:          file_ext = ".ver"; break;
                default:                                file_ext = ".bin"; break;
            };

            /* NTS: if bit 15 of rtTypeID is set (rtTypeID & 0x8000), rtTypeID is an integer identifier.
             *      otherwise, rtTypeID is an offset to the length + string combo containing the
             *      identifier as a string. offset is relative to the resource table.
             *
             *      there are some standard integer identifiers defined by Windows, such as
             *      RT_ICON, RT_CURSOR, RT_DIALOG, etc. that define standard resources. */
            if (exe_ne_header_resource_table_typeinfo_TYPEID_IS_INTEGER(tinfo->rtTypeID)) {
                printf("            rtTypeID:   INTEGER 0x%04x",
                    exe_ne_header_resource_table_typeinfo_TYPEID_AS_INTEGER(tinfo->rtTypeID));
                rtTypeIDintstr = exe_ne_header_resource_table_typeinfo_TYPEID_INTEGER_name_str(tinfo->rtTypeID);
                if (rtTypeIDintstr != NULL) printf(" %s",rtTypeIDintstr);
                printf("\n");
            }
            else {
                exe_ne_header_resource_table_get_string(tmp,sizeof(tmp),&ne_resources,tinfo->rtTypeID);
                printf("            rtTypeID:   STRING OFFSET 0x%04x '%s'",tinfo->rtTypeID,tmp);
                printf("\n");
            }

            printf("            rtResourceCount: %u\n",tinfo->rtResourceCount);
            for (ni=0;ni < tinfo->rtResourceCount;ni++) {
                ninfo = exe_ne_header_resource_table_get_typeinfo_nameinfo_entry(tinfo,ni);
                if (ninfo == NULL) {
                    printf("                NULL\n");
                    continue;
                }

                printf("            Entry #%d:\n",ni+1);
                printf("                rnOffset:           %u sectors << %u = %lu bytes\n",
                    ninfo->rnOffset,
                    exe_ne_header_resource_table_get_shift(&ne_resources),
                    (unsigned long)ninfo->rnOffset << (unsigned long)exe_ne_header_resource_table_get_shift(&ne_resources));
                printf("                rnLength:           %u sectors << %u = %lu bytes\n",
                    ninfo->rnLength,
                    exe_ne_header_resource_table_get_shift(&ne_resources),
                    (unsigned long)ninfo->rnLength << (unsigned long)exe_ne_header_resource_table_get_shift(&ne_resources));

                printf("                rnFlags:            0x%04x",
                    ninfo->rnFlags);
                if (ninfo->rnFlags & 0x10)
                    printf(" MOVABLE");
                else
                    printf(" FIXED");
                if (ninfo->rnFlags & 0x20)
                    printf(" SHARABLE");
                else
                    printf(" NONSHAREABLE");
                if (ninfo->rnFlags & 0x40)
                    printf(" PRELOAD");
                else
                    printf(" LOADONCALL");
                printf("\n");

                /* NTS: if bit 15 of rnID is set (rnID & 0x8000), rnID is an integer identifier.
                 *      otherwise, rnID is an offset to the length + string combo containing the
                 *      identifier as a string. offset is relative to the resource table. */
                if (exe_ne_header_resource_table_typeinfo_RNID_IS_INTEGER(ninfo->rnID)) {
                    printf("                rnID:               INTEGER 0x%04x\n",
                        exe_ne_header_resource_table_typeinfo_RNID_AS_INTEGER(ninfo->rnID));
                }
                else {
                    exe_ne_header_resource_table_get_string(tmp,sizeof(tmp),&ne_resources,ninfo->rnID);
                    printf("                rnID:               STRING OFFSET 0x%04x '%s'\n",
                        ninfo->rnID,tmp);
                }

                /* then choose file to write */
                sprintf(tmp,"%04X%04X%s",
                    (unsigned int)tinfo->rtTypeID,
                    (unsigned int)ninfo->rnID,
                    file_ext);

                printf("                Writing to: %s\n",tmp);

                fcpy = (unsigned long)ninfo->rnLength << (unsigned long)exe_ne_header_resource_table_get_shift(&ne_resources);
                foff = (unsigned long)ninfo->rnOffset << (unsigned long)exe_ne_header_resource_table_get_shift(&ne_resources);
                if ((unsigned long)lseek(src_fd,foff,SEEK_SET) != foff) {
                    printf("                ! Cannot seek to offset\n");
                    continue;
                }

                fd = open(tmp,O_CREAT|O_TRUNC|O_WRONLY|O_BINARY,0644);
                if (fd < 0) {
                    fprintf(stderr,"Unable to write %s, %s\n",tmp,strerror(errno));
                    return 1;
                }

                fcnt = 0;
                while (fcpy > 0UL) {
                    int docpy = (fcpy > sizeof(tmp) ? sizeof(tmp) : fcpy);
                    int rd = read(src_fd,tmp,docpy);

                    if (rd > 0) {
                        /* WAIT: If this is an RT_ICON or RT_CURSOR, prepend a header to make it a valid file */
                        if (opt_pric && fcnt == 0) {
                            if (tinfo->rtTypeID == exe_ne_header_RT_BITMAP) {
                                if (exe_ne_header_is_WINOLDBITMAP((const unsigned char*)tmp,docpy)) {
                                    /* then it is necessary to convert, not just copy, because
                                     * the alignment rules are different:
                                     *
                                     * Windows 1.x/2.x BITMAP:          Requires WORD alignment, DIBs are top-down
                                     * Windows 3.x BITMAPINFOHEADER:    Requires DWORD alignment, DIBs are bottom-up */
                                    struct exe_ne_header_BITMAPINFOHEADER bmphdr3x;
                                    const struct exe_ne_header_RTBITMAP *bmphdr2x =
                                        (const struct exe_ne_header_RTBITMAP *)tmp;
                                    unsigned int pal_colors = 1 << bmphdr2x->bmBitsPixel;
                                    unsigned int align3x = (((bmphdr2x->bmBitsPixel * bmphdr2x->bmWidth) + 31) & (~31)) >> 3; // BITMAPINFOHEADER alignment
                                    struct exe_ne_header_RGBQUAD rgb;
                                    unsigned long bboff = 14;
                                    unsigned char hd[14];

                                    if (bmphdr2x->bmPlanes == 1 && (bmphdr2x->bmBitsPixel == 1 || bmphdr2x->bmBitsPixel == 4)) {
                                        printf("* Converting BITMAP to BITMAPINFOHEADER\n");

                                        /* reseek file pointer, bitmap bits immediately follow bmphdr2x */
                                        lseek(src_fd,foff+sizeof(*bmphdr2x),SEEK_SET);

                                        /* generate BMP FILE header */
                                        bboff += sizeof(bmphdr3x);
                                        bboff += pal_colors * 4;

                                        memcpy(hd+0,"BM",2);
                                        *((uint32_t*)(hd+2)) = (align3x * abs(bmphdr2x->bmHeight)) + bboff;
                                        *((uint16_t*)(hd+6)) = 0;
                                        *((uint16_t*)(hd+8)) = 0;
                                        *((uint32_t*)(hd+10)) = bboff;
                                        write(fd,hd,sizeof(hd));

                                        /* BITMAPINFOHEADER */
                                        bmphdr3x.biSize = sizeof(bmphdr3x);
                                        bmphdr3x.biWidth = bmphdr2x->bmWidth;
                                        bmphdr3x.biHeight = bmphdr2x->bmHeight;
                                        bmphdr3x.biPlanes = bmphdr2x->bmPlanes;
                                        bmphdr3x.biBitCount = bmphdr2x->bmBitsPixel;
                                        bmphdr3x.biCompression = 0;
                                        bmphdr3x.biSizeImage = align3x * abs(bmphdr2x->bmHeight);
                                        bmphdr3x.biXPelsPerMeter = 0;
                                        bmphdr3x.biYPelsPerMeter = 0;
                                        bmphdr3x.biClrUsed = 0;
                                        bmphdr3x.biClrImportant = 0;
                                        write(fd,&bmphdr3x,sizeof(bmphdr3x));

                                        /* color palette */
                                        if (bmphdr2x->bmBitsPixel == 1) {
                                            {
                                                rgb.rgbRed = 0x00; rgb.rgbGreen = 0x00; rgb.rgbBlue = 0x00; rgb.rgbReserved = 0x00;
                                                write(fd,&rgb,sizeof(rgb));
                                            }
                                            {
                                                rgb.rgbRed = 0xFF; rgb.rgbGreen = 0xFF; rgb.rgbBlue = 0xFF; rgb.rgbReserved = 0x00;
                                                write(fd,&rgb,sizeof(rgb));
                                            }
                                        }
                                        else {
                                            /* I'll add 4bpp generation when I see it in the wild */
                                            printf("! Cannot guess palette\n");
                                        }

                                        /* copy scanlines */
                                        {
                                            size_t rd = bmphdr2x->bmWidthBytes;
                                            size_t wd = align3x;
                                            size_t bufl = (rd > wd) ? rd : wd;
                                            unsigned char *buf = malloc(bufl);
                                            unsigned int y;

                                            if (buf) {
                                                memset(buf,0,bufl);
                                                for (y=0;y < (unsigned int)abs(bmphdr2x->bmHeight);y++) {
                                                    /* we have to flip the bitmap upside-down as we convert */
                                                    lseek(src_fd,foff+sizeof(*bmphdr2x)+((abs(bmphdr2x->bmHeight) - 1 - y) * rd),SEEK_SET);
                                                    read(src_fd,buf,rd);
                                                    write(fd,buf,wd);
                                                }
                                                free(buf);
                                            }
                                        }

                                        /* DONE */
                                        break;
                                    }
                                    else {
                                        printf("! Cannot convert BITMAP to BITMAPINFOHEADER, unknown format\n");
                                    }
                                }
                                else {
                                    /* need to add a BITMAPFILEHEADER to make it valid */
                                    const struct exe_ne_header_BITMAPINFOHEADER *bmphdr =
                                        (const struct exe_ne_header_BITMAPINFOHEADER *)tmp;
                                    unsigned int pal_colors =
                                        exe_ne_header_BITMAPINFOHEADER_get_palette_count(bmphdr);
                                    unsigned long bboff = 14;
                                    unsigned char hd[14];

                                    bboff += bmphdr->biSize;
                                    bboff += pal_colors * 4;

                                    memcpy(hd+0,"BM",2);
                                    *((uint32_t*)(hd+2)) = fcpy + 14;
                                    *((uint16_t*)(hd+6)) = 0;
                                    *((uint16_t*)(hd+8)) = 0;
                                    *((uint32_t*)(hd+10)) = bboff;
                                    write(fd,hd,sizeof(hd));
                                }
                            }
                            else if (tinfo->rtTypeID == exe_ne_header_RT_ICON) {
                                if (exe_ne_header_is_WINOLDICON((const unsigned char*)tmp,docpy)) {
                                    /* then it is necessary to convert, not just copy, because
                                     * the alignment rules are different:
                                     *
                                     * Windows 1.x/2.x BITMAP:          Requires WORD alignment, DIBs are top-down
                                     * Windows 3.x BITMAPINFOHEADER:    Requires DWORD alignment, DIBs are bottom-up */
                                    struct exe_ne_header_resource_ICONDIR pre;
                                    struct exe_ne_header_resource_ICONDIRENTRY dent;
                                    struct exe_ne_header_BITMAPINFOHEADER bmphdr3x;
                                    struct exe_ne_header_RTICONBITMAP *bmphdr2x =
                                        (struct exe_ne_header_RTICONBITMAP *)tmp;
                                    struct exe_ne_header_RGBQUAD rgb;
                                    unsigned int align3xmono;
                                    unsigned int align3x;

                                    /* Windows 1.x/2.x icons for whatever reason have bits/pixel == 0 and planes == 0,
                                     * which apparently means monochromatic anyway */
                                    if (bmphdr2x->bmPlanes == 0)
                                        bmphdr2x->bmPlanes = 1;
                                    if (bmphdr2x->bmBitsPixel == 0)
                                        bmphdr2x->bmBitsPixel = 1;

                                    align3x = (((bmphdr2x->bmBitsPixel * bmphdr2x->bmWidth) + 31) & (~31)) >> 3; // BITMAPINFOHEADER alignment
                                    align3xmono = ((bmphdr2x->bmWidth + 31) & (~31)) >> 3; // BITMAPINFOHEADER alignment
                                    if (bmphdr2x->bmPlanes == 1 && bmphdr2x->bmBitsPixel == 1) {
                                        printf("* Converting BITMAP to BITMAPINFOHEADER\n");

                                        /* reseek file pointer, bitmap bits immediately follow bmphdr2x */
                                        lseek(src_fd,foff+sizeof(*bmphdr2x),SEEK_SET);

                                        pre.idReserved = 0;
                                        pre.idType = 1;
                                        pre.idCount = 1;
                                        write(fd,&pre,sizeof(pre));

                                        dent.bWidth = bmphdr2x->bmWidth;
                                        dent.bHeight = bmphdr2x->bmHeight;
                                        dent.bColorCount = 1 << bmphdr2x->bmBitsPixel;
                                        dent.bReserved = 0;
                                        dent.wPlanes = bmphdr2x->bmPlanes;
                                        dent.wBitCount = bmphdr2x->bmBitsPixel;
                                        dent.dwBytesInRes = sizeof(bmphdr3x) + (4UL << (unsigned long)bmphdr2x->bmBitsPixel) +
                                            (align3x * abs(bmphdr2x->bmHeight)) + (align3xmono * abs(bmphdr2x->bmHeight)); // icon + mask
                                        dent.dwImageOffset = sizeof(pre) + sizeof(dent);
                                        write(fd,&dent,sizeof(dent));

                                        /* BITMAPINFOHEADER */
                                        bmphdr3x.biSize = sizeof(bmphdr3x);
                                        bmphdr3x.biWidth = bmphdr2x->bmWidth;
                                        bmphdr3x.biHeight = bmphdr2x->bmHeight * 2; // icon + mask
                                        bmphdr3x.biPlanes = bmphdr2x->bmPlanes;
                                        bmphdr3x.biBitCount = bmphdr2x->bmBitsPixel;
                                        bmphdr3x.biCompression = 0;
                                        bmphdr3x.biSizeImage = (align3x * abs(bmphdr2x->bmHeight)) +
                                            (align3xmono * abs(bmphdr2x->bmHeight)); // icon + mask
                                        bmphdr3x.biXPelsPerMeter = 0;
                                        bmphdr3x.biYPelsPerMeter = 0;
                                        bmphdr3x.biClrUsed = 0;
                                        bmphdr3x.biClrImportant = 0;
                                        write(fd,&bmphdr3x,sizeof(bmphdr3x));

                                        /* color palette */
                                        if (bmphdr2x->bmBitsPixel == 1) {
                                            {
                                                rgb.rgbRed = 0x00; rgb.rgbGreen = 0x00; rgb.rgbBlue = 0x00; rgb.rgbReserved = 0x00;
                                                write(fd,&rgb,sizeof(rgb));
                                            }
                                            {
                                                rgb.rgbRed = 0xFF; rgb.rgbGreen = 0xFF; rgb.rgbBlue = 0xFF; rgb.rgbReserved = 0x00;
                                                write(fd,&rgb,sizeof(rgb));
                                            }
                                        }
                                        else {
                                            /* I'll add 4bpp generation when I see it in the wild */
                                            printf("! Cannot guess palette\n");
                                        }

                                        /* NTS: Where Windows 3.x stores the image first, followed by the mask,
                                         *      Windows 1.x/2.x store the mask first, then the image. Like bitmaps
                                         *      in Windows 1.x/2.x the DIB is top-down, we convert here to bottom-up. */
                                        {
                                            size_t ird = bmphdr2x->bmWidthBytes;
                                            size_t mrd = ((bmphdr2x->bmWidth + 15) & (~15)) >> 3; /* WORD align requirement */
                                            size_t iwd = align3x;
                                            size_t mwd = align3xmono;
                                            size_t ibufl = (ird > iwd) ? ird : iwd;
                                            size_t mbufl = (mrd > mwd) ? mrd : mwd;
                                            unsigned char *img = malloc(ibufl * bmphdr2x->bmHeight);
                                            unsigned char *msk = malloc(mbufl * bmphdr2x->bmHeight);
                                            unsigned int y;

                                            /* load */
                                            for (y=0;y < (unsigned int)abs(bmphdr2x->bmHeight);y++)
                                                read(src_fd,msk + ((abs(bmphdr2x->bmHeight) - 1 - y) * mbufl),mrd);
                                            for (y=0;y < (unsigned int)abs(bmphdr2x->bmHeight);y++)
                                                read(src_fd,img + ((abs(bmphdr2x->bmHeight) - 1 - y) * ibufl),ird);

                                            /* write */
                                            for (y=0;y < (unsigned int)abs(bmphdr2x->bmHeight);y++)
                                                write(fd,img + (y * ibufl),iwd);
                                            for (y=0;y < (unsigned int)abs(bmphdr2x->bmHeight);y++)
                                                write(fd,msk + (y * mbufl),mwd);

                                            free(img);
                                            free(msk);
                                        }

                                        /* DONE */
                                        break;
                                    }
                                    else {
                                        printf("! Cannot convert BITMAP to BITMAPINFOHEADER, unknown format\n");
                                    }
                                }
                                else {
                                    struct exe_ne_header_resource_ICONDIR pre;
                                    struct exe_ne_header_resource_ICONDIRENTRY dent;
                                    struct exe_ne_header_BITMAPINFOHEADER *bmp =
                                        (struct exe_ne_header_BITMAPINFOHEADER*)tmp;

                                    pre.idReserved = 0;
                                    pre.idType = 1;
                                    pre.idCount = 1;
                                    write(fd,&pre,sizeof(pre));

                                    dent.bWidth = bmp->biWidth;
                                    dent.bHeight = bmp->biHeight >> 1;      /* remember icons carry image + mask and dwHeight is double the actual height */
                                    dent.bColorCount = 1 << bmp->biBitCount;
                                    dent.bReserved = 0;
                                    dent.wPlanes = bmp->biPlanes;
                                    dent.wBitCount = bmp->biBitCount;
                                    dent.dwBytesInRes = fcpy;
                                    dent.dwImageOffset = sizeof(pre) + sizeof(dent);
                                    write(fd,&dent,sizeof(dent));
                                }
                            }
                            else if (tinfo->rtTypeID == exe_ne_header_RT_CURSOR) {
                                /* raw resource data for a cursor when in an NE executable:
                                 *
                                 * WORD                 hotspot_x
                                 * WORD                 hotspot_y
                                 * BITMAPINFOHEADER     cursor bitmapinfo
                                 * RGBQUAD              cursor palette
                                 * BYTE[]               cursor bitmap
                                 *
                                 */
                                /* There's no way to auto detect from values because the first two are the cursor hotspot.
                                 * A cursor hotspot of 3,3 could be mistaken as an old Windows 2.0 icon. We have to use
                                 * the NE header's version number */
                                if (ne_header.minimum_windows_version < 0x300) {
                                    /* then it is necessary to convert, not just copy, because
                                     * the alignment rules are different:
                                     *
                                     * Windows 1.x/2.x BITMAP:          Requires WORD alignment, DIBs are top-down
                                     * Windows 3.x BITMAPINFOHEADER:    Requires DWORD alignment, DIBs are bottom-up */
                                    struct exe_ne_header_resource_CURSORDIR pre;
                                    struct exe_ne_header_resource_CURSORDIRENTRY dent;
                                    struct exe_ne_header_BITMAPINFOHEADER bmphdr3x;
                                    struct exe_ne_header_RTCURSORBITMAP *bmphdr2x =
                                        (struct exe_ne_header_RTCURSORBITMAP *)tmp;
                                    struct exe_ne_header_RGBQUAD rgb;
                                    unsigned int align3xmono;
                                    unsigned int align3x;

                                    /* Windows 1.x/2.x icons for whatever reason have bits/pixel == 0 and planes == 0,
                                     * which apparently means monochromatic anyway */
                                    if (bmphdr2x->bmPlanes == 0)
                                        bmphdr2x->bmPlanes = 1;
                                    if (bmphdr2x->bmBitsPixel == 0)
                                        bmphdr2x->bmBitsPixel = 1;

                                    align3x = (((bmphdr2x->bmBitsPixel * bmphdr2x->bmWidth) + 31) & (~31)) >> 3; // BITMAPINFOHEADER alignment
                                    align3xmono = ((bmphdr2x->bmWidth + 31) & (~31)) >> 3; // BITMAPINFOHEADER alignment
                                    if (bmphdr2x->bmPlanes == 1 && bmphdr2x->bmBitsPixel == 1) {
                                        printf("* Converting BITMAP to BITMAPINFOHEADER\n");

                                        /* reseek file pointer, bitmap bits immediately follow bmphdr2x */
                                        lseek(src_fd,foff+sizeof(*bmphdr2x),SEEK_SET);

                                        pre.cdReserved = 0;
                                        pre.cdType = 1;
                                        pre.cdCount = 1;
                                        write(fd,&pre,sizeof(pre));

                                        dent.bWidth = bmphdr2x->bmWidth;
                                        dent.bHeight = bmphdr2x->bmHeight;
                                        dent.bColorCount = 1 << bmphdr2x->bmBitsPixel;
                                        dent.bReserved = 0;
                                        dent.wXHotspot = 0;
                                        dent.wYHotspot = 0;
                                        dent.bColorCount = 0;
                                        dent.dwBytesInRes = sizeof(bmphdr3x) + (4UL << (unsigned long)bmphdr2x->bmBitsPixel) +
                                            (align3x * abs(bmphdr2x->bmHeight)) + (align3xmono * abs(bmphdr2x->bmHeight)); // icon + mask
                                        dent.dwImageOffset = sizeof(pre) + sizeof(dent);
                                        write(fd,&dent,sizeof(dent));

                                        /* BITMAPINFOHEADER */
                                        bmphdr3x.biSize = sizeof(bmphdr3x);
                                        bmphdr3x.biWidth = bmphdr2x->bmWidth;
                                        bmphdr3x.biHeight = bmphdr2x->bmHeight * 2; // icon + mask
                                        bmphdr3x.biPlanes = bmphdr2x->bmPlanes;
                                        bmphdr3x.biBitCount = bmphdr2x->bmBitsPixel;
                                        bmphdr3x.biCompression = 0;
                                        bmphdr3x.biSizeImage = (align3x * abs(bmphdr2x->bmHeight)) +
                                            (align3xmono * abs(bmphdr2x->bmHeight)); // icon + mask
                                        bmphdr3x.biXPelsPerMeter = 0;
                                        bmphdr3x.biYPelsPerMeter = 0;
                                        bmphdr3x.biClrUsed = 0;
                                        bmphdr3x.biClrImportant = 0;
                                        write(fd,&bmphdr3x,sizeof(bmphdr3x));

                                        /* color palette */
                                        if (bmphdr2x->bmBitsPixel == 1) {
                                            {
                                                rgb.rgbRed = 0x00; rgb.rgbGreen = 0x00; rgb.rgbBlue = 0x00; rgb.rgbReserved = 0x00;
                                                write(fd,&rgb,sizeof(rgb));
                                            }
                                            {
                                                rgb.rgbRed = 0xFF; rgb.rgbGreen = 0xFF; rgb.rgbBlue = 0xFF; rgb.rgbReserved = 0x00;
                                                write(fd,&rgb,sizeof(rgb));
                                            }
                                        }
                                        else {
                                            /* I'll add 4bpp generation when I see it in the wild */
                                            printf("! Cannot guess palette\n");
                                        }

                                        /* NTS: Where Windows 3.x stores the image first, followed by the mask,
                                         *      Windows 1.x/2.x store the mask first, then the image. Like bitmaps
                                         *      in Windows 1.x/2.x the DIB is top-down, we convert here to bottom-up. */
                                        {
                                            size_t ird = bmphdr2x->bmWidthBytes;
                                            size_t mrd = ((bmphdr2x->bmWidth + 15) & (~15)) >> 3; /* WORD align requirement */
                                            size_t iwd = align3x;
                                            size_t mwd = align3xmono;
                                            size_t ibufl = (ird > iwd) ? ird : iwd;
                                            size_t mbufl = (mrd > mwd) ? mrd : mwd;
                                            unsigned char *img = malloc(ibufl * bmphdr2x->bmHeight);
                                            unsigned char *msk = malloc(mbufl * bmphdr2x->bmHeight);
                                            unsigned int y;

                                            /* load */
                                            for (y=0;y < (unsigned int)abs(bmphdr2x->bmHeight);y++)
                                                read(src_fd,msk + ((abs(bmphdr2x->bmHeight) - 1 - y) * mbufl),mrd);
                                            for (y=0;y < (unsigned int)abs(bmphdr2x->bmHeight);y++)
                                                read(src_fd,img + ((abs(bmphdr2x->bmHeight) - 1 - y) * ibufl),ird);

                                            /* write */
                                            for (y=0;y < (unsigned int)abs(bmphdr2x->bmHeight);y++)
                                                write(fd,img + (y * ibufl),iwd);
                                            for (y=0;y < (unsigned int)abs(bmphdr2x->bmHeight);y++)
                                                write(fd,msk + (y * mbufl),mwd);

                                            free(img);
                                            free(msk);
                                        }

                                        /* DONE */
                                        break;
                                    }
                                    else {
                                        printf("! Cannot convert BITMAP to BITMAPINFOHEADER, unknown format\n");
                                    }
                                }
                                else {
                                    struct exe_ne_header_resource_CURSORDIR pre;
                                    struct exe_ne_header_resource_CURSORDIRENTRY dent;
                                    struct exe_ne_header_BITMAPINFOHEADER *bmp =
                                        (struct exe_ne_header_BITMAPINFOHEADER*)(tmp + 4);

                                    pre.cdReserved = 0;
                                    pre.cdType = 2;
                                    pre.cdCount = 1;
                                    write(fd,&pre,sizeof(pre));

                                    dent.bWidth = bmp->biWidth;
                                    dent.bHeight = bmp->biHeight >> 1;      /* remember cursors carry image + mask and dwHeight is double the actual height */
                                    dent.bColorCount = 1 << bmp->biBitCount;
                                    dent.bReserved = 0;
                                    dent.wXHotspot = *((uint16_t*)(tmp + 0));
                                    dent.wYHotspot = *((uint16_t*)(tmp + 2));
                                    dent.dwBytesInRes = fcpy - 4;
                                    dent.dwImageOffset = sizeof(pre) + sizeof(dent);
                                    write(fd,&dent,sizeof(dent));

                                    if (rd >= 4) {
                                        rd -= 4;
                                        fcpy -= 4;
                                        docpy -= 4;
                                        memmove(tmp,tmp+4,rd);
                                    }
                                }
                            }
                        }

                        write(fd,tmp,rd);
                        fcpy -= rd;
                        fcnt += rd;
                    }

                    if (rd < docpy) {
                        fprintf(stderr,"Early EOF on resource\n");
                        break;
                    }
                }

                close(fd);
            }
        }
    }

    exe_ne_header_resource_table_free(&ne_resources);
    close(src_fd);
    return 0;
}
