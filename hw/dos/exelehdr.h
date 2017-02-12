
#define EXE_HEADER_LE_HEADER_SIZE                                   (0xAC)
#define EXE_HEADER_LE_HEADER_SIZE_WINDOWS_VXD                       (0xC4)
#define EXE_LE_SIGNATURE                                            (0x454C)
#define EXE_LX_SIGNATURE                                            (0x584C)

#define LE_HEADER_MODULE_TYPE_FLAGS_PER_PROCESS_DLL_INIT            (1UL << 2UL)
#define LE_HEADER_MODULE_TYPE_FLAGS_NO_INTERNAL_FIXUP               (1UL << 4UL)
#define LE_HEADER_MODULE_TYPE_FLAGS_NO_EXTERNAL_FIXUP               (1UL << 5UL)

#define LE_HEADER_MODULE_TYPE_FLAGS_PM_WINDOWING_MASK               (3UL << 8UL)
#define LE_HEADER_MODULE_TYPE_FLAGS_PM_WINDOWING_UNKNOWN            (0UL << 8UL)
#define LE_HEADER_MODULE_TYPE_FLAGS_PM_WINDOWING_INCOMPATIBLE       (1UL << 8UL)
#define LE_HEADER_MODULE_TYPE_FLAGS_PM_WINDOWING_COMPATIBLE         (2UL << 8UL)
#define LE_HEADER_MODULE_TYPE_FLAGS_PM_WINDOWING_USES_API           (3UL << 8UL)

#define LE_HEADER_MODULE_TYPE_FLAGS_MODULE_NOT_LOADABLE             (1UL << 13UL)
#define LE_HEADER_MODULE_TYPE_FLAGS_IS_DLL                          (1UL << 15UL)

#define LE_HEADER_MODULE_TYPE_FLAGS_PER_PROCESS_LIBRARY_TERMINATION (1UL << 31UL)

#define LE_HEADER_OBJECT_TABLE_ENTRY_FLAGS_READABLE                 (1UL << 0UL)
#define LE_HEADER_OBJECT_TABLE_ENTRY_FLAGS_WRITEABLE                (1UL << 1UL)
#define LE_HEADER_OBJECT_TABLE_ENTRY_FLAGS_EXECUTABLE               (1UL << 2UL)
#define LE_HEADER_OBJECT_TABLE_ENTRY_FLAGS_RESOURCE                 (1UL << 3UL)
#define LE_HEADER_OBJECT_TABLE_ENTRY_FLAGS_DISCARDABLE              (1UL << 4UL)
#define LE_HEADER_OBJECT_TABLE_ENTRY_FLAGS_SHARED                   (1UL << 5UL)
#define LE_HEADER_OBJECT_TABLE_ENTRY_FLAGS_PRELOAD                  (1UL << 6UL)
#define LE_HEADER_OBJECT_TABLE_ENTRY_FLAGS_INVALID                  (1UL << 7UL)
#define LE_HEADER_OBJECT_TABLE_ENTRY_FLAGS_ZEROFILLED               (1UL << 8UL)
#define LE_HEADER_OBJECT_TABLE_ENTRY_FLAGS_RESIDENT                 (1UL << 9UL) /* valid for VDD, PDD, etc */
#define LE_HEADER_OBJECT_TABLE_ENTRY_FLAGS_RESIDENT_LONG_LOCKABLE   (1UL << 10UL)
#define LE_HEADER_OBJECT_TABLE_ENTRY_FLAGS_1616ALIAS                (1UL << 12UL)
#define LE_HEADER_OBJECT_TABLE_ENTRY_FLAGS_386_BIG_DEFAULT          (1UL << 13UL)
#define LE_HEADER_OBJECT_TABLE_ENTRY_FLAGS_CONFORMING               (1UL << 14UL)
#define LE_HEADER_OBJECT_TABLE_ENTRY_FLAGS_IO_PRIVILEGE             (1UL << 15UL)

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
    uint32_t        bytes_on_last_page;             // +0x2C Like MS-DOS header, the last page is this long   NTS: LX format defines as PAGE_SHIFT
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
};                                                  // =0xAC FIXME: If there are additional fields defined by the standard here, let me know!
#pragma pack(pop)

#pragma pack(push,1)
struct exe_le_header_windows_vxd {                  // guess what: Microsoft 386/VXD drivers have additional fields, required by Windows
    struct exe_le_header s;                         // +0x00 LE header standard fields
    uint32_t        _unknown_AC;                    // +0xAC ??? (often zero)
    uint32_t        _unknown_B0;                    // +0xB0 ??? (often zero)
    uint32_t        _unknown_B4;                    // +0xB4 ??? (often zero)
    uint32_t        _unknown_B8;                    // +0xB8 ??? (often zero)
    uint32_t        _unknown_BC;                    // +0xBC ??? (often zero)
    uint16_t        DDB_Req_Device_Number;          // +0xC0 Copy of VXD DDB block DDB_Req_Device_Number field
    uint16_t        DDB_SDK_Version;                // +0xC2 copy of VXD DDB block DDB_SDK_Version field
};                                                  // =0xC4
#pragma pack(pop)
/* ^ Note that Microsoft's linker, even if told the output will be a "DEV386" file, leaves the extended fields set to zero.
 *   You then run an additional tool in the DDK called "ADDHDR.EXE" which reads the DDB block and copies the two DDB fields
 *   listed above into the extended part of the LE header. Windows will not load your VXD/386 file without these additional
 *   fields. */

/* maximum bytes you'll need for all variations of the LE header */
#define exe_le_header_MAX                           (0xC4)

#pragma pack(push,1)
struct exe_le_header_object_table_entry {
    uint32_t        virtual_segment_size;           // +0x00 in bytes
    uint32_t        relocation_base_address;        // +0x04 base of relocation, address this object will be loaded to
    uint32_t        object_flags;                   // +0x08 object flags
    uint32_t        page_map_index;                 // +0x0C index of first object page  table
    uint32_t        page_map_entries;               // +0x10 number of object page table entries
    uint32_t        _reserved;                      // +0x14
};                                                  // =0x18
#pragma pack(pop)

#pragma pack(push,1)
// NTS: I can find the LX definition of this structure, but LE structures remain undocumented.
//      I had to reverse engineer this from what I observe in an EXE file. The data in LE files
//      makes no sense unless interpreted this way.
struct exe_le_header_object_page_table_entry {
    uint16_t        flags;                          // +0x00 flags
    uint16_t        page_data_offset;               // +0x02 offset from preload page << PAGE_SHIFT, 1-based (so subtract by 1 before shift)
};                                                  // =0x04
#pragma pack(pop)

#pragma pack(push,1)
// NTS: IBM confusingly describes each entry as if one 64-bit word, with bit field markers.
//      If you actually followed it, you'd think the page data offset were in the upper 32 bits,
//      and you'd read the fields backwards.
struct exe_lx_header_object_page_table_entry {
    uint32_t        page_data_offset;               // +0x00 offset from preload page << PAGE_SHIFT, 1-based (so subtract by 1 before shift)
    uint16_t        data_size;                      // +0x04 data size
    uint16_t        flags;                          // +0x06 flags
};                                                  // =0x08
#pragma pack(pop)

#pragma pack(push,1)
// Windows VXD DDB structure (Windows 3.1)
struct windows_vxd_ddb_win31 {
    uint32_t                        DDB_Next;               // +0x00 next pointer in memory (0 in file)
    uint16_t                        DDB_SDK_Version;        // +0x04 SDK version (in this case 3.10)
    uint16_t                        DDB_Req_Device_Number;  // +0x06 required device number (can be Undefined_Device_ID)
    uint8_t                         DDB_Dev_Major_Version;  // +0x08
    uint8_t                         DDB_Dev_Minor_Version;  // +0x09
    uint16_t                        DDB_Flags;              // +0x0A Flags for init calls complete
    char                            DDB_Name[8];            // +0x0C Device name (padded with spaces, not NUL terminated)
    uint32_t                        DDB_Init_Order;         // +0x14 Initialization Order
    uint32_t                        DDB_Control_Proc;       // +0x18 Offset of control procedure
    uint32_t                        DDB_V86_API_Proc;       // +0x1C Offset of API procedure or 0
    uint32_t                        DDB_PM_API_Proc;        // +0x20 Offset of API procedure or 0
    uint32_t                        DDB_V86_API_CSIP;       // +0x24 CS:IP of API entry point
    uint32_t                        DDB_PM_API_CSIP;        // +0x28 CS:IP of API entry point
    uint32_t                        DDB_Reference_Data;     // +0x2C Reference data from real mode
    uint32_t                        DDB_Service_Table_Ptr;  // +0x30 Pointer to service table
    uint32_t                        DDB_Service_Table_Size; // +0x34 Number of services
};                                                          // =0x38
#pragma pack(pop)

static inline uint32_t exe_le_PAGE_SHIFT(const struct exe_le_header * const hdr) {
    if (hdr->signature == EXE_LX_SIGNATURE)
        return hdr->bytes_on_last_page;     // LX replaces with PAGE_SHIFT

    return 0;
}

const char *le_cpu_type_to_str(const uint8_t b);
const char *le_target_operating_system_to_str(const uint8_t b);

