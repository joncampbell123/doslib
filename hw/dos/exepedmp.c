
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
#pragma pack(pop)

#pragma pack(push,1)
struct exe_pe_opthdr_peplus_standard { // EXE_PE_OPTHDR_MAGIC_PEPLUS
        uint16_t                                        Magic;                          /* +0x00 */
        uint8_t                                         MajorLinkerVersion;             /* +0x02 */
        uint8_t                                         MinorLinkerVersion;             /* +0x03 */
        uint32_t                                        SizeOfCode;                     /* +0x04 size of code [text], sum total */
        uint32_t                                        SizeOfInitializedData;          /* +0x08 size of initialized data, sum total */
        uint32_t                                        SizeOfUninitializedData;        /* +0x0C size of uninitialized data [bss], sum total */
        uint32_t                                        AddressOfEntryPoint;            /* +0x10 RVA of entry point, 0 if none (DLL) */
        uint32_t                                        BaseOfCode;                     /* +0x14 RVA of the base of code section */
};                                                                                      /* =0x18 */
#pragma pack(pop)

#pragma pack(push,1)
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
#pragma pack(pop)

#pragma pack(push,1)
struct exe_pe_opthdr_peplus_windows {
        uint64_t                                        ImageBase;                      /* +0x00 (+0x18) preferred loading address */
        uint32_t                                        SectionAlignment;               /* +0x08 (+0x20) */
        uint32_t                                        FileAlignment;                  /* +0x0C (+0x24) */
        uint16_t                                        MajorOperatingSystemVersion;    /* +0x10 (+0x28) */
        uint16_t                                        MinorOperatingSystemVersion;    /* +0x12 (+0x2A) */
        uint16_t                                        MajorImageVersion;              /* +0x14 (+0x2C) */
        uint16_t                                        MinorImageVersion;              /* +0x16 (+0x2E) */
        uint16_t                                        MajorSubsystemVersion;          /* +0x18 (+0x30) */
        uint16_t                                        MinorSubsystemVersion;          /* +0x1A (+0x32) */
        uint32_t                                        Win32VersionValue;              /* +0x1C (+0x34) Reserved, zero */
        uint32_t                                        SizeOfImage;                    /* +0x20 (+0x38) */
        uint32_t                                        SizeOfHeaders;                  /* +0x24 (+0x3C) */
        uint32_t                                        CheckSum;                       /* +0x28 (+0x40) */
        uint16_t                                        Subsystem;                      /* +0x2C (+0x44) */
        uint16_t                                        DLLCharacteristics;             /* +0x2E (+0x46) */
        uint64_t                                        SizeOfStackReserve;             /* +0x30 (+0x48) */
        uint64_t                                        SizeOfStackCommit;              /* +0x38 (+0x50) */
        uint64_t                                        SizeOfHeapReserve;              /* +0x40 (+0x58) */
        uint64_t                                        SizeOfHeapCommit;               /* +0x48 (+0x60) */
        uint32_t                                        LoaderFlags;                    /* +0x50 (+0x68) */
        uint32_t                                        NumberOfRvaAndSizes;            /* +0x54 (+0x6C) */
};                                                                                      /* =0x58 (+0x70) */
#pragma pack(pop)

#pragma pack(push,1)
struct exe_pe_opthdr_data_directory_entry {
	uint32_t					RVA;				/* +0x00 */
	uint32_t					Size;				/* +0x04 */
};											/* =0x08 */
#pragma pack(pop)
/* NTS: The "RVA" is a file offset if it's the directory entry index for the certificate table. */

#define EXE_PE_OPTHDR_DLLCHARACT_DYNAMIC_BASE                           0x0040
#define EXE_PE_OPTHDR_DLLCHARACT_FORCE_INTEGRITY                        0x0080
#define EXE_PE_OPTHDR_DLLCHARACT_NX_COMPAT                              0x0100
#define EXE_PE_OPTHDR_DLLCHARACT_NO_ISOLATION                           0x0200
#define EXE_PE_OPTHDR_DLLCHARACT_NO_SEH                                 0x0400
#define EXE_PE_OPTHDR_DLLCHARACT_NO_BIND                                0x0800
#define EXE_PE_OPTHDR_DLLCHARACT_WDM_DRIVER                             0x2000
#define EXE_PE_OPTHDR_DLLCHARACT_TERMINAL_SERVER_AWARE                  0x8000

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
        EXE_PE_OPTHDR_SUBSYSTEM_EFI_ROM=13,
        EXE_PE_OPTHDR_SUBSYSTEM_XBOX=14,

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
                case EXE_PE_OPTHDR_SUBSYSTEM_EFI_ROM:           return "EFI_ROM";
                case EXE_PE_OPTHDR_SUBSYSTEM_XBOX:              return "XBOX";
                default:                                        break;
        }

        return "?";
}

#pragma pack(push,1)
struct exe_pe_opthdr_pe { // EXE_PE_OPTHDR_MAGIC_PE
        struct exe_pe_opthdr_pe_standard	        standard;			/* +0x00 */
	struct exe_pe_opthdr_pe_windows			windows;			/* +0x1C (size 0x44) */
};										        /* =0x60 */
/* data directory follows */
#pragma pack(pop)

#pragma pack(push,1)
struct exe_pe_opthdr_peplus { // EXE_PE_OPTHDR_MAGIC_PEPLUS
        struct exe_pe_opthdr_peplus_standard            standard;			/* +0x00 */
	struct exe_pe_opthdr_peplus_windows             windows;			/* +0x18 (size 0x58) */
};										        /* =0x70 */
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

#pragma pack(push,1)
struct exe_pe_section_table_entry {
        char                                            Name[8];                        /* +0x00 */
        uint32_t                                        VirtualSize;                    /* +0x08 */
        uint32_t                                        VirtualAddress;                 /* +0x0C */
        uint32_t                                        SizeOfRawData;                  /* +0x10 */
        uint32_t                                        PointerToRawData;               /* +0x14 */
        uint32_t                                        PointerToRelocations;           /* +0x18 */
        uint32_t                                        PointerToLinenumbers;           /* +0x1C */
        uint16_t                                        NumberOfRelocations;            /* +0x20 */
        uint16_t                                        NumberOfLinenumbers;            /* +0x22 */
        uint32_t                                        Characteristics;                /* +0x24 */
};											/* =0x28 */
#pragma pack(pop)

#define EXE_PE_SECTFLAGS_TYPE_REG                                               0x00000000 /* not a flag, just a constant */
#define EXE_PE_SECTFLAGS_TYPE_DSECT                                             0x00000001
#define EXE_PE_SECTFLAGS_TYPE_NOLOAD                                            0x00000002
#define EXE_PE_SECTFLAGS_TYPE_GROUP                                             0x00000004
#define EXE_PE_SECTFLAGS_TYPE_NO_PAD                                            0x00000008
#define EXE_PE_SECTFLAGS_TYPE_COPY                                              0x00000010
#define EXE_PE_SECTFLAGS_CNT_CODE                                               0x00000020
#define EXE_PE_SECTFLAGS_CNT_INITIALIZED_DATA                                   0x00000040
#define EXE_PE_SECTFLAGS_CNT_UNINITIALIZED_DATA                                 0x00000080
#define EXE_PE_SECTFLAGS_LNK_OTHER                                              0x00000100
#define EXE_PE_SECTFLAGS_LNK_INFO                                               0x00000200
#define EXE_PE_SECTFLAGS_TYPE_OVER                                              0x00000400
#define EXE_PE_SECTFLAGS_LNK_REMOVE                                             0x00000800
#define EXE_PE_SECTFLAGS_LNK_COMDAT                                             0x00001000
#define EXE_PE_SECTFLAGS_MEM_FARDATA                                            0x00008000
#define EXE_PE_SECTFLAGS_MEM_PURGEABLE                                          0x00020000
#define EXE_PE_SECTFLAGS_MEM_16BIT                                              0x00020000
#define EXE_PE_SECTFLAGS_MEM_LOCKED                                             0x00040000
#define EXE_PE_SECTFLAGS_MEM_PRELOAD                                            0x00080000

/* enumeration within SCN_ALIGN_MASK */
#define EXE_PE_SECTFLAGS_SCN_ALIGN_1BYTES                                       0x00100000
#define EXE_PE_SECTFLAGS_SCN_ALIGN_2BYTES                                       0x00200000
#define EXE_PE_SECTFLAGS_SCN_ALIGN_4BYTES                                       0x00300000
#define EXE_PE_SECTFLAGS_SCN_ALIGN_8BYTES                                       0x00400000
#define EXE_PE_SECTFLAGS_SCN_ALIGN_16BYTES                                      0x00500000
#define EXE_PE_SECTFLAGS_SCN_ALIGN_32BYTES                                      0x00600000
#define EXE_PE_SECTFLAGS_SCN_ALIGN_64BYTES                                      0x00700000
#define EXE_PE_SECTFLAGS_SCN_ALIGN_128BYTES                                     0x00800000
#define EXE_PE_SECTFLAGS_SCN_ALIGN_256BYTES                                     0x00900000
#define EXE_PE_SECTFLAGS_SCN_ALIGN_512BYTES                                     0x00A00000
#define EXE_PE_SECTFLAGS_SCN_ALIGN_1024BYTES                                    0x00B00000
#define EXE_PE_SECTFLAGS_SCN_ALIGN_2048BYTES                                    0x00C00000
#define EXE_PE_SECTFLAGS_SCN_ALIGN_4096BYTES                                    0x00D00000
#define EXE_PE_SECTFLAGS_SCN_ALIGN_8192BYTES                                    0x00E00000
#define EXE_PE_SECTFLAGS_SCN_ALIGN_MASK                                         0x00F00000
#define EXE_PE_SECTFLAGS_SCN_ALIGN_MASKSHIFT                                    20

#define EXE_PE_SECTFLAGS_LNK_NRELOC_OVFL                                        0x01000000
#define EXE_PE_SECTFLAGS_MEM_DISCARDABLE                                        0x02000000
#define EXE_PE_SECTFLAGS_MEM_NOT_CACHED                                         0x04000000
#define EXE_PE_SECTFLAGS_MEM_NOT_PAGED                                          0x08000000
#define EXE_PE_SECTFLAGS_MEM_SHARED                                             0x10000000
#define EXE_PE_SECTFLAGS_MEM_EXECUTE                                            0x20000000
#define EXE_PE_SECTFLAGS_MEM_READ                                               0x40000000
#define EXE_PE_SECTFLAGS_MEM_WRITE                                              0x80000000

typedef uint64_t EXE_PE_VA; /* virtual address */
typedef uint32_t EXE_PE_RVA; /* relative virtual address */

/* hash table lookup */
#define EXE_PE_HASHTBL_SIZE     256
typedef uint8_t EXE_PE_HASH_T;

struct pe_header_vablock { // one 4KB block of virtual address space
	unsigned char*			MEM;
	EXE_PE_VA			VA;
	struct pe_header_vablock*	next;
};

struct pe_header_vamem {
	struct pe_header_vablock*	hash[EXE_PE_HASHTBL_SIZE]; // alloc here
	EXE_PE_VA			pagemask;
	EXE_PE_VA			alloc;
	EXE_PE_VA			alloc_limit;
};

void pe_header_vablock_free(struct pe_header_vamem *v,struct pe_header_vablock **h) {
	if (*h) {
		struct pe_header_vablock *n = (*h)->next;
		if ((*h)->MEM) free((*h)->MEM);
		(*h)->next = NULL;
		(*h)->VA = 0;
		free(*h);
		*h = n;

		assert(v->alloc >= (v->pagemask+(EXE_PE_VA)1u));
		v->alloc -= (v->pagemask+(EXE_PE_VA)1u);
	}
}

void pe_header_vablock_freeall(struct pe_header_vamem *v,struct pe_header_vablock **h) {
	while (*h != NULL)
		pe_header_vablock_free(v,h);
}

EXE_PE_HASH_T pe_header_parser_hashva(EXE_PE_VA v) {
        v ^=  (v >> (EXE_PE_VA)32u) ^ (EXE_PE_VA)0xA151;
        v ^= ~(v >> (EXE_PE_VA)16u) ^ (EXE_PE_VA)0x23B6;
        v ^=  (v >>  (EXE_PE_VA)8u) ^ (EXE_PE_VA)0x19EA;
        return (EXE_PE_HASH_T)v;
}

static inline size_t pe_header_vamem_pageofs(struct pe_header_vamem *v,EXE_PE_VA va) {
	return (size_t)(va & v->pagemask); /* byte offset within page */
}

struct pe_header_vablock *pe_header_vablock_lookup(struct pe_header_vablock *h,EXE_PE_VA va) {
	for (;h != NULL;h=h->next) {
		if (h->VA == va)
			return h;
	}

	return NULL;
}

struct pe_header_vablock *pe_header_vablock_insert(struct pe_header_vamem *v,struct pe_header_vablock **h,EXE_PE_VA va) {
	/* enforce allocation limit */
	if (v->alloc >= v->alloc_limit)
		return NULL;

	/* assumes caller has already checked there is no block for page containing va, va is already masked by ~pagemask */
	{
		struct pe_header_vablock *nb = malloc(sizeof(struct pe_header_vablock));
		if (nb != NULL) {
			nb->MEM = malloc((size_t)(v->pagemask+1u));
			if (!nb->MEM) {
				free(nb);
				return NULL;
			}
			memset(nb->MEM,0,(size_t)(v->pagemask+1u));
			v->alloc += (v->pagemask+(EXE_PE_VA)1u);
			nb->VA = va;

			/* insert into head of list */
			nb->next = (*h);
			(*h) = nb;
			return nb;
		}
	}

	return NULL;
}

struct pe_header_vablock *pe_header_vamem_insert(struct pe_header_vamem *v,EXE_PE_VA va) {
	/* assumes caller has already checked there is no block for page containing va */
	const EXE_PE_HASH_T hashi = pe_header_parser_hashva(va & (~v->pagemask));
	return pe_header_vablock_insert(v,&v->hash[hashi],va & (~v->pagemask));
}

struct pe_header_vablock *pe_header_vamem_lookup(struct pe_header_vamem *v,EXE_PE_VA va) {
	const EXE_PE_HASH_T hashi = pe_header_parser_hashva(va & (~v->pagemask));
	return pe_header_vablock_lookup(v->hash[hashi],va & (~v->pagemask));
}

void pe_header_vamem_free(struct pe_header_vamem *v) {
	unsigned int i;

	for (i=0;i < EXE_PE_HASHTBL_SIZE;i++)
		pe_header_vablock_freeall(v,&v->hash[i]);

	assert(v->alloc == 0);
	free(v);
}

struct pe_header_vamem *pe_header_vamem_alloc(uint32_t pagesize) {
	struct pe_header_vamem *v;

	if (pagesize == 0 || (pagesize & (pagesize - 1u)) != 0) /* must be power of 2 and nonzero */
		return NULL;

	v = malloc(sizeof(*v));
	if (v == NULL)
		return NULL;

	memset(v,0,sizeof(*v));
	v->pagemask = pagesize - 1u;
#if TARGET_MSDOS == 16
#  if defined(__LARGE__) || defined(__COMPACT__) || defined(__HUGE__)
	v->alloc_limit = (EXE_PE_VA)256u << (EXE_PE_VA)10u; // 256KB limit (should fit well within 640KB)
#  else
	v->alloc_limit = (EXE_PE_VA)16u << (EXE_PE_VA)10u; // 16KB limit (should fit well within 64KB)
#  endif
#else
	v->alloc_limit = (EXE_PE_VA)32u << (EXE_PE_VA)20u; // 32MB limit (Linux kernel OOM killer will terminate this program if we allocate too much memory)
#endif
	return v;
}

struct pe_header_parser {
        int                                             src_fd; /* does not own descriptor */
        uint32_t                                        pe_header_offset;
        struct exe_pe_header                            pe_header;
        uint32_t                                        opthdr_offset;
        uint16_t                                        opthdr_magic;
        struct exe_pe_optional_header_raw               opthdr_raw; /* allocated */
        struct exe_pe_opthdr_data_directory_entry*      datadir; /* points into opthdr_raw */
        size_t                                          datadir_count;
        uint32_t                                        sections_offset;
        struct exe_pe_section_table_entry*              sections; /* allocated */
        size_t                                          sections_count;
        EXE_PE_VA                                       imagebase;
        EXE_PE_VA                                       imagesize;
        uint32_t                                        sectionalign;
        struct pe_header_vamem*                         vmem;
};

#pragma pack(push,1)
struct pe_header_import_directory_table {
        uint32_t                                        ImportLookupTableRVA;     /* +0x00 RVA */
        uint32_t                                        TimeDateStamp;            /* +0x04 */
        uint32_t                                        ForwarderChain;           /* +0x08 index */
        uint32_t                                        NameRVA;                  /* +0x0C RVA */
        uint32_t                                        ImportAddressTableRVA;    /* +0x10 RVA */
};                                                                                /* =0x14 */
#pragma pack(pop)

#pragma pack(push,1)
struct pe_header_export_directory_table {
        uint32_t                                        ExportFlags;                    /* +0x00 */
        uint32_t                                        TimeDateStamp;                  /* +0x04 */
        uint16_t                                        MajorVersion;                   /* +0x08 */
        uint16_t                                        MinorVersion;                   /* +0x0A */
        uint32_t                                        NameRVA;                        /* +0x0C */
        uint32_t                                        OrdinalBase;                    /* +0x10 */
        uint32_t                                        AddressTableEntries;            /* +0x14 */
        uint32_t                                        NumberOfNamePointers;           /* +0x18 */
        uint32_t                                        ExportAddressTableRVA;          /* +0x1C */
        uint32_t                                        NamePointerRVA;                 /* +0x20 */
        uint32_t                                        OrdinalTableRVA;                /* +0x24 */
};                                                                                      /* =0x28 */
#pragma pack(pop)

#pragma pack(push,1)
struct pe_header_resource_directory_table {
        uint32_t                                        Characteristics;                /* +0x00 */
        uint32_t                                        TimeDateStamp;                  /* +0x04 */
        uint16_t                                        MajorVersion;                   /* +0x08 */
        uint16_t                                        MinorVersion;                   /* +0x0A */
        uint16_t                                        NumberOfNameEntries;            /* +0x0C */
        uint16_t                                        NumberOfIDEntries;              /* +0x0E */
};                                                                                      /* =0x10 */
#pragma pack(pop)

#pragma pack(push,1)
struct pe_header_resource_directory_entry {
	union {
		uint32_t				NameRVA;			/* +0x00 */
		uint32_t				IntegerID;			/* +0x00 */
	} f;
	union {
		uint32_t				DataEntryRVA;			/* +0x04 */
		uint32_t				SubdirectoryRVA;		/* +0x04 */
	} s;
};                                                                                      /* =0x08 */
#pragma pack(pop)

static inline EXE_PE_VA pe_header_parser_RVAtoVA(struct pe_header_parser *hp,const EXE_PE_RVA addr) {
        return (EXE_PE_VA)addr + hp->imagebase;
}

static inline EXE_PE_RVA pe_header_parser_VAtoRVA(struct pe_header_parser *hp,const EXE_PE_VA addr) {
        return (EXE_PE_RVA)(addr - hp->imagebase);
}

struct pe_header_vamem *pe_header_parser_vamem(struct pe_header_parser *hp) {
	if (!hp->vmem) hp->vmem = pe_header_vamem_alloc(hp->sectionalign);
	return hp->vmem;
}

struct exe_pe_section_table_entry *pe_header_parser_section_table_lookupVA(struct pe_header_parser *hp,EXE_PE_VA va) {
        struct exe_pe_section_table_entry *himatch = NULL;
        size_t si;

        /* Microsoft says that the sections must be in ascending order and adjacent, probably so that their code can search like this */
        if (hp->sections != NULL) {
                for (si=0;si < hp->sections_count;si++) {
                        struct exe_pe_section_table_entry *ent = hp->sections + si;
                        if (va >= (hp->imagebase+ent->VirtualAddress))
                                himatch = ent;
                        else
                                break;
                }
        }

        return himatch;
}

/* clear a block (TODO: According to a LRU algorithm) to free up memory to allow for another one to be allocated */
void pe_header_vamem_clear_lrused(struct pe_header_vamem *v) {
	unsigned int hi;

	for (hi=0;hi < EXE_PE_HASHTBL_SIZE;hi++) {
		struct pe_header_vablock **h = &v->hash[hi];

		if (*h != NULL) {
			pe_header_vablock_free(v,h);
			break;
		}
	}
}

struct pe_header_vablock *pe_header_vamem_insert_and_free_lrused(struct pe_header_vamem *v,EXE_PE_VA va) {
	/* assumes caller has already checked there is no block for page containing va */
	const EXE_PE_HASH_T hashi = pe_header_parser_hashva(va & (~v->pagemask));
	struct pe_header_vablock *blk;
	unsigned int tryi=0;

	for (tryi=0;tryi < 4;tryi++) {
		if ((blk=pe_header_vablock_insert(v,&v->hash[hashi],va & (~v->pagemask))) != NULL)
			return blk;

		pe_header_vamem_clear_lrused(v);
	}

	return NULL;
}

struct pe_header_vablock *pe_header_parser_vaload(struct pe_header_parser *hp,struct pe_header_vamem *v,EXE_PE_VA va,struct exe_pe_section_table_entry *ste) {
        /* assume "va" is already page aligned */
        struct pe_header_vablock *blk;
        EXE_PE_RVA rva,rvasec;
        EXE_PE_VA chksize;
        off_t fileofs;
        size_t todo;

        if (hp->src_fd < 0) return NULL;

        chksize = ste->VirtualSize;
        /* Open Watcom makes win32 binaries with .rsrc section VirtualSize == 0, Microsoft C++ does not do that */
        if (chksize == 0) chksize = ste->SizeOfRawData;

        rva = pe_header_parser_VAtoRVA(hp,va);
        if (rva < ste->VirtualAddress) return NULL;
        rvasec = rva - ste->VirtualAddress;
        if (rvasec >= chksize) return NULL;

        blk = pe_header_vamem_insert_and_free_lrused(v,va);
        if (blk == NULL) return NULL;

        if (rvasec < chksize && ste->PointerToRawData != 0) {
                fileofs = (off_t)(rvasec + ste->PointerToRawData);

                todo = chksize - rvasec;
                if (todo > hp->sectionalign) todo = hp->sectionalign;

                assert((v->pagemask + 1) == hp->sectionalign);
                assert(todo != 0);

                if (lseek(hp->src_fd,fileofs,SEEK_SET) == fileofs)
                        read(hp->src_fd,blk->MEM,todo);

        }

        return blk;
}

size_t pe_header_parser_varead(struct pe_header_parser *hp,EXE_PE_VA va,unsigned char *buf,size_t sz) {
	struct pe_header_vablock *blk;
	struct pe_header_vamem *v;
	size_t todo,pago;
	size_t rd = 0;

	if (hp == NULL || buf == NULL || sz == 0)
		return 0;
	if ((v=pe_header_parser_vamem(hp)) == NULL)
		return 0;

	while (sz > 0) {
		pago = (size_t)pe_header_vamem_pageofs(v,va);
		todo = (size_t)(v->pagemask) + (size_t)1u - pago;
		if (todo > sz) todo = sz;

		blk = pe_header_vamem_lookup(v,va & (~v->pagemask));
		if (blk == NULL) {
			struct exe_pe_section_table_entry *ste = pe_header_parser_section_table_lookupVA(hp,va & (~v->pagemask));
			if (!ste) break;

			blk = pe_header_parser_vaload(hp,v,va & (~v->pagemask),ste);
			if (blk == NULL) break;
		}

		assert(todo != 0);
		assert(todo <= sz);
		assert(blk != NULL);
		assert(blk->MEM != NULL);
		memcpy(buf,blk->MEM+pago,todo);
		buf += todo;
		va += todo;
		sz -= todo;
		rd += todo;
	}

	return rd;
}

size_t pe_header_parser_varead_lenwstring(struct pe_header_parser *hp,EXE_PE_VA va,unsigned char *buf,size_t sz) {
	uint16_t len = 0,cc;
	size_t rd = 0;

	/* uint16_t len
	 * WCHAR str[len] */
	pe_header_parser_varead(hp,va,(unsigned char*)(&len),2);
	va += 2;

	/* FIXME: What this code should really do is read and convert to UTF-8 */
	/* FIXME: What about UCS-16 surrogate pairs? */

	while (sz > 1) {
		if (len == 0)
			break;

		if (pe_header_parser_varead(hp,va,(unsigned char*)(&cc),2) != 2)
			break;
		va += 2;

		if (cc == 0)
			break;

		if (cc < 0x100)
			*buf = (char)cc;
		else
			*buf = '?';

		buf++;
		len--;
		sz--;
	}

	if (sz > 0) *buf = 0;
	return rd;
}

size_t pe_header_parser_varead_stringz(struct pe_header_parser *hp,EXE_PE_VA va,unsigned char *buf,size_t sz) {
	size_t rd = 0;

	while (sz > 1) {
		if (pe_header_parser_varead(hp,va,buf,1) != 1)
			break;

		if (*buf == 0)
			break;

		sz--;
		va++;
		buf++;
	}

	if (sz > 0) *buf = 0;
	return rd;
}

void pe_parser_init(struct pe_header_parser *hp) {
	memset(hp,0,sizeof(*hp));
	hp->src_fd = -1;
}

void pe_parser_uninit(struct pe_header_parser *hp) {
	if (hp->opthdr_raw.data) {
		free(hp->opthdr_raw.data);
		hp->opthdr_raw.alloc_size = 0;
		hp->opthdr_raw.data = NULL;
		hp->opthdr_raw.size = 0;
	}
	hp->datadir = NULL;
	if (hp->sections) {
		free(hp->sections);
		hp->sections = NULL;
	}
	if (hp->vmem) {
		pe_header_vamem_free(hp->vmem);
		hp->vmem = NULL;
	}
	memset(hp,0,sizeof(*hp));
}

void dump_import_table(struct pe_header_parser *pe_parser,struct exe_pe_opthdr_data_directory_entry *ddent) {
	EXE_PE_VA idt_read_va = (EXE_PE_VA)ddent->RVA + pe_parser->imagebase;
	struct pe_header_import_directory_table idt;
	char *Name = malloc(256+1);

	while (1) {
		if (pe_header_parser_varead(pe_parser,idt_read_va,(unsigned char*)(&idt),sizeof(idt)) != sizeof(idt))
			break;
		idt_read_va += sizeof(idt);

		if (idt.ImportLookupTableRVA == 0)
			break;

		Name[0] = 0;
		if (idt.NameRVA != 0)
			pe_header_parser_varead_stringz(pe_parser,(EXE_PE_VA)idt.NameRVA+pe_parser->imagebase,(unsigned char*)Name,256+1);

		printf("  == Import Directory Table Entry ==\n");
		printf("    ImportLookupTableRVA:           0x%08lx\n",
			(unsigned long)idt.ImportLookupTableRVA);
		printf("    TimeDateStamp:                  %lu\n",
			(unsigned long)idt.TimeDateStamp);
		printf("    ForwarderChain:                 0x%08lx\n",
			(unsigned long)idt.ForwarderChain);
		printf("    NameRVA:                        0x%08lx '%s'\n",
			(unsigned long)idt.NameRVA,Name?Name:"");
		printf("    ImportAddressTableRVA:          0x%08lx\n",
			(unsigned long)idt.ImportAddressTableRVA);

		if (idt.ImportAddressTableRVA != 0) {
			EXE_PE_VA iat_va = (EXE_PE_VA)idt.ImportAddressTableRVA + pe_parser->imagebase;

			printf("      == Import Lookup Table ==\n");
			if (pe_parser->opthdr_magic == EXE_PE_OPTHDR_MAGIC_PE) {
				uint32_t entry;

				while (pe_header_parser_varead(pe_parser,iat_va,(unsigned char*)(&entry),4) == 4) {
					if (entry == 0ul)
						break;

					/* NTS: Microsoft officially documents that bit 31/63 set means an ordinal.
					 *
					 *      Windows 95/98 system DLLs and even some of their user-space programs
					 *      violate this rule because the RVAs in the EXE/DLL files point directly
					 *      at the symbol rather than at an ASCII string to resolve at load time.
					 *      This is obvious when the RVA we just read points outside the image
					 *      that we are reading right now.
					 *
					 *      Obviously by having the import address tables written in this manner,
					 *      the EXE/DLL can load faster because there is no symbol lookup to slow
					 *      anything down.
					 *
					 *      It also explains why Windows 95/98 were often so insistent on restarting
					 *      the system every time you updated some part of the system. Because if
					 *      the DLLs directly point at each other's symbols like this, a change of
					 *      system components will potentially break things! */

					/* NTS: Another hint that imports are pre-resolved, though not as reliable as these
					 *      tests, is that the EXE/DLL PE header directory entry for the IAT (Import Address
					 *      Table) is zero, meaning there is no IAT. But some DLLs like AVIFIL32.DLL violate
					 *      this rule and they do have an IAT. */

					/* If the entry is nonzero and points within our own image, it's a normal Name RVA */
					if ((EXE_PE_VA)entry < pe_parser->imagesize) {
						EXE_PE_VA hintname_va = (EXE_PE_VA)entry+pe_parser->imagebase;
						uint16_t hint;

						pe_header_parser_varead(pe_parser,hintname_va,(unsigned char*)(&hint),2);

						Name[0] = 0;
						pe_header_parser_varead_stringz(pe_parser,hintname_va+(EXE_PE_VA)2ul,(unsigned char*)Name,256+1);

						printf("        Name '%s' (hint 0x%04xu)\n",Name,hint);
					}
					/* If bit 31 == 0 but too large to point within our own image, it's a pre-resolved symbol address (Windows 95) */
					else if (entry < 0x80000000ul) {
						printf("        Pre-resolved symbol address 0x%08lx\n",
							(unsigned long)entry);
					}
					/* If bit 31 == 1 and the ordinal value is less than 65536, it's an ordinal reference.
					 * 65536 is chosen because GetProcAddress() is documented to distinguish whether it's
					 * parameter is string or ordinal by whether the high-order word is zero (upper 16 bits),
					 * therefore despite the capabilities of the PE format, the highest valid ordinal is
					 * 0xFFFF (65535) and that is what this code uses to determine ordinal from Windows 95
					 * pre-resolved VAs. Windows 95 system DLLs like GDI32.DLL and KERNEL32.DLL all use
					 * image bases well above 0x80000000, usually 0xBFxxxxxx (right below kernel space at
					 * 0xC0000000), and these RVAs would otherwise be mistaken as ordinal references with
					 * very large ordinal numbers. */
					else if (entry < 0x80010000ul) {
						printf("        Ordinal %lu\n",
							(unsigned long)(entry & (uint32_t)0x7FFFFFFFul));
					}
					/* Anything else is a pre-resolved symbol address (Windows 95) */
					else {
						printf("        Pre-resolved symbol address 0x%08lx\n",
							(unsigned long)entry);
					}

					iat_va += 4;
				}
			}
			else if (pe_parser->opthdr_magic == EXE_PE_OPTHDR_MAGIC_PEPLUS) {
				uint64_t entry;

				while (pe_header_parser_varead(pe_parser,iat_va,(unsigned char*)(&entry),8) == 8) {
					if (entry == 0ull)
						break;

					if (entry & (uint64_t)0x8000000000000000ull) {
						printf("        Ordinal %llu\n",
							(unsigned long long)(entry & (uint64_t)0x7FFFFFFFFFFFFFFFul));
					}
					else {
						EXE_PE_VA hintname_va = (EXE_PE_VA)entry+pe_parser->imagebase;
						uint16_t hint;

						Name[0] = 0;
						pe_header_parser_varead(pe_parser,hintname_va,(unsigned char*)(&hint),2);
						pe_header_parser_varead_stringz(pe_parser,hintname_va+(EXE_PE_VA)2ul,(unsigned char*)Name,256+1);

						printf("        Name '%s' (hint 0x%04xu)\n",Name,hint);
					}

					iat_va += 8;
				}

			}
		}
	}

	free(Name);
}

void dump_export_table(struct pe_header_parser *pe_parser,struct exe_pe_opthdr_data_directory_entry *ddent) {
	struct pe_header_export_directory_table edt;
	EXE_PE_VA edt_min_va = (EXE_PE_VA)ddent->RVA + pe_parser->imagebase;
	EXE_PE_VA edt_max_va = (EXE_PE_VA)ddent->RVA + pe_parser->imagebase + (EXE_PE_VA)ddent->Size;
	EXE_PE_VA edt_read_va = edt_min_va;
	EXE_PE_VA exportaddr;
	EXE_PE_VA nameaddr;
	char *Name = NULL;
	uint16_t ordinal;
	unsigned int ei;
	unsigned int i;
	uint32_t u32;
	uint16_t u16;

	if (pe_header_parser_varead(pe_parser,edt_read_va,(unsigned char*)(&edt),sizeof(edt)) != sizeof(edt))
		return;

	Name = malloc(256+1);

	Name[0] = 0;
	if (edt.NameRVA != 0)
		pe_header_parser_varead_stringz(pe_parser,(EXE_PE_VA)edt.NameRVA+pe_parser->imagebase,(unsigned char*)Name,256+1);

	printf("  == Export Directory Table Entry ==\n");
	printf("    ExportFlags:                    0x%08lx\n",
		(unsigned long)edt.ExportFlags);
	printf("    TimeDateStamp:                  %lu\n",
		(unsigned long)edt.TimeDateStamp);
	printf("    Version:                        %u.%u\n",
		(unsigned int)edt.MajorVersion,
		(unsigned int)edt.MinorVersion);
	printf("    NameRVA:                        0x%08lx '%s'\n",
		(unsigned long)edt.NameRVA,Name);
	printf("    OrdinalBase:                    %lu\n",
		(unsigned long)edt.OrdinalBase);
	printf("    AddressTableEntries:            %lu\n",
		(unsigned long)edt.AddressTableEntries);
	printf("    NumberOfNamePointers:           %lu\n",
		(unsigned long)edt.NumberOfNamePointers);
	printf("    ExportAddressTableRVA:          0x%08lx\n",
		(unsigned long)edt.ExportAddressTableRVA);
	printf("    NamePointerRVA:                 0x%08lx\n",
		(unsigned long)edt.NamePointerRVA);
	printf("    OrdinalTableRVA:                0x%08lx\n",
		(unsigned long)edt.OrdinalTableRVA);

	printf("  == Exported symbols by name ==\n");
	for (i=0;i < edt.NumberOfNamePointers;i++) { // length of name pointer and ordinal table
		exportaddr = 0;
		nameaddr = 0;

		if (pe_header_parser_varead(pe_parser,((EXE_PE_VA)i*(EXE_PE_VA)4u) + (EXE_PE_VA)edt.NamePointerRVA + pe_parser->imagebase,(unsigned char*)(&u32),4) == 4) {
			if (u32 != 0)
				nameaddr = (EXE_PE_VA)u32 + pe_parser->imagebase;
		}

		/* ordinal is relative to OrdinalBase contrary to mistakes in Microsoft documentation */
		ordinal = 0xFFFF;
		pe_header_parser_varead(pe_parser,((EXE_PE_VA)i*(EXE_PE_VA)2u) + (EXE_PE_VA)edt.OrdinalTableRVA + pe_parser->imagebase,(unsigned char*)(&ordinal),2);
		if (ordinal >= (uint32_t)edt.AddressTableEntries) ordinal = 0xFFFF;

		if (ordinal != 0xFFFF) {
			if (pe_header_parser_varead(pe_parser,((EXE_PE_VA)ordinal*(EXE_PE_VA)4u) + (EXE_PE_VA)edt.ExportAddressTableRVA + pe_parser->imagebase,(unsigned char*)(&u32),4) == 4) {
				if (u32 != 0)
					exportaddr = (EXE_PE_VA)u32 + pe_parser->imagebase;
			}

			ordinal += edt.OrdinalBase;
		}

		if (ordinal == 0xFFFF)
			printf("    #(N/A):");
		else
			printf("    #%lu:",(unsigned long)ordinal);

		if (nameaddr != (EXE_PE_VA)0) {
			Name[0] = 0;
			pe_header_parser_varead_stringz(pe_parser,nameaddr,(unsigned char*)Name,256+1);

			printf(" '%s'",Name);
		}
		else {
			printf(" (no name)");
		}

		if (exportaddr != (EXE_PE_VA)0)
			printf(" 0x%llx",(unsigned long long)exportaddr);

		printf("\n");
	}

	printf("  == Exported symbols by ordinal ==\n");
	for (i=0;i < edt.AddressTableEntries;i++) { // length of name pointer and ordinal table
		ordinal = i + edt.OrdinalBase;
		exportaddr = 0;
		nameaddr = 0;

		if (pe_header_parser_varead(pe_parser,((EXE_PE_VA)i*(EXE_PE_VA)4u) + (EXE_PE_VA)edt.ExportAddressTableRVA + pe_parser->imagebase,(unsigned char*)(&u32),4) == 4) {
			if (u32 != 0)
				exportaddr = (EXE_PE_VA)u32 + pe_parser->imagebase;
		}

		for (ei=0;ei < edt.NumberOfNamePointers;ei++) {
			if (pe_header_parser_varead(pe_parser,((EXE_PE_VA)ei*(EXE_PE_VA)2u) + (EXE_PE_VA)edt.OrdinalTableRVA + pe_parser->imagebase,(unsigned char*)(&u16),2) == 2) {
				if (u16 == i)
					break;
			}
		}

		if (ei < edt.NumberOfNamePointers) {
			if (pe_header_parser_varead(pe_parser,((EXE_PE_VA)ei*(EXE_PE_VA)4u) + (EXE_PE_VA)edt.NamePointerRVA + pe_parser->imagebase,(unsigned char*)(&u32),4) == 4) {
				if (u32 != 0)
					nameaddr = (EXE_PE_VA)u32 + pe_parser->imagebase;
			}
		}

		printf("    #%lu:",(unsigned long)ordinal);

		if (nameaddr != (EXE_PE_VA)0) {
			Name[0] = 0;
			pe_header_parser_varead_stringz(pe_parser,nameaddr,(unsigned char*)Name,256+1);

			printf(" '%s'",Name);
		}
		else {
			printf(" (no name)");
		}

		if (exportaddr != (EXE_PE_VA)0)
			printf(" 0x%llx",(unsigned long long)exportaddr);

		printf("\n");
	}

	free(Name);
}

static void drtlspc(unsigned int l) {
	while (l > 0) {
		printf("    ");
		l--;
	}
}

void dump_resource_table_level(struct pe_header_parser *pe_parser,struct exe_pe_opthdr_data_directory_entry *ddent,EXE_PE_VA read_rva,unsigned int level) {
	/* Not documented by Microsoft: "RVA" in this context really means relative to the resource section, NOT relative to image base.
	 * Pffft, oh Microsoft! Forgetting minor details like that as usual, of course. */
	EXE_PE_VA rsrcbase = (EXE_PE_VA)(ddent->RVA) + pe_parser->imagebase;
	struct pe_header_resource_directory_table rdt;
	struct pe_header_resource_directory_entry rde;
	EXE_PE_VA read_va = rsrcbase + read_rva;
	unsigned long i;
	char *Name;

	assert(sizeof(rde) == 0x08);

	if (pe_header_parser_varead(pe_parser,read_va,(unsigned char*)(&rdt),sizeof(rdt)) != sizeof(rdt))
		return;
	read_va += sizeof(rdt);

	drtlspc(level); printf("  == Resource Directory Table Entry ==\n");
	drtlspc(level); printf("    Characteristics:                0x%08lx\n",
		(unsigned long)rdt.Characteristics);
	drtlspc(level); printf("    TimeDateStamp:                  %lu\n",
		(unsigned long)rdt.TimeDateStamp);
	drtlspc(level); printf("    Version:                        %u.%u\n",
		rdt.MajorVersion,
		rdt.MinorVersion);
	drtlspc(level); printf("    NumberOfNameEntries:            0x%08lx\n",
		(unsigned long)rdt.NumberOfNameEntries);
	drtlspc(level); printf("    NumberOfIDEntries:              0x%08lx\n",
		(unsigned long)rdt.NumberOfIDEntries);

	for (i=0;i < (unsigned long)rdt.NumberOfNameEntries+(unsigned long)rdt.NumberOfIDEntries;i++) {
		if (pe_header_parser_varead(pe_parser,read_va,(unsigned char*)(&rde),sizeof(rde)) != sizeof(rde))
			return;
		read_va += sizeof(rde);

		/* Oh, typical Microsoft. Documenting the resource directory entry name/ID thing but completely neglecting
		 * to document that all name RVAs have bit 31 set. */

		drtlspc(level); printf("    Resource %s at RVA 0x%08lx",
			(rde.s.DataEntryRVA & 0x80000000ul)?"directory":"leaf",
			(unsigned long)(rde.s.DataEntryRVA & 0x7FFFFFFFu));

		if (rde.f.NameRVA & 0x80000000ul) {
			Name = malloc(256+1);

			Name[0] = 0;
			if (rde.f.NameRVA != 0)
				pe_header_parser_varead_lenwstring(pe_parser,(EXE_PE_VA)(rde.f.NameRVA&0x7FFFFFFFul)+rsrcbase,(unsigned char*)Name,256+1);

			printf(" '%s'",Name);
			free(Name);
		}
		else {
			printf(" ordinal %lu",(unsigned long)rde.f.IntegerID);
		}

		printf("\n");

		if (rde.s.DataEntryRVA & 0x80000000ul) {
			dump_resource_table_level(pe_parser,ddent,rde.s.DataEntryRVA&0x7FFFFFFFu,level+1u);
		}
	}
}

void dump_resource_table(struct pe_header_parser *pe_parser,struct exe_pe_opthdr_data_directory_entry *ddent) {
	dump_resource_table_level(pe_parser,ddent,0,0);
}

static unsigned char            opt_sort_ordinal = 0;
static unsigned char            opt_sort_names = 0;
static unsigned char            opt_dumpsect = 0;

static char*                    src_file = NULL;
static int                      src_fd = -1;

static struct exe_dos_header    exehdr;
static struct exe_dos_layout    exelayout;

static void help(void) {
    fprintf(stderr,"EXENEDMP -i <exe file>\n");
    fprintf(stderr," -sn        Sort names\n");
    fprintf(stderr," -so        Sort by ordinal\n");
    fprintf(stderr," -dumpsect  Dump sections\n");
}

int main(int argc,char **argv) {
    struct pe_header_parser pe_parser;
    uint32_t pe_header_offset;
    uint32_t file_size;
    char *a;
    int i;

    pe_parser_init(&pe_parser);
    memset(&exehdr,0,sizeof(exehdr));

    for (i=1;i < argc;) {
        a = argv[i++];

        if (*a == '-') {
            do { a++; } while (*a == '-');

            if (!strcmp(a,"h") || !strcmp(a,"help")) {
                help();
                return 1;
            }
            else if (!strcmp(a,"dumpsect")) {
                opt_dumpsect = 1;
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
        read(src_fd,&pe_parser.pe_header,sizeof(pe_parser.pe_header)) != sizeof(pe_parser.pe_header)) {
        fprintf(stderr,"Cannot read PE header\n");
        return 1;
    }
    if (pe_parser.pe_header.signature != EXE_PE_SIGNATURE) {
        fprintf(stderr,"Not an PE executable\n");
        return 1;
    }

    pe_parser.src_fd = src_fd;
    pe_parser.pe_header_offset = pe_header_offset;
    pe_parser.opthdr_offset = pe_parser.pe_header_offset + sizeof(pe_parser.pe_header);
    pe_parser.sections_offset = pe_parser.pe_header_offset + sizeof(pe_parser.pe_header) + pe_parser.pe_header.fileheader.SizeOfOptionalHeader;
    printf("* PE header at %lu\n",(unsigned long)pe_header_offset);
    printf("    Machine:                        0x%04x (%s)\n",
        pe_parser.pe_header.fileheader.Machine,
        exe_pe_fileheader_machine_to_str(pe_parser.pe_header.fileheader.Machine));
    printf("    NumberOfSections:               %u\n",
        pe_parser.pe_header.fileheader.NumberOfSections);
    printf("    TimeDateStamp:                  %lu (0x%08lx)\n",
        (unsigned long)pe_parser.pe_header.fileheader.TimeDateStamp,
        (unsigned long)pe_parser.pe_header.fileheader.TimeDateStamp);
    printf("    PointerToSymbolTable:           %lu (0x%08lx) (file offset)\n",
        (unsigned long)pe_parser.pe_header.fileheader.PointerToSymbolTable,
        (unsigned long)pe_parser.pe_header.fileheader.PointerToSymbolTable);
    printf("    NumberOfSymbols:                %lu\n",
        (unsigned long)pe_parser.pe_header.fileheader.NumberOfSymbols);
    printf("    SizeOfOptionalHeader:           %lu\n",
        (unsigned long)pe_parser.pe_header.fileheader.SizeOfOptionalHeader);
    printf("    Characteristics:                0x%04x\n",
        pe_parser.pe_header.fileheader.Characteristics);
    if (pe_parser.pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_RELOCS_STRIPPED)
        printf("      - Relocations stripped\n");
    if (pe_parser.pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_EXECUTABLE_IMAGE)
        printf("      - Image is valid and executable\n");
    if (pe_parser.pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_LINE_NUMS_STRIPPED)
        printf("      - Line numbers have been removed\n");
    if (pe_parser.pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_LOCAL_SYMS_STRIPPED)
        printf("      - Local symbols have been removed\n");
    if (pe_parser.pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_AGGRESSIVE_WS_TRIM)
        printf("      - Aggressively trim working set\n");
    if (pe_parser.pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_LARGE_ADDRESS_AWARE)
        printf("      - Large address aware\n");
    if (pe_parser.pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_16BIT_MACHINE)
        printf("      - 16-bit machine / Reserved\n");
    if (pe_parser.pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_BYTES_REVERSED_LO)
        printf("      - Little endian (LSB before MSB)\n");
    if (pe_parser.pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_32BIT_MACHINE)
        printf("      - 32-bit machine\n");
    if (pe_parser.pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_DEBUG_STRIPPED)
        printf("      - Debugging information stripped from image file\n");
    if (pe_parser.pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP)
        printf("      - Copy and run from swap if run from removable media\n");
    if (pe_parser.pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_NET_RUN_FROM_SWAP)
        printf("      - Copy and run from swap if run from network media\n");
    if (pe_parser.pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_SYSTEM)
        printf("      - Image is a system file, not user program\n");
    if (pe_parser.pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_DLL)
        printf("      - Image is a DLL library\n");
    if (pe_parser.pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_UP_SYSTEM_ONLY)
        printf("      - Run only on a UP machine\n");
    if (pe_parser.pe_header.fileheader.Characteristics & EXE_PE_HEADER_IMAGE_FILE_BYTES_REVERSED_HI)
        printf("      - Big endian (MSB before LSB)\n");

    printf("* Optional header at %lu\n",(unsigned long)pe_parser.opthdr_offset);
    printf("* Section table at %lu\n",(unsigned long)pe_parser.sections_offset);

    /* read the optional header */
    if (pe_parser.pe_header.fileheader.SizeOfOptionalHeader >= 2 && pe_parser.pe_header.fileheader.SizeOfOptionalHeader <= 0x4000) {
        if (lseek(src_fd,pe_parser.opthdr_offset,SEEK_SET) != pe_parser.opthdr_offset) {
            printf("! Failed to seek to opthdr\n");
            return 1;
        }

        pe_parser.opthdr_raw.size = pe_parser.pe_header.fileheader.SizeOfOptionalHeader;
        pe_parser.opthdr_raw.alloc_size = 512;
        if (pe_parser.opthdr_raw.alloc_size < pe_parser.opthdr_raw.size)
                pe_parser.opthdr_raw.alloc_size = pe_parser.opthdr_raw.size;
        pe_parser.opthdr_raw.data = malloc(pe_parser.opthdr_raw.alloc_size);
        if (!pe_parser.opthdr_raw.data) {
            printf("! Failed to allocate memory for optional header\n");
            return 1;
        }
        memset(pe_parser.opthdr_raw.data,0,pe_parser.opthdr_raw.alloc_size);
        if ((size_t)read(src_fd,pe_parser.opthdr_raw.data,pe_parser.opthdr_raw.size) != pe_parser.opthdr_raw.size) {
            printf("! Failed to read optional header\n");
            return 1;
        }

        pe_parser.opthdr_magic = exe_pe_opthdr_magic_value(&pe_parser.opthdr_raw);

        /* Windows 3.1 Win32s core DLLs have a magic value of zero */
        if (pe_parser.opthdr_magic == 0)
            pe_parser.opthdr_magic = EXE_PE_OPTHDR_MAGIC_PE;

        if (pe_parser.opthdr_magic == EXE_PE_OPTHDR_MAGIC_PE) {
            if (pe_parser.opthdr_raw.size >= sizeof(struct exe_pe_opthdr_pe)) {
                struct exe_pe_opthdr_pe *hdr = (struct exe_pe_opthdr_pe*)(pe_parser.opthdr_raw.data);
                assert(pe_parser.opthdr_raw.alloc_size >= sizeof(*hdr));

                pe_parser.datadir = (struct exe_pe_opthdr_data_directory_entry*)(pe_parser.opthdr_raw.data+sizeof(struct exe_pe_opthdr_pe));
                pe_parser.datadir_count = (pe_parser.opthdr_raw.size-sizeof(struct exe_pe_opthdr_pe))/sizeof(struct exe_pe_opthdr_data_directory_entry);
                if (pe_parser.datadir_count > hdr->windows.NumberOfRvaAndSizes)
                    pe_parser.datadir_count = hdr->windows.NumberOfRvaAndSizes;
            }
        }
        else if (pe_parser.opthdr_magic == EXE_PE_OPTHDR_MAGIC_PEPLUS) {
            if (pe_parser.opthdr_raw.size >= sizeof(struct exe_pe_opthdr_peplus)) {
                struct exe_pe_opthdr_peplus *hdr = (struct exe_pe_opthdr_peplus*)(pe_parser.opthdr_raw.data);
                assert(pe_parser.opthdr_raw.alloc_size >= sizeof(*hdr));

                pe_parser.datadir = (struct exe_pe_opthdr_data_directory_entry*)(pe_parser.opthdr_raw.data+sizeof(struct exe_pe_opthdr_peplus));
                pe_parser.datadir_count = (pe_parser.opthdr_raw.size-sizeof(struct exe_pe_opthdr_peplus))/sizeof(struct exe_pe_opthdr_data_directory_entry);
                if (pe_parser.datadir_count > hdr->windows.NumberOfRvaAndSizes)
                    pe_parser.datadir_count = hdr->windows.NumberOfRvaAndSizes;
            }
        }

        printf("    Optional header magic:          0x%04x (%s)\n",
            pe_parser.opthdr_magic,exe_ne_opthdr_magic_to_str(pe_parser.opthdr_magic));

	printf(" == Optional header, standard section ==\n");

#define HS(x) hdr->standard.x
#define WS(x) hdr->windows.x
#define EXIST(w) ( ( (size_t)((char*)(&(w))) + sizeof(w) - (size_t)((char*)(hdr)) ) <= pe_parser.opthdr_raw.size )
        if (pe_parser.opthdr_magic == EXE_PE_OPTHDR_MAGIC_PE) {
            struct exe_pe_opthdr_pe *hdr = (struct exe_pe_opthdr_pe*)(pe_parser.opthdr_raw.data);
            assert(pe_parser.opthdr_raw.alloc_size >= sizeof(*hdr));

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
	else if (pe_parser.opthdr_magic == EXE_PE_OPTHDR_MAGIC_PEPLUS) {
            struct exe_pe_opthdr_peplus *hdr = (struct exe_pe_opthdr_peplus*)(pe_parser.opthdr_raw.data);
            assert(pe_parser.opthdr_raw.alloc_size >= sizeof(*hdr));

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
	}

        printf(" == Optional header, windows section ==\n");

        if (pe_parser.opthdr_magic == EXE_PE_OPTHDR_MAGIC_PE) {
            struct exe_pe_opthdr_pe *hdr = (struct exe_pe_opthdr_pe*)(pe_parser.opthdr_raw.data);
            assert(pe_parser.opthdr_raw.alloc_size >= sizeof(*hdr));

            if (EXIST(WS(ImageBase))) {
                pe_parser.imagebase = WS(ImageBase);
                printf("    ImageBase:                      0x%08lx (VA)\n",
                    (unsigned long)WS(ImageBase));
            }
            if (EXIST(WS(SectionAlignment))) {
                pe_parser.sectionalign = WS(SectionAlignment);
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
                pe_parser.imagesize = WS(SizeOfImage);
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
                if (WS(DLLCharacteristics) & EXE_PE_OPTHDR_DLLCHARACT_DYNAMIC_BASE)
                    printf("        - DLL can be relocated at load time\n");
                if (WS(DLLCharacteristics) & EXE_PE_OPTHDR_DLLCHARACT_FORCE_INTEGRITY)
                    printf("        - Enforce code integrity checks\n");
                if (WS(DLLCharacteristics) & EXE_PE_OPTHDR_DLLCHARACT_NX_COMPAT)
                    printf("        - NX compatible\n");
                if (WS(DLLCharacteristics) & EXE_PE_OPTHDR_DLLCHARACT_NO_ISOLATION)
                    printf("        - Isolation aware, but do not isolate the image\n");
                if (WS(DLLCharacteristics) & EXE_PE_OPTHDR_DLLCHARACT_NO_SEH)
                    printf("        - Does not use Structured Exception Handling\n");
                if (WS(DLLCharacteristics) & EXE_PE_OPTHDR_DLLCHARACT_NO_BIND)
                    printf("        - Do not bind image\n");
                if (WS(DLLCharacteristics) & EXE_PE_OPTHDR_DLLCHARACT_WDM_DRIVER)
                    printf("        - WDM driver\n");
                if (WS(DLLCharacteristics) & EXE_PE_OPTHDR_DLLCHARACT_TERMINAL_SERVER_AWARE)
                    printf("        - Terminal Server aware\n");
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
        else if (pe_parser.opthdr_magic == EXE_PE_OPTHDR_MAGIC_PEPLUS) {
            struct exe_pe_opthdr_peplus *hdr = (struct exe_pe_opthdr_peplus*)(pe_parser.opthdr_raw.data);
            assert(pe_parser.opthdr_raw.alloc_size >= sizeof(*hdr));

            if (EXIST(WS(ImageBase))) {
                pe_parser.imagebase = WS(ImageBase);
                printf("    ImageBase:                      0x%016llx (VA)\n",
                    (unsigned long long)WS(ImageBase));
            }
            if (EXIST(WS(SectionAlignment))) {
                pe_parser.sectionalign = WS(SectionAlignment);
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
                pe_parser.imagesize = WS(SizeOfImage);
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
                if (WS(DLLCharacteristics) & EXE_PE_OPTHDR_DLLCHARACT_DYNAMIC_BASE)
                    printf("        - DLL can be relocated at load time\n");
                if (WS(DLLCharacteristics) & EXE_PE_OPTHDR_DLLCHARACT_FORCE_INTEGRITY)
                    printf("        - Enforce code integrity checks\n");
                if (WS(DLLCharacteristics) & EXE_PE_OPTHDR_DLLCHARACT_NX_COMPAT)
                    printf("        - NX compatible\n");
                if (WS(DLLCharacteristics) & EXE_PE_OPTHDR_DLLCHARACT_NO_ISOLATION)
                    printf("        - Isolation aware, but do not isolate the image\n");
                if (WS(DLLCharacteristics) & EXE_PE_OPTHDR_DLLCHARACT_NO_SEH)
                    printf("        - Does not use Structured Exception Handling\n");
                if (WS(DLLCharacteristics) & EXE_PE_OPTHDR_DLLCHARACT_NO_BIND)
                    printf("        - Do not bind image\n");
                if (WS(DLLCharacteristics) & EXE_PE_OPTHDR_DLLCHARACT_WDM_DRIVER)
                    printf("        - WDM driver\n");
                if (WS(DLLCharacteristics) & EXE_PE_OPTHDR_DLLCHARACT_TERMINAL_SERVER_AWARE)
                    printf("        - Terminal Server aware\n");
            }
            if (EXIST(WS(SizeOfStackReserve))) {
                printf("    SizeOfStackReserve:             0x%016llx\n",
                    (unsigned long long)WS(SizeOfStackReserve));
            }
            if (EXIST(WS(SizeOfStackCommit))) {
                printf("    SizeOfStackCommit:              0x%016llx\n",
                    (unsigned long long)WS(SizeOfStackCommit));
            }
            if (EXIST(WS(SizeOfHeapReserve))) {
                printf("    SizeOfHeapReserve:              0x%016llx\n",
                    (unsigned long long)WS(SizeOfHeapReserve));
            }
            if (EXIST(WS(SizeOfHeapCommit))) {
                printf("    SizeOfHeapCommit:               0x%016llx\n",
                    (unsigned long long)WS(SizeOfHeapCommit));
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

        if (pe_parser.datadir != NULL) {
            struct exe_pe_opthdr_data_directory_entry *ent;
            unsigned int i=0;

            printf(" == Optional header, data directory ==\n");
            for (i=0;i < pe_parser.datadir_count;i++) {
                printf("    Data directory #%u (%s):\n",i,exe_pe_opthdr_data_directory_entry_to_str(i));

                ent = &pe_parser.datadir[i];
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
            }
        }
    }
    if (pe_parser.pe_header.fileheader.NumberOfSections > 0 && pe_parser.pe_header.fileheader.NumberOfSections <= 4096) {
        if (lseek(src_fd,pe_parser.sections_offset,SEEK_SET) != pe_parser.sections_offset) {
            printf("! Failed to seek to section table\n");
            return 1;
        }

        pe_parser.sections_count = pe_parser.pe_header.fileheader.NumberOfSections;
        pe_parser.sections = malloc(sizeof(struct exe_pe_section_table_entry) * pe_parser.sections_count);
        if (!pe_parser.sections) {
            printf("! Cannot allocate memory for section table\n");
            return 1;
        }

        if ((size_t)read(src_fd,pe_parser.sections,pe_parser.sections_count * sizeof(struct exe_pe_section_table_entry)) !=
            (pe_parser.sections_count * sizeof(struct exe_pe_section_table_entry))) {
            printf("! Cannot read section table\n");
            return 1;
        }
    }

    if (pe_parser.sections != NULL && pe_parser.sections_count != 0) {
        struct exe_pe_section_table_entry *ent;
        unsigned int i;
        EXE_PE_VA va;

        pe_parser.imagesize = 0;
        for (i=0;i < pe_parser.sections_count;i++) {
            ent = pe_parser.sections + i;
            va = (EXE_PE_VA)(ent->VirtualAddress) + (EXE_PE_VA)(ent->VirtualSize);
            if (pe_parser.imagesize < va) pe_parser.imagesize = va;
        }
    }

    if (pe_parser.sections != NULL && pe_parser.sections_count != 0) {
        struct exe_pe_section_table_entry *ent;
        unsigned int i;
        char tmp[9];

        printf(" == Section table ==\n"); 
        for (i=0;i < pe_parser.sections_count;i++) {
            ent = pe_parser.sections + i;

            memcpy(tmp,ent->Name,8); tmp[8] = 0;
            printf("    Section #%u '%s':\n",i,tmp);
            printf("        VirtualSize:                0x%08lx\n",
                (unsigned long)(ent->VirtualSize));
            printf("        VirtualAddress:             0x%08lx\n",
                (unsigned long)(ent->VirtualAddress));
            printf("        SizeOfRawData:              0x%08lx\n",
                (unsigned long)(ent->SizeOfRawData));
            printf("        PointerToRawData:           0x%08lx\n",
                (unsigned long)(ent->PointerToRawData));
            printf("        PointerToRelocations:       0x%08lx\n",
                (unsigned long)(ent->PointerToRelocations));
            printf("        PointerToLinenumbers:       0x%08lx\n",
                (unsigned long)(ent->PointerToLinenumbers));
            printf("        NumberOfRelocations:        0x%08lx\n",
                (unsigned long)(ent->NumberOfRelocations));
            printf("        NumberOfLinenumbers:        0x%08lx\n",
                (unsigned long)(ent->NumberOfLinenumbers));
            printf("        Characteristics:            0x%08lx\n",
                (unsigned long)(ent->Characteristics));
            if (ent->Characteristics & EXE_PE_SECTFLAGS_TYPE_DSECT)
                printf("        - DSECT\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_TYPE_NOLOAD)
                printf("        - NOLOAD\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_TYPE_GROUP)
                printf("        - GROUP\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_TYPE_NO_PAD)
                printf("        - Do not pad section to next boundary\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_TYPE_COPY)
                printf("        - COPY\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_CNT_CODE)
                printf("        - Contains code\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_CNT_INITIALIZED_DATA)
                printf("        - Initialized data\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_CNT_UNINITIALIZED_DATA)
                printf("        - Uninitialized data\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_LNK_OTHER)
                printf("        - OTHER\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_LNK_INFO)
                printf("        - Section contains comments or other info\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_TYPE_OVER)
                printf("        - TYPE_OVER\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_LNK_REMOVE)
                printf("        - Section will not become part of the image\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_LNK_COMDAT)
                printf("        - Section contains COMDAT data\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_MEM_FARDATA)
                printf("        - MEM_FARDATA\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_MEM_PURGEABLE)
                printf("        - MEM_PURGEABLE\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_MEM_16BIT)
                printf("        - MEM_16BIT\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_MEM_LOCKED)
                printf("        - MEM_LOCKED\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_MEM_PRELOAD)
                printf("        - MEM_PRELOAD\n");
            {
                unsigned int shf = (ent->Characteristics & EXE_PE_SECTFLAGS_SCN_ALIGN_MASK) >> EXE_PE_SECTFLAGS_SCN_ALIGN_MASKSHIFT;
                if (shf != 0) {
                    printf("        - Align data to %u-byte boundary\n",1u << (shf-1));
                }
            }
            if (ent->Characteristics & EXE_PE_SECTFLAGS_LNK_NRELOC_OVFL)
                printf("        - Section contains extended relocations\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_MEM_DISCARDABLE)
                printf("        - Section can be discarded as needed\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_MEM_NOT_CACHED)
                printf("        - Section cannot be cached\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_MEM_NOT_PAGED)
                printf("        - Section is not pageable\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_MEM_SHARED)
                printf("        - Section can be shared in memory\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_MEM_EXECUTE)
                printf("        - Section can be executed as code\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_MEM_READ)
                printf("        - Section can be read\n");
            if (ent->Characteristics & EXE_PE_SECTFLAGS_MEM_WRITE)
                printf("        - Section can be written to\n");
        }
    }

    if (pe_parser.datadir != NULL && EXE_PE_DATADIRENT_IMPORT_TABLE < pe_parser.datadir_count) {
        struct exe_pe_opthdr_data_directory_entry *ddent = &pe_parser.datadir[EXE_PE_DATADIRENT_IMPORT_TABLE];
        if (ddent->Size != 0 && ddent->RVA != 0) {
            printf("== Import Lookup Table ==\n");
            dump_import_table(&pe_parser,ddent);
        }
    }

    if (pe_parser.datadir != NULL && EXE_PE_DATADIRENT_EXPORT_TABLE < pe_parser.datadir_count) {
        struct exe_pe_opthdr_data_directory_entry *ddent = &pe_parser.datadir[EXE_PE_DATADIRENT_EXPORT_TABLE];
        if (ddent->Size != 0 && ddent->RVA != 0) {
            printf("== Export Lookup Table ==\n");
            dump_export_table(&pe_parser,ddent);
        }
    }

    if (pe_parser.datadir != NULL && EXE_PE_DATADIRENT_RESOURCE_TABLE < pe_parser.datadir_count) {
        struct exe_pe_opthdr_data_directory_entry *ddent = &pe_parser.datadir[EXE_PE_DATADIRENT_RESOURCE_TABLE];
        if (ddent->Size != 0 && ddent->RVA != 0) {
            printf("== Resources ==\n");
            dump_resource_table(&pe_parser,ddent);
        }
    }

    if (opt_dumpsect && pe_parser.sections != NULL && pe_parser.sections_count != 0) {
        struct exe_pe_section_table_entry* ent;
        unsigned char tmp[256];
        EXE_PE_VA minv,maxv,t;
        EXE_PE_VA vo;
        size_t rd,i;
        size_t si=0;

        ent = pe_parser.sections+(si++);
        minv = t = (EXE_PE_VA)ent->VirtualAddress + pe_parser.imagebase;
        if (ent->VirtualSize != 0) t += (EXE_PE_VA)ent->VirtualSize;
        else t += (EXE_PE_VA)ent->SizeOfRawData;
        maxv = t;

        while (si < pe_parser.sections_count) {
            ent = pe_parser.sections+(si++);
            t = (EXE_PE_VA)ent->VirtualAddress + pe_parser.imagebase;
            if (minv > t) minv = t;
            if (ent->VirtualSize != 0) t += (EXE_PE_VA)ent->VirtualSize;
            else t += (EXE_PE_VA)ent->SizeOfRawData;
            if (maxv < t) maxv = t;
        }

        printf("== Section dump 0x%llx-0x%llx ==\n",
            (unsigned long long)minv,
            (unsigned long long)maxv-1ull);

        for (vo=minv;vo < maxv;vo += 256) {
            rd = pe_header_parser_varead(&pe_parser,vo,tmp,sizeof(tmp));
            if (rd != 0) {
                printf("== VA 0x%llx-0x%llx ==\n",(unsigned long long)vo,(unsigned long long)vo+(unsigned long long)rd-1ull);
                for (i=0;i < rd;i++) {
                    if ((i&15) == 0) printf("  ");
                    printf(" %02x",tmp[i]);
                    if ((i&15) == 15) printf("\n");
                }
                if ((rd&15) == 15) printf("\n");
            }
        }
    }

    pe_parser_uninit(&pe_parser);
    close(src_fd);
    return 0;
}
