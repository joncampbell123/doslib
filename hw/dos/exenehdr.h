
#define EXE_HEADER_NE_HEADER_SIZE                   (0x40UL)

#define EXE_NE_SIGNATURE                            (0x454EU)

#pragma pack(push,1)
struct exe_ne_header {
    uint16_t        signature;                      // +0x00 0x454E 'NE'
    uint8_t         linker_version;                 // +0x02 major version of the linker
    uint8_t         linker_revision;                // +0x03 minor version of the linker
    uint16_t        entry_table_offset;             // +0x04 offset to the entry header (relative to start of NE header)
    uint16_t        entry_table_length;             // +0x06 in bytes
    uint32_t        file_crc;                       // +0x08 32-bit CRC of entire file (CRC field as 0) OR "reserved" in later documentation
    uint16_t        flags;                          // +0x0C flags for the EXE file contents
    uint16_t        auto_data_segment_number;       // +0x0E number of auto data segment (1-based, zero if NOAUTODATA) */
    uint16_t        init_local_heap;                // +0x10 initial size of the local heap, or 0 if no local allocation */
    uint16_t        init_stack_size;                // +0x12 initial size of the stack. 0 if SS != DS */
    uint16_t        entry_ip;                       // +0x14 value of IP on entry
    uint16_t        entry_cs;                       // +0x16 segment index of CS segment on entry (1-based)
    uint16_t        entry_sp;                       // +0x18 value of SP on entry. if SS == AUTODATA segment and SP == 0, then
                                                    //       SP = sizeof AUTODATA segment + sizeof stack
    uint16_t        entry_ss;                       // +0x1A segment index of SS segment on entry (1-based)
    uint16_t        segment_table_entries;          // +0x1C number of entries in the segment table
    uint16_t        module_reftable_entries;        // +0x1E number of entries in the module reference table
    uint16_t        nonresident_name_table_length;  // +0x20 number of BYTES in the nonresident name table
    uint16_t        segment_table_offset;           // +0x22 offset to segment table (relative to start of NE header)
    uint16_t        resource_table_offset;          // +0x24 offset to resource table (relative to start of NE header)
    uint16_t        resident_name_table_offset;     // +0x26 offset to resident name table (relative to start of NE header)
    uint16_t        module_reference_table_offset;  // +0x28 offset to module reference table (relative to start of NE header)
    uint16_t        imported_name_table_offset;     // +0x2A offset to imported name table (relative to start of NE header)
    uint32_t        nonresident_name_table_offset;  // +0x2C offset to non-resident name table (relative to beginning of file) <- PAY ATTENTION HERE
    uint16_t        movable_entry_points;           // +0x30 number of movable entry points
    uint16_t        sector_shift;                   // +0x32 shift value used to convert sectors to file offset (sector << shift) == file offset
    uint16_t        resource_segments;              // +0x34 number of resource segments
    uint8_t         target_os;                      // +0x36 target OS ("according to which bits are set" but it's an enum not bitfield!)
    uint8_t         other_flags;                    // +0x37 other OS flags, meaning seems to vary between Windows and OS/2
    uint16_t        fastload_offset_sectors;        // +0x38 offset (in sectors) of fast-load area
    uint16_t        fastload_length_sectors;        // +0x3A length (in sectors) of fast-load area
    uint16_t        minimum_code_swap_area_size;    // +0x3C AKA "reserved"
    uint16_t        minimum_windows_version;        // +0x3E expected windows version (i.e. 0x300 = 3.0, 0x30A = 3.10)
};                                                  // +0x40
#pragma pack(pop)

/* exe_ne_header->other_flags (Windows) */
#define EXE_NE_HEADER_OTHER_FLAGS_WIN_WIN2X_IN_3X       (1U << 1U)  /* Windows 2.x app that can run in Windows 3.x protected mode */
#define EXE_NE_HEADER_OTHER_FLAGS_WIN_WIN2X_PROP_FONTS  (1U << 2U)  /* Windows 2.x app that supports proportional fonts */
#define EXE_NE_HEADER_OTHER_FLAGS_WIN_HAS_FASTLOAD      (1U << 3U)  /* Has a fast-load area */

/* exe_ne_header->other_flags (OS/2) */
#define EXE_NE_HEADER_OTHER_FLAGS_OS2_LFN               (1U << 0U)  /* OS/2 long filename support */
#define EXE_NE_HEADER_OTHER_FLAGS_OS2_2X_PROTMODE       (1U << 1U)  /* OS/2 2.x protected mode */
#define EXE_NE_HEADER_OTHER_FLAGS_OS2_PROP_FONTS        (1U << 2U)  /* OS/2 supports proportional fonts */
#define EXE_NE_HEADER_OTHER_FLAGS_OS2_HAS_GANGLOAD      (1U << 3U)  /* Has a gang-load area */

/* exe_ne_header->target_os_flags */
#define EXE_NE_HEADER_TARGET_OS_NONE                (0U)        /* NTS: Or, Windows 2.x and earlier */
#define EXE_NE_HEADER_TARGET_OS_OS2                 (1U)
#define EXE_NE_HEADER_TARGET_OS_WINDOWS             (2U)        /* NTS: Or, Windows 3.x and later */
#define EXE_NE_HEADER_TARGET_OS_EURO_MSDOS_4        (3U)
#define EXE_NE_HEADER_TARGET_OS_WINDOWS_386         (4U)
#define EXE_NE_HEADER_TARGET_OS_BOSS                (5U)

/* exe_ne_header->flags bits [1:0] */
#define EXE_NE_HEADER_FLAGS_DGROUPTYPE_NOAUTODATA   (0U)
#define EXE_NE_HEADER_FLAGS_DGROUPTYPE_SINGLEDATA   (1U)        /* AKA SHARED */
#define EXE_NE_HEADER_FLAGS_DGROUPTYPE_MULTIPLEDATA (2U)
#define EXE_NE_HEADER_FLAGS_DGROUPTYPE_MASK         (3U)

#define EXE_NE_HEADER_FLAGS_GLOBAL_INIT             (1U << 2U)  /* bit 2 */
#define EXE_NE_HEADER_FLAGS_PROTECTED_MODE_ONLY     (1U << 3U)  /* bit 3 */
#define EXE_NE_HEADER_FLAGS_8086                    (1U << 4U)  /* bit 4 contains 8086 instructions */
#define EXE_NE_HEADER_FLAGS_80286                   (1U << 5U)  /* bit 5 contains 286 instructions */
#define EXE_NE_HEADER_FLAGS_80386                   (1U << 6U)  /* bit 6 contains 386 instructions */
#define EXE_NE_HEADER_FLAGS_80x87                   (1U << 7U)  /* bit 7 contains 80x87 (FPU) instructions */

/* bits [10:8] */
#define EXE_NE_HEADER_FLAGS_APPTYPE_NONE            (0U << 8U)  
#define EXE_NE_HEADER_FLAGS_APPTYPE_FULLSCREEN      (1U << 8U)  /* not aware of Windows/PM API */
#define EXE_NE_HEADER_FLAGS_APPTYPE_WINPM_COMPAT    (2U << 8U)  /* compatible with Windows/PM API */
#define EXE_NE_HEADER_FLAGS_APPTYPE_WINPM_USES      (3U << 8U)  /* uses Windows/PM API */
#define EXE_NE_HEADER_FLAGS_APPTYPE_MASK            (7U << 8U)
#define EXE_NE_HEADER_FLAGS_APPTYPE_SHIFT           (8U)

/* CONFLICTING INFO HERE! */
#define EXE_NE_HEADER_FLAGS_OS2_FAMILY_APP          (1U << 11U)
#define EXE_NE_HEADER_FLAGS_FIRST_SEGMENT_CODE_APP_LOAD (1U << 11U) /* first segment contains code to load the application */

#define EXE_NE_HEADER_FLAGS_LINK_ERRORS             (1U << 13U) /* errors detected at link time but linker made the EXE anyway */
#define EXE_NE_HEADER_FLAGS_NON_CONFORMING          (1U << 14U) /* ?? */
#define EXE_NE_HEADER_FLAGS_DLL                     (1U << 15U) /* not an application, but a DLL. ignore SS:SP, CS:IP is FAR PROC LIBMAIN. */

