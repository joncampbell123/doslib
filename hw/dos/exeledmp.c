
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>

#include <hw/dos/exehdr.h>

#ifndef O_BINARY
#define O_BINARY (0)
#endif

static char*                    src_file = NULL;
static int                      src_fd = -1;

static struct exe_dos_header    exehdr;
static struct exe_dos_layout    exelayout;

static void help(void) {
    fprintf(stderr,"EXELEDMP -i <exe file>\n");
}

///////////////

#pragma pack(push,1)
struct exe_le_header {
    uint16_t        signature;                      // +0x00 0x454C 'LE'
    uint8_t         byte_order;                     // +0x02 byte order (0 = little endian  nonzero = big endian)
    uint8_t         word_order;                     // +0x03 word order (0 = little endian  nonzero = big endian)
    uint32_t        executable_format_level;        // +0x04 incremented each incompatible change to the EXE format
    uint16_t        cpu_type;                       // +0x08 cpu type (1 = 286, 2 = 386, 3 = 486, 4 = 586, and more)
    uint16_t        target_operating_system;        // +0x0A target os (1 = OS/2, 2 = windows, 3 = DOS 4.x, 4 = Windows 386)
    uint32_t        module_version;                 // +0x0C version of the EXE, for differentiating revisions of dynamic linked modules
    uint32_t        module_type_flags;              // +0x10 module flags (see enum)
    uint32_t        number_of_memory_pages;         // +0x14 number of memory pages
    uint32_t        initial_object_cs_number;       // +0x18
    uint32_t        initial_eip;                    // +0x1C
    uint32_t        initial_object_ss_number;       // +0x20
    uint32_t        initial_esp;                    // +0x24
    uint32_t        memory_page_size;               // +0x28
    uint32_t        bytes_on_last_page;             // +0x2C
    uint32_t        fixup_section_size;             // +0x30
    uint32_t        fixup_section_checksum;         // +0x34
    uint32_t        loader_section_size;            // +0x38
    uint32_t        loader_section_checksum;        // +0x3C
    uint32_t        offset_of_object_table;         // +0x40
    uint32_t        object_table_entries;           // +0x44
    uint32_t        object_page_map_offset;         // +0x48
    uint32_t        object_iterate_data_map_offset; // +0x4C
    uint32_t        resource_table_offset;          // +0x50
    uint32_t        resource_table_entries;         // +0x54
    uint32_t        resident_names_table_offset;    // +0x58
    uint32_t        entry_table_offset;             // +0x5C
    uint32_t        module_directives_table_offset; // +0x60
    uint32_t        module_directives_entries;      // +0x64
    uint32_t        fixup_page_table_offset;        // +0x68
    uint32_t        fixup_record_table_offset;      // +0x6C
    uint32_t        imported_modules_name_table_offset;// +0x70
    uint32_t        imported_modules_count;         // +0x74
    uint32_t        imported_procedure_name_table_offset;// +0x78
    uint32_t        per_page_checksum_table_offset; // +0x7C
    uint32_t        data_pages_offset;              // +0x80 from top of file
    uint32_t        preload_page_count;             // +0x84
    uint32_t        nonresident_names_table_offset; // +0x88 from top of file
    uint32_t        nonresident_names_table_length; // +0x8C
    uint32_t        nonresident_names_table_checksum;// +0x90
    uint32_t        automatic_data_object;          // +0x94
    uint32_t        debug_information_offset;       // +0x98
    uint32_t        debug_information_length;       // +0x9C
    uint32_t        preload_instances_pages_number; // +0xA0
    uint32_t        demand_instances_pages_number;  // +0xA4
    uint32_t        extra_heap_allocation;          // +0xA8
};                                                  // =0xAC
#pragma pack(pop)
#define EXE_HEADER_LE_HEADER_SIZE           (0xAC)
#define EXE_LE_SIGNATURE                    (0x454C)

#define LE_HEADER_MODULE_TYPE_FLAGS_PER_PROCESS_DLL_INIT            (1U << 2U)
#define LE_HEADER_MODULE_TYPE_FLAGS_NO_INTERNAL_FIXUP               (1U << 4U)
#define LE_HEADER_MODULE_TYPE_FLAGS_NO_EXTERNAL_FIXUP               (1U << 5U)

#define LE_HEADER_MODULE_TYPE_FLAGS_PM_WINDOWING_MASK               (3U << 8U)
#define LE_HEADER_MODULE_TYPE_FLAGS_PM_WINDOWING_UNKNOWN            (0U << 8U)
#define LE_HEADER_MODULE_TYPE_FLAGS_PM_WINDOWING_INCOMPATIBLE       (1U << 8U)
#define LE_HEADER_MODULE_TYPE_FLAGS_PM_WINDOWING_COMPATIBLE         (2U << 8U)
#define LE_HEADER_MODULE_TYPE_FLAGS_PM_WINDOWING_USES_API           (3U << 8U)

#define LE_HEADER_MODULE_TYPE_FLAGS_MODULE_NOT_LOADABLE             (1U << 13U)
#define LE_HEADER_MODULE_TYPE_FLAGS_IS_DLL                          (1U << 15U)

const char *le_cpu_type_to_str(const uint8_t b) {
    switch (b) {
        case 0x01:  return "Intel 80286";
        case 0x02:  return "Intel 80386";
        case 0x03:  return "Intel 80486";
        case 0x04:  return "Intel 80586";
        case 0x20:  return "Intel i860 (N10)";
        case 0x21:  return "Intel N11";
        case 0x40:  return "MIPS Mark I (R2000/R3000)";
        case 0x41:  return "MIPS Mark II (R6000)";
        case 0x42:  return "MIPS Mark III (R4000)";
        default:    break;
    }

    return "";
}

const char *le_target_operating_system_to_str(const uint8_t b) {
    switch (b) {
        case 0x01:  return "OS/2";
        case 0x02:  return "Windows";
        case 0x03:  return "DOS 4.x";
        case 0x04:  return "Windows 386";
        default:    break;
    }

    return "";
}

///////////////

int main(int argc,char **argv) {
    struct exe_le_header le_header;
    uint32_t le_header_offset;
    uint32_t file_size;
    char *a;
    int i;

    assert(sizeof(le_header) == EXE_HEADER_LE_HEADER_SIZE);
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
        read(src_fd,&le_header_offset,4) != 4) {
        fprintf(stderr,"Cannot read extension\n");
        return 1;
    }
    printf("    EXE extension (if exists) at: %lu\n",(unsigned long)le_header_offset);
    if ((le_header_offset+EXE_HEADER_LE_HEADER_SIZE) >= file_size) {
        printf("! NE header not present (offset out of range)\n");
        return 0;
    }

    /* go read the extended header */
    if (lseek(src_fd,le_header_offset,SEEK_SET) != le_header_offset ||
        read(src_fd,&le_header,sizeof(le_header)) != sizeof(le_header)) {
        fprintf(stderr,"Cannot read LE header\n");
        return 1;
    }
    if (le_header.signature != EXE_LE_SIGNATURE) { // TODO 'LX'
        fprintf(stderr,"Not an LE executable\n");
        return 1;
    }

    printf("* LE header at %lu\n",(unsigned long)le_header_offset);
    printf("    Byte order:                     0x%02x (%s-endian)\n",
            le_header.byte_order,
            le_header.byte_order ? "big" : "little");
    printf("    Word order:                     0x%02x (%s-endian)\n",
            le_header.word_order,
            le_header.byte_order ? "big" : "little");
    printf("    Executable format level:        0x%08lx\n",
            (unsigned long)le_header.executable_format_level);
    printf("    CPU type:                       0x%02x (%s)\n",
            le_header.cpu_type,
            le_cpu_type_to_str(le_header.cpu_type));
    printf("    Target operating system:        0x%02x (%s)\n",
            le_header.target_operating_system,
            le_target_operating_system_to_str(le_header.target_operating_system));
    printf("    Module version:                 0x%08lx\n",
            (unsigned long)le_header.module_version);
    printf("    Module type flags:              0x%04lx\n",
            (unsigned long)le_header.module_type_flags);
    if (le_header.module_type_flags & LE_HEADER_MODULE_TYPE_FLAGS_PER_PROCESS_DLL_INIT)
        printf("        LE_HEADER_MODULE_TYPE_FLAGS_PER_PROCESS_DLL_INIT\n");
    if (le_header.module_type_flags & LE_HEADER_MODULE_TYPE_FLAGS_NO_INTERNAL_FIXUP)
        printf("        LE_HEADER_MODULE_TYPE_FLAGS_NO_INTERNAL_FIXUP\n");
    if (le_header.module_type_flags & LE_HEADER_MODULE_TYPE_FLAGS_NO_EXTERNAL_FIXUP)
        printf("        LE_HEADER_MODULE_TYPE_FLAGS_NO_EXTERNAL_FIXUP\n");
    if (le_header.module_type_flags & LE_HEADER_MODULE_TYPE_FLAGS_MODULE_NOT_LOADABLE)
        printf("        LE_HEADER_MODULE_TYPE_FLAGS_MODULE_NOT_LOADABLE\n");
    if (le_header.module_type_flags & LE_HEADER_MODULE_TYPE_FLAGS_IS_DLL)
        printf("        LE_HEADER_MODULE_TYPE_FLAGS_IS_DLL\n");
    switch (le_header.module_type_flags & LE_HEADER_MODULE_TYPE_FLAGS_PM_WINDOWING_MASK) {
        case LE_HEADER_MODULE_TYPE_FLAGS_PM_WINDOWING_UNKNOWN:
            printf("        LE_HEADER_MODULE_TYPE_FLAGS_PM_WINDOWING_UNKNOWN\n");
            break;
        case LE_HEADER_MODULE_TYPE_FLAGS_PM_WINDOWING_INCOMPATIBLE:
            printf("        LE_HEADER_MODULE_TYPE_FLAGS_PM_WINDOWING_INCOMPATIBLE\n");
            break;
        case LE_HEADER_MODULE_TYPE_FLAGS_PM_WINDOWING_COMPATIBLE:
            printf("        LE_HEADER_MODULE_TYPE_FLAGS_PM_WINDOWING_COMPATIBLE\n");
            break;
        case LE_HEADER_MODULE_TYPE_FLAGS_PM_WINDOWING_USES_API:
            printf("        LE_HEADER_MODULE_TYPE_FLAGS_PM_WINDOWING_USES_API\n");
            break;
    };
    // ^ NTS: I would like to point out how funny it is that so far, every 32-bit DOS program I've
    //        compiled with Open Watcom has a valid LE header that indicates an OS/2 executable that
    //        is "compatible with the PM windowing system". I take this to mean that perhaps this
    //        structure was chosen for 32-bit DOS because OS/2 would run the program in a window
    //        anyway with an API available that mirrors the DOS API?

#if 0
struct exe_le_header {
    uint32_t        number_of_memory_pages;         // +0x14 number of memory pages
    uint32_t        initial_object_cs_number;       // +0x18
    uint32_t        initial_eip;                    // +0x1C
    uint32_t        initial_object_ss_number;       // +0x20
    uint32_t        initial_esp;                    // +0x24
    uint32_t        memory_page_size;               // +0x28
    uint32_t        bytes_on_last_page;             // +0x2C
    uint32_t        fixup_section_size;             // +0x30
    uint32_t        fixup_section_checksum;         // +0x34
    uint32_t        loader_section_size;            // +0x38
    uint32_t        loader_section_checksum;        // +0x3C
    uint32_t        offset_of_object_table;         // +0x40
    uint32_t        object_table_entries;           // +0x44
    uint32_t        object_page_map_offset;         // +0x48
    uint32_t        object_iterate_data_map_offset; // +0x4C
    uint32_t        resource_table_offset;          // +0x50
    uint32_t        resource_table_entries;         // +0x54
    uint32_t        resident_names_table_offset;    // +0x58
    uint32_t        entry_table_offset;             // +0x5C
    uint32_t        module_directives_table_offset; // +0x60
    uint32_t        module_directives_entries;      // +0x64
    uint32_t        fixup_page_table_offset;        // +0x68
    uint32_t        fixup_record_table_offset;      // +0x6C
    uint32_t        imported_modules_name_table_offset;// +0x70
    uint32_t        imported_modules_count;         // +0x74
    uint32_t        imported_procedure_name_table_offset;// +0x78
    uint32_t        per_page_checksum_table_offset; // +0x7C
    uint32_t        data_pages_offset;              // +0x80 from top of file
    uint32_t        preload_page_count;             // +0x84
    uint32_t        nonresident_names_table_offset; // +0x88 from top of file
    uint32_t        nonresident_names_table_length; // +0x8C
    uint32_t        nonresident_names_table_checksum;// +0x90
    uint32_t        automatic_data_object;          // +0x94
    uint32_t        debug_information_offset;       // +0x98
    uint32_t        debug_information_length;       // +0x9C
    uint32_t        preload_instances_pages_number; // +0xA0
    uint32_t        demand_instances_pages_number;  // +0xA4
    uint32_t        extra_heap_allocation;          // +0xA8
};
#endif

    close(src_fd);
    return 0;
}
