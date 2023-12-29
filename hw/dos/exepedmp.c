
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>

#include <hw/dos/exehdr.h>
#include <hw/dos/exenehdr.h>
#include <hw/dos/exenepar.h>

#ifndef O_BINARY
#define O_BINARY (0)
#endif

#define EXE_PE_SIGNATURE                                                        (0x4550U)

/* Characteristics flags */
#define EXE_PE_HEADER_IMAGE_FILE_RELOCS_STRIPPED                                0x0001
#define EXE_PE_HEADER_IMAGE_FILE_EXECUTABLE_IMAGE                               0x0002
#define EXE_PE_HEADER_IMAGE_FILE_LINE_NUMS_STRIPPED                             0x0004
#define EXE_PE_HEADER_IMAGE_FILE_LOCAL_SYMS_STRIPPED                            0x0008
#define EXE_PE_HEADER_IMAGE_FILE_AGGRESSIVE_WS_TRIM                             0x0010
#define EXE_PE_HEADER_IMAGE_FILE_LARGE_ADDRESS_AWARE                            0x0020
#define EXE_PE_HEADER_IMAGE_FILE_16BIT_MACHINE                                  0x0040
#define EXE_PE_HEADER_IMAGE_FILE_BYTES_REVERSED_LO                              0x0080
#define EXE_PE_HEADER_IMAGE_FILE_32BIT_MACHINE                                  0x0100
#define EXE_PE_HEADER_IMAGE_FILE_DEBUG_STRIPPED                                 0x0200
#define EXE_PE_HEADER_IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP                        0x0400
#define EXE_PE_HEADER_IMAGE_FILE_NET_RUN_FROM_SWAP                              0x0800
#define EXE_PE_HEADER_IMAGE_FILE_SYSTEM                                         0x1000
#define EXE_PE_HEADER_IMAGE_FILE_DLL                                            0x2000
#define EXE_PE_HEADER_IMAGE_FILE_UP_SYSTEM_ONLY                                 0x4000
#define EXE_PE_HEADER_IMAGE_FILE_BYTES_REVERSED_HI                              0x8000

#pragma pack(push,1)
struct exe_pe_coff_file_header {
        uint16_t                                Machine;                        /* +0x00 target machine */
        uint16_t                                NumberOfSections;               /* +0x02 number of PE sections */
        uint32_t                                TimeDateStamp;                  /* +0x04 Time/Date timestamp */
        uint32_t                                PointerToSymbolTable;           /* +0x08 File offset of COFF symbol table or 0 */
        uint32_t                                NumberOfSymbols;                /* +0x0C Number of entries in symbol table */
        uint16_t                                SizeOfOptionalHeader;           /* +0x10 Size of the optional header */
        uint16_t                                Characteristics;                /* +0x12 Flags indicating attributes of the file */
};                                                                              /* =0x14 */
#pragma pack(pop)

#pragma pack(push,1)
/* This is the initial header. It does not define anything more because fields past this struct do vary */
struct exe_pe_header {
        uint32_t                                signature;                      /* +0x00 PE\0\0 */
        struct exe_pe_coff_file_header          fileheader;                     /* +0x04 COFF File Header (0x14 bytes) */
};                                                                              /* =0x18 */
/* optional header follows including section table */
#pragma pack(pop)

struct exe_pe_optional_header_raw {
        unsigned char*                          data;
        size_t                                  size;
        size_t                                  alloc_size;
};

const char *exe_pe_fileheader_machine_to_str(const uint16_t Machine) {
        switch (Machine) {
                case 0x0000:    return "UNKNOWN";
                case 0x014C:    return "I386";
                case 0x0162:    return "R3000";
                case 0x0166:    return "R4000";
                case 0x0168:    return "R10000";
                case 0x0169:    return "WCEMIPSV2";
                case 0x0184:    return "ALPHA";
                case 0x01A2:    return "SH3";
                case 0x01A3:    return "SH3DSP";
                case 0x01A6:    return "SH4";
                case 0x01A8:    return "SH5";
                case 0x01C0:    return "ARM";
                case 0x01C2:    return "THUMB";
                case 0x01D3:    return "AM33";
                case 0x01F0:    return "POWERPC";
                case 0x01F1:    return "POWERPCFP";
                case 0x0200:    return "IA64";
                case 0x0266:    return "MIPS16";
                case 0x0268:    return "M68K";
                case 0x0284:    return "ALPHA64";
                case 0x0366:    return "MIPSFPU";
                case 0x0466:    return "MIPSFPU16";
                case 0x0EBC:    return "EBC";
                case 0x8664:    return "AMD64";
                case 0x9041:    return "M32R";
                default:        break;
        }

        return "?";
}

#define EXE_PE_OPTHDR_MAGIC_ROM                         (0x0107)
#define EXE_PE_OPTHDR_MAGIC_PE				(0x010B)
#define EXE_PE_OPTHDR_MAGIC_PEPLUS			(0x020B)

#pragma pack(push,1)
struct exe_pe_opthdr_pe_standard { // EXE_PE_OPTHDR_MAGIC_PE
        uint16_t                                        Magic;                          /* +0x00 */
        uint8_t                                         MajorLinkerVersion;             /* +0x02 */
        uint8_t                                         MinorLinkerVersion;             /* +0x03 */
        uint32_t                                        SizeOfCode;                     /* +0x04 size of code [text], sum total */
        uint32_t                                        SizeOfInitializedData;          /* +0x08 size of initialized data, sum total */
        uint32_t                                        SizeOfUninitializedData;        /* +0x0C size of uninitialized data [bss], sum total */
        uint32_t                                        AddressOfEntryPoint;            /* +0x10 RVA of entry point, 0 if none (DLL) */
        uint32_t                                        BaseOfCode;                     /* +0x14 RVA of the base of code section */
        uint32_t                                        BaseOfData;                     /* +0x18 RVA of the base of data section */
};                                                                                      /* =0x1C */

struct exe_pe_opthdr_pe_windows {
        uint32_t                                        ImageBase;                      /* +0x00 (+0x1C) preferred loading address */
        uint32_t                                        SectionAlignment;               /* +0x04 (+0x20) */
        uint32_t                                        FileAlignment;                  /* +0x08 (+0x24) */
        uint16_t                                        MajorOperatingSystemVersion;    /* +0x0C (+0x28) */
        uint16_t                                        MinorOperatingSystemVersion;    /* +0x0E (+0x2A) */
        uint16_t                                        MajorImageVersion;              /* +0x10 (+0x2C) */
        uint16_t                                        MinorImageVersion;              /* +0x12 (+0x2E) */
        uint16_t                                        MajorSubsystemVersion;          /* +0x14 (+0x30) */
        uint16_t                                        MinorSubsystemVersion;          /* +0x16 (+0x32) */
        uint32_t                                        Win32VersionValue;              /* +0x18 (+0x34) Reserved, zero */
        uint32_t                                        SizeOfImage;                    /* +0x1C (+0x38) */
        uint32_t                                        SizeOfHeaders;                  /* +0x20 (+0x3C) */
        uint32_t                                        CheckSum;                       /* +0x24 (+0x40) */
        uint16_t                                        Subsystem;                      /* +0x28 (+0x44) */
        uint16_t                                        DLLCharacteristics;             /* +0x2A (+0x46) */
        uint32_t                                        SizeOfStackReserve;             /* +0x2C (+0x48) */
        uint32_t                                        SizeOfStackCommit;              /* +0x30 (+0x4C) */
        uint32_t                                        SizeOfHeapReserve;              /* +0x34 (+0x50) */
        uint32_t                                        SizeOfHeapCommit;               /* +0x38 (+0x54) */
        uint32_t                                        LoaderFlags;                    /* +0x3C (+0x58) */
        uint32_t                                        NumberOfRvaAndSizes;            /* +0x40 (+0x5C) */
};                                                                                      /* =0x44 (+0x60) */

struct exe_pe_opthdr_data_directory_entry {
	uint32_t					RVA;				/* +0x00 */
	uint32_t					Size;				/* +0x04 */
};											/* =0x08 */
/* NTS: The "RVA" is a file offset if it's the directory entry index for the certificate table. */

enum exe_pe_opthdr_data_directory_index {
        EXE_PE_DATADIRENT_EXPORT_TABLE=0,
        EXE_PE_DATADIRENT_IMPORT_TABLE=1,
        EXE_PE_DATADIRENT_RESOURCE_TABLE=2,
        EXE_PE_DATADIRENT_EXCEPTION_TABLE=3,
        EXE_PE_DATADIRENT_CERTIFICATE_TABLE=4, // "RVA" is a file offset
        EXE_PE_DATADIRENT_BASE_RELOCATION_TABLE=5,
        EXE_PE_DATADIRENT_DEBUG=6,
        EXE_PE_DATADIRENT_ARCHITECTURE=7,
        EXE_PE_DATADIRENT_GLOBAL_PTR=8, // RVA to store in global ptr register, size must be zero
        EXE_PE_DATADIRENT_TLS_TABLE=9,
        EXE_PE_DATADIRENT_LOAD_CONFIG_TABLE=10,
        EXE_PE_DATADIRENT_BOUND_IMPORT=11,
        EXE_PE_DATADIRENT_IAT=12,
        EXE_PE_DATADIRENT_DELAY_IMPORT_DESCRIPTOR=13,
        EXE_PE_DATADIRENT_COMPLUS_RUNTIME_HEADER=14,
        EXE_PE_DATADIRENT_CLR_RUNTIME_HEADER=14,
        EXE_PE_DATADIRENT_RESERVED15=15,

        EXE_PE_DATADIRENT__MAX
};

const char *exe_pe_opthdr_data_directory_entry_str[EXE_PE_DATADIRENT__MAX] = {
        "Export table",                 // 0
        "Import table",
        "Resource table",
        "Exception table",
        "Certificate table",
        "Base relocation table",        // 5
        "Debug",
        "Architecture",
        "Global ptr",
        "TLS table",
        "Load config table",            // 10
        "Bound import",
        "IAT",
        "Delay import descriptor",
        "COM+/CLR runtime header",
        "Reserved"                      // 15
};

const char *exe_pe_opthdr_data_directory_entry_to_str(unsigned int i) {
        if (i < EXE_PE_DATADIRENT__MAX)
                return exe_pe_opthdr_data_directory_entry_str[i];
        return "?";
}

enum exe_pe_opthdr_subsystem {
        EXE_PE_OPTHDR_SUBSYSTEM_UNKNOWN=0,
        EXE_PE_OPTHDR_SUBSYSTEM_NATIVE=1,
        EXE_PE_OPTHDR_SUBSYSTEM_WINDOWS_GUI=2,
        EXE_PE_OPTHDR_SUBSYSTEM_WINDOWS_CUI=3,
        EXE_PE_OPTHDR_SUBSYSTEM_POSIX_CUI=7,
        EXE_PE_OPTHDR_SUBSYSTEM_WINDOWS_CE_GUI=9,
        EXE_PE_OPTHDR_SUBSYSTEM_EFI_APPLICATION=10,
        EXE_PE_OPTHDR_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER=11,
        EXE_PE_OPTHDR_SUBSYSTEM_EFI_RUNTIME_DRIVER=12,

        EXE_PE_OPTHDR_SUBSYSTEM__MAX
};

const char *exe_pe_opthdr_subsystem_to_str(const uint16_t s) {
        switch (s) {
                case EXE_PE_OPTHDR_SUBSYSTEM_UNKNOWN:           return "UNKNOWN";
                case EXE_PE_OPTHDR_SUBSYSTEM_NATIVE:            return "NATIVE";
                case EXE_PE_OPTHDR_SUBSYSTEM_WINDOWS_GUI:       return "WINDOWS_GUI";
                case EXE_PE_OPTHDR_SUBSYSTEM_WINDOWS_CUI:       return "WINDOWS_CUI";
                case EXE_PE_OPTHDR_SUBSYSTEM_POSIX_CUI:         return "POSIX_CUI";
                case EXE_PE_OPTHDR_SUBSYSTEM_WINDOWS_CE_GUI:    return "WINDOWS_CE_GUI";
                case EXE_PE_OPTHDR_SUBSYSTEM_EFI_APPLICATION:   return "EFI_APPLICATION";
                case EXE_PE_OPTHDR_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER: return "EFI_BOOT_SERVICE_DRIVER";
                case EXE_PE_OPTHDR_SUBSYSTEM_EFI_RUNTIME_DRIVER: return "EFI_RUNTIME_DRIVER";
                default:                                        break;
        }

        return "?";
}

struct exe_pe_opthdr_pe { // EXE_PE_OPTHDR_MAGIC_PE
        struct exe_pe_opthdr_pe_standard	        standard;			/* +0x00 */
	struct exe_pe_opthdr_pe_windows			windows;			/* +0x1C (size 0x44) */
};										        /* =0x60 */
/* data directory follows */
#pragma pack(pop)

const char *exe_ne_opthdr_magic_to_str(const uint16_t magic) {
        switch (magic) {
                case EXE_PE_OPTHDR_MAGIC_ROM:           return "ROM";
                case EXE_PE_OPTHDR_MAGIC_PE:            return "PE";
                case EXE_PE_OPTHDR_MAGIC_PEPLUS:        return "PE+";
                default:                                break;
        }

        return "?";
}

uint16_t exe_pe_opthdr_magic_value(struct exe_pe_optional_header_raw *h) {
        if (h->size >= 2)
                return *((uint16_t*)(h->data+0));

        return 0;
}

struct exe_pe_opthdr_data_directory_entry *exe_pe_opthdr_data_directory(struct exe_pe_optional_header_raw *h,size_t entry) {
#define COMMON_RETURN(ht,dt,entry) \
    if (h->size >= sizeof(ht)) {\
        ht *hdr = (ht*)(h->data);\
        if (entry < hdr->windows.NumberOfRvaAndSizes) {\
            const size_t ofs = sizeof(ht)/*follows opthdr*/ + (entry * sizeof(dt));\
            if ((ofs + sizeof(dt)) <= h->size)\
                return (dt*)(h->data + ofs);\
        }\
    }

        switch (exe_pe_opthdr_magic_value(h)) {
                case EXE_PE_OPTHDR_MAGIC_PE:
                        COMMON_RETURN(struct exe_pe_opthdr_pe, struct exe_pe_opthdr_data_directory_entry, entry);
			break;
                default:
                        break;
        }

        return NULL;
#undef COMMON_RETURN
}

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

int main(int argc,char **argv) {
    struct exe_pe_optional_header_raw pe_opthdr_raw;
    struct exe_pe_header pe_header;
    uint32_t pe_header_offset;
    uint32_t file_size;
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
        read(src_fd,&pe_header_offset,4) != 4) {
        fprintf(stderr,"Cannot read extension\n");
        return 1;
    }
    printf("    EXE extension (if exists) at: %lu\n",(unsigned long)pe_header_offset);

    /* go read the extended header */
    if (lseek(src_fd,pe_header_offset,SEEK_SET) != pe_header_offset ||
        read(src_fd,&pe_header,sizeof(pe_header)) != sizeof(pe_header)) {
        fprintf(stderr,"Cannot read PE header\n");
        return 1;
    }
    if (pe_header.signature != EXE_PE_SIGNATURE) {
        fprintf(stderr,"Not an PE executable\n");
        return 1;
    }

    printf("* PE header at %lu\n",(unsigned long)pe_header_offset);
    printf("    Machine:                        0x%04x (%s)\n",
        pe_header.fileheader.Machine,
        exe_pe_fileheader_machine_to_str(pe_header.fileheader.Machine));
    printf("    NumberOfSections:               %u\n",
        pe_header.fileheader.NumberOfSections);
    printf("    TimeDateStamp:                  %lu (0x%08lx)\n",
        (unsigned long)pe_header.fileheader.TimeDateStamp,
        (unsigned long)pe_header.fileheader.TimeDateStamp);
    printf("    PointerToSymbolTable:           %lu (0x%08lx) (file offset)\n",
        (unsigned long)pe_header.fileheader.PointerToSymbolTable,
        (unsigned long)pe_header.fileheader.PointerToSymbolTable);
    printf("    NumberOfSymbols:                %lu\n",
        (unsigned long)pe_header.fileheader.NumberOfSymbols);
    printf("    SizeOfOptionalHeader:           %lu\n",
        (unsigned long)pe_header.fileheader.SizeOfOptionalHeader);
    printf("    Characteristics:                0x%04x\n",
        pe_header.fileheader.Characteristics);
    if (pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_RELOCS_STRIPPED)
        printf("      - Relocations stripped\n");
    if (pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_EXECUTABLE_IMAGE)
        printf("      - Image is valid and executable\n");
    if (pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_LINE_NUMS_STRIPPED)
        printf("      - Line numbers have been removed\n");
    if (pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_LOCAL_SYMS_STRIPPED)
        printf("      - Local symbols have been removed\n");
    if (pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_AGGRESSIVE_WS_TRIM)
        printf("      - Aggressively trim working set\n");
    if (pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_LARGE_ADDRESS_AWARE)
        printf("      - Large address aware\n");
    if (pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_16BIT_MACHINE)
        printf("      - 16-bit machine / Reserved\n");
    if (pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_BYTES_REVERSED_LO)
        printf("      - Little endian (LSB before MSB)\n");
    if (pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_32BIT_MACHINE)
        printf("      - 32-bit machine\n");
    if (pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_DEBUG_STRIPPED)
        printf("      - Debugging information stripped from image file\n");
    if (pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP)
        printf("      - Copy and run from swap if run from removable media\n");
    if (pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_NET_RUN_FROM_SWAP)
        printf("      - Copy and run from swap if run from network media\n");
    if (pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_SYSTEM)
        printf("      - Image is a system file, not user program\n");
    if (pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_DLL)
        printf("      - Image is a DLL library\n");
    if (pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_UP_SYSTEM_ONLY)
        printf("      - Run only on a UP machine\n");
    if (pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_BYTES_REVERSED_HI)
        printf("      - Big endian (MSB before LSB)\n");

    /* read the optional header */
    if (pe_header.fileheader.SizeOfOptionalHeader >= 2 && pe_header.fileheader.SizeOfOptionalHeader <= 0x4000) {
        uint16_t opt_magic;

        pe_opthdr_raw.size = pe_header.fileheader.SizeOfOptionalHeader;
        pe_opthdr_raw.alloc_size = 512;
        if (pe_opthdr_raw.alloc_size < pe_opthdr_raw.size)
                pe_opthdr_raw.alloc_size = pe_opthdr_raw.size;
        pe_opthdr_raw.data = malloc(pe_opthdr_raw.alloc_size);
        if (!pe_opthdr_raw.data) {
            printf("! Failed to allocate memory for optional header\n");
            return 1;
        }
        memset(pe_opthdr_raw.data,0,pe_opthdr_raw.alloc_size);
        if ((size_t)read(src_fd,pe_opthdr_raw.data,pe_opthdr_raw.size) != pe_opthdr_raw.size) {
            printf("! Failed to read optional header\n");
            return 1;
        }

        opt_magic = exe_pe_opthdr_magic_value(&pe_opthdr_raw);

        printf("    Optional header magic:          0x%04x (%s)\n",
            opt_magic,exe_ne_opthdr_magic_to_str(opt_magic));

	printf(" == Optional header, standard section ==\n");

#define HS(x) hdr->standard.x
#define WS(x) hdr->windows.x
#define EXIST(w) ( ( (size_t)((char*)(&(w))) + sizeof(w) - (size_t)((char*)(hdr)) ) <= pe_opthdr_raw.size )
        if (opt_magic == EXE_PE_OPTHDR_MAGIC_PE) {
            struct exe_pe_opthdr_pe *hdr = (struct exe_pe_opthdr_pe*)(pe_opthdr_raw.data);
            assert(pe_opthdr_raw.alloc_size >= sizeof(*hdr));

            if (EXIST(HS(MinorLinkerVersion))) {
                printf("    Linker version:                 %u.%u\n",
                    HS(MajorLinkerVersion),
                    HS(MinorLinkerVersion));
            }
            if (EXIST(HS(SizeOfCode))) {
                printf("    SizeOfCode:                     0x%08lx\n",
                    (unsigned long)HS(SizeOfCode));
            }
            if (EXIST(HS(SizeOfInitializedData))) {
                printf("    SizeOfInitializedData:          0x%08lx\n",
                    (unsigned long)HS(SizeOfInitializedData));
            }
            if (EXIST(HS(SizeOfUninitializedData))) {
                printf("    SizeOfUninitializedData:        0x%08lx\n",
                    (unsigned long)HS(SizeOfUninitializedData));
            }
            if (EXIST(HS(AddressOfEntryPoint))) {
                printf("    AddressOfEntryPoint:            0x%08lx (RVA)\n",
                    (unsigned long)HS(AddressOfEntryPoint));
            }
            if (EXIST(HS(BaseOfCode))) {
                printf("    BaseOfCode:                     0x%08lx (RVA)\n",
                    (unsigned long)HS(BaseOfCode));
            }
            if (EXIST(HS(BaseOfData))) {
                printf("    BaseOfData:                     0x%08lx (RVA)\n",
                    (unsigned long)HS(BaseOfData));
            }
	}

        printf(" == Optional header, windows section ==\n");

        if (opt_magic == EXE_PE_OPTHDR_MAGIC_PE) {
            struct exe_pe_opthdr_pe *hdr = (struct exe_pe_opthdr_pe*)(pe_opthdr_raw.data);
            assert(pe_opthdr_raw.alloc_size >= sizeof(*hdr));

            if (EXIST(WS(ImageBase))) {
                printf("    ImageBase:                      0x%08lx (VA)\n",
                    (unsigned long)WS(ImageBase));
            }
            if (EXIST(WS(SectionAlignment))) {
                printf("    SectionAlignment;               0x%08lx\n",
                    (unsigned long)WS(SectionAlignment));
            }
            if (EXIST(WS(FileAlignment))) {
                printf("    FileAlignment:                  0x%08lx\n",
                    (unsigned long)WS(FileAlignment));
            }
            if (EXIST(WS(MinorOperatingSystemVersion))) {
                printf("    Operating System Version:       %u.%u\n",
                    WS(MajorOperatingSystemVersion),
                    WS(MinorOperatingSystemVersion));
            }
            if (EXIST(WS(MinorImageVersion))) {
                printf("    Image Version:                  %u.%u\n",
                    WS(MajorImageVersion),
                    WS(MinorImageVersion));
            }
            if (EXIST(WS(MinorSubsystemVersion))) {
                printf("    Subsystem version:              %u.%u\n",
                    WS(MajorSubsystemVersion),
                    WS(MinorSubsystemVersion));
            }
            if (EXIST(WS(Win32VersionValue))) {
                printf("    Win32VersionValue/Reserved:     0x%08lx\n",
                    (unsigned long)WS(Win32VersionValue));
            }
            if (EXIST(WS(SizeOfImage))) {
                printf("    SizeOfImage:                    0x%08lx\n",
                    (unsigned long)WS(SizeOfImage));
            }
            if (EXIST(WS(SizeOfHeaders))) {
                printf("    SizeOfHeaders:                  0x%08lx\n",
                    (unsigned long)WS(SizeOfHeaders));
            }
            if (EXIST(WS(CheckSum))) {
                printf("    CheckSum:                       0x%08lx\n",
                    (unsigned long)WS(CheckSum));
            }
            if (EXIST(WS(Subsystem))) {
                printf("    Subsystem:                      0x%08lx (%s)\n",
                    (unsigned long)WS(Subsystem),
                    exe_pe_opthdr_subsystem_to_str(WS(Subsystem)));
            }
            if (EXIST(WS(DLLCharacteristics))) {
                printf("    DLL Characteristics:            0x%08lx\n",
                    (unsigned long)WS(DLLCharacteristics));
            }
            if (EXIST(WS(SizeOfStackReserve))) {
                printf("    SizeOfStackReserve:             0x%08lx\n",
                    (unsigned long)WS(SizeOfStackReserve));
            }
            if (EXIST(WS(SizeOfStackCommit))) {
                printf("    SizeOfStackCommit:              0x%08lx\n",
                    (unsigned long)WS(SizeOfStackCommit));
            }
            if (EXIST(WS(SizeOfHeapReserve))) {
                printf("    SizeOfHeapReserve:              0x%08lx\n",
                    (unsigned long)WS(SizeOfHeapReserve));
            }
            if (EXIST(WS(SizeOfHeapCommit))) {
                printf("    SizeOfHeapCommit:               0x%08lx\n",
                    (unsigned long)WS(SizeOfHeapCommit));
            }
            if (EXIST(WS(LoaderFlags))) {
                printf("    LoaderFlags:                    0x%08lx\n",
                    (unsigned long)WS(LoaderFlags));
            }
            if (EXIST(WS(NumberOfRvaAndSizes))) {
                printf("    NumberOfRvaAndSizes:            %lu\n",
                    (unsigned long)WS(NumberOfRvaAndSizes));
            }
        }
#undef EXIST
#undef WS
#undef HS

        printf(" == Optional header, data directory ==\n");
        {
            struct exe_pe_opthdr_data_directory_entry *ent;
            unsigned int i=0;

            while ((ent=exe_pe_opthdr_data_directory(&pe_opthdr_raw,i)) != NULL) {
                printf("    Data directory #%u (%s):\n",i,exe_pe_opthdr_data_directory_entry_to_str(i));
                if (i == EXE_PE_DATADIRENT_CERTIFICATE_TABLE) {
                    printf("        File offset:                0x%08lx\n",
                        (unsigned long)(ent->RVA));
                }
                else {
                    printf("        RVA:                        0x%08lx\n",
                        (unsigned long)(ent->RVA));
                }
                printf("        Size:                       0x%08lx\n",
                    (unsigned long)(ent->Size));
                i++;
            }
        }
    }

    close(src_fd);
    return 0;
}
