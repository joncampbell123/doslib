
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
};                                                  // =0x40
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_segment_entry {
    uint16_t        offset_in_segments;             // +0x00 offset (in sectors) to segment data. zero if no data exists
    uint16_t        length;                         // +0x02 length (in bytes) of segment data. zero == 64KB if offset != 0
    uint16_t        flags;                          // +0x04 flags
    uint16_t        minimum_allocation_size;        // +0x06 minimum allocation size (in bytes). zero == 64KB
};                                                  // =0x08
#pragma pack(pop)

#pragma pack(push,1)
union exe_ne_header_segment_relocation_entry {
    struct {
        unsigned char       reloc_address_type;     // +0x00 relocation address type
        unsigned char       reloc_type;             // +0x01 relocation type
        uint16_t            seg_offset;             // +0x02 offset of the relocation in the segment
        unsigned char       raw5,raw6;              // +0x04 raw bytes
        unsigned char       raw7,raw8;              // +0x06 raw bytes
    } r;                                            // =0x08 RAW BYTES base header
    struct {
        unsigned char       _pad[4];                // +0x00 pad
        unsigned char       segment_index;          // +0x04 segment index the reference refs to (not 0xFF)
        unsigned char       zero;                   // +0x05 is zero
        uint16_t            seg_offset;             // +0x06 offset into the segment_index this refs to
    } intref;                                       // =0x08 internal reference (segment index != 0xFF)
    struct {
        unsigned char       _pad[4];                // +0x00 pad
        unsigned char       segment_index;          // +0x04 segment index the reference refs to (0xFF)
        unsigned char       zero;                   // +0x05 is zero
        uint16_t            entry_ordinal;          // +0x06 ordinal value in entry table
    } movintref;                                    // =0x08 movable internal reference (segment index == 0xFF)
    struct {
        unsigned char       _pad[4];                // +0x00 pad
        uint16_t            module_reference_index; // +0x04 index into the module reference table
        uint16_t            ordinal;                // +0x06 module symbol to refer to by ordinal
    } ordinal;                                      // =0x08 ordinal reference
    struct {
        unsigned char       _pad[4];                // +0x00 pad
        uint16_t            module_reference_index; // +0x04 index into the module reference table
        uint16_t            imported_name_offset;   // +0x06 offset into the imported name table
    } name;                                         // =0x08 name reference
    struct {
        unsigned char       _pad[4];                // +0x00 pad
        uint16_t            fixup;                  // +0x04 fixup type
        uint16_t            zero;                   // +0x06 is zero
    } osfixup;                                      // =0x08 name reference
};                                                  // =0x08
#pragma pack(pop)

/* exe_ne_header_segment_relocation_entry->r.reloc_type */
#define EXE_NE_HEADER_SEGMENT_RELOC_TYPE_INTERNAL_REFERENCE         (0x00)
#define EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_ORDINAL           (0x01)
#define EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_NAME              (0x02)
#define EXE_NE_HEADER_SEGMENT_RELOC_TYPE_OSFIXUP                    (0x03)
#define EXE_NE_HEADER_SEGMENT_RELOC_TYPE_MASK                       (0x03)
#define EXE_NE_HEADER_SEGMENT_RELOC_TYPE_ADDITIVE                   (0x04)

/* exe_ne_header_segment_relocation_entry->r.reloc_address_type */
#define EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_OFFSET_LOBYTE         (0x00)  /* offset (low 8 bits) */
#define EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_SEGMENT               (0x02)  /* 16-bit segment */
#define EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_FAR_POINTER           (0x03)  /* 16:16 segment:offset */
#define EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_OFFSET                (0x05)  /* 16-bit offset */
#define EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_FAR48_POINTER         (0x0B)  /* 16:32 segment:offset */
#define EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_OFFSET32              (0x0D)  /* 32-bit offset */
#define EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_MASK                  (0x0F)

/* exe_ne_header_segment_entry->flags */
#define EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_DATA          (1U << 0U)  /* is data segment (else, is code) */
#define EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_ALLOCATED     (1U << 1U)  /* loader has allocated memory (internal runtime state, obviously) */
#define EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_LOADED        (1U << 2U)  /* loader has loaded the segment (internal runtime state, obviously) */
#define EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_MOVEABLE      (1U << 4U)  /* is MOVEABLE (else, FIXED) */
#define EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_SHAREABLE     (1U << 5U)  /* is SHAREABLE (else, NONSHAREABLE) */
#define EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_PRELOAD       (1U << 6U)  /* is PRELOAD (else, LOADONCALL) */
#define EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_RO_EX         (1U << 7U)  /* code: EXECUTEONLY   data: READONLY */
#define EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_RELOCATIONS   (1U << 8U)  /* segment contains relocation data (follows segment data in file) */
#define EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_DISCARDABLE   (1U << 12U) /* is DISCARDABLE */

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

#pragma pack(push,1)
struct exe_ne_header_resource_table {
    uint16_t                    rscAlignShift;      // +0x00
    /* TYPEINFO rscTypes[]                             +0x02      each entry sizeof(rscTypes) except last which is just the first uint16_t */
    /* BYTE rscResourceNames[]                         +0x02      array of length + string, until length == 0 */
};                                                  // =0x02
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_resource_table_typeinfo {
    uint16_t                    rtTypeID;           // +0x00
    uint16_t                    rtResourceCount;    // +0x02
    uint32_t                    rtReserved;         // +0x04
    /* NAMEINFO rtNameInfo[]                           +0x08 */
};                                                  // =0x08
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_resource_table_nameinfo {
    uint16_t                    rnOffset;           // +0x00 offset of the resource in segments. offset = rnOffset << rscAlignShift
    uint16_t                    rnLength;           // +0x02 length of the resource in segments. size = rnLength << rscAlignShift. NOT BYTES!!!!
    uint16_t                    rnFlags;            // +0x04
    uint16_t                    rnID;               // +0x06
    uint16_t                    rnHandle;           // +0x08
    uint16_t                    rnUsage;            // +0x0A
};                                                  // =0x0C
#pragma pack(pop)
// NTS: Microsoft (and everyone else on the net with NE docs) mis-documents rnLength as length in bytes. WRONG!!!!!

#pragma pack(push,1)
struct exe_ne_header_resource_ICONDIR {
    uint16_t                    idReserved;         // +0x00
    uint16_t                    idType;             // +0x02
    uint16_t                    idCount;            // +0x04
    /* ICONDIRENTRY             idEntries[];           +0x06 */
};                                                  // =0x06
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_resource_ICONDIRENTRY {
    uint8_t                     bWidth;             // +0x00
    uint8_t                     bHeight;            // +0x01
    uint8_t                     bColorCount;        // +0x02
    uint8_t                     bReserved;          // +0x03
    uint16_t                    wPlanes;            // +0x04
    uint16_t                    wBitCount;          // +0x06
    uint32_t                    dwBytesInRes;       // +0x08
    uint32_t                    dwImageOffset;      // +0x0C
};                                                  // =0x10
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_resource_GRICONDIRENTRY {      // RT_GROUP_ICON
    uint8_t                     bWidth;             // +0x00
    uint8_t                     bHeight;            // +0x01
    uint8_t                     bColorCount;        // +0x02
    uint8_t                     bReserved;          // +0x03
    uint16_t                    wPlanes;            // +0x04
    uint16_t                    wBitCount;          // +0x06
    uint32_t                    dwBytesInRes;       // +0x08
    uint16_t                    nID;                // +0x0C NTS: This is the ICON rnID, not "index into RT_ICON group" as early Windows 3.1 mis-documentes it.
};                                                  // =0x0E
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_resource_CURSORDIR {
    uint16_t                    cdReserved;         // +0x00
    uint16_t                    cdType;             // +0x02
    uint16_t                    cdCount;            // +0x04
    /* CURSORDIRENTRY           cdEntries[];           +0x06 */
};                                                  // =0x06
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_resource_CURSORDIRENTRY {
    uint8_t                     bWidth;             // +0x00
    uint8_t                     bHeight;            // +0x01
    uint8_t                     bColorCount;        // +0x02
    uint8_t                     bReserved;          // +0x03
    uint16_t                    wXHotspot;          // +0x04
    uint16_t                    wYHotspot;          // +0x06
    uint32_t                    dwBytesInRes;       // +0x08
    uint32_t                    dwImageOffset;      // +0x0C
};                                                  // =0x10
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_resource_GRCURSORDIRENTRY {
    uint16_t                    wWidth;             // +0x00
    uint16_t                    wHeight;            // +0x02
    uint16_t                    wPlanes;            // +0x04
    uint16_t                    wBitCount;          // +0x06
    uint32_t                    lBytesInRes;        // +0x08
    uint16_t                    nID;                // +0x0C NTS: This is the CURSOR rnID, not "wImageIndex into RT_CURSOR group" as early Windows 3.1 mis-documentes it.
};                                                  // =0x0E
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_RGBQUAD {
    uint8_t                     rgbBlue;            // +0x00
    uint8_t                     rgbGreen;           // +0x01
    uint8_t                     rgbRed;             // +0x02
    uint8_t                     rgbReserved;        // +0x03
};                                                  // =0x04
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_BITMAP {                       // the original (pre-Windows 3.x) BITMAP format. names taken from Windows 1.04 SDK.
    int16_t                     bmType;             // +0x00 must be zero, according to MSDN docs. Windows 1.x SDK doesn't say anything about it.
    int16_t                     bmWidth;            // +0x02
    int16_t                     bmHeight;           // +0x04
    int16_t                     bmWidthBytes;       // +0x06 bytes per scanline aka stride. MSDN says must be multiple of 2 because old Windows assumes WORD alignment.
    uint8_t                     bmPlanes;           // +0x08
    uint8_t                     bmBitsPixel;        // +0x09
};                                                  // =0x0A
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_RTBITMAP {                     // RT_BITMAP resource in Windows 1.x/2.x
    uint8_t                     rnType;             // +0x00 always 0x02
    uint8_t                     rnFlags;            // +0x01 flags of some kind (unknown meaning)
    int16_t                     bmType;             // +0x02 must be zero, according to MSDN docs. Windows 1.x SDK doesn't say anything about it.
    int16_t                     bmWidth;            // +0x04
    int16_t                     bmHeight;           // +0x06
    int16_t                     bmWidthBytes;       // +0x08 bytes per scanline aka stride. MSDN says must be multiple of 2 because old Windows assumes WORD alignment.
    uint8_t                     bmPlanes;           // +0x0A
    uint8_t                     bmBitsPixel;        // +0x0B
    uint32_t                    bmZero;             // +0x0C unknown field, always zero
};                                                  // =0x10
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_RTICONBITMAP {                 // RT_ICON resource in Windows 1.x/2.x
    uint8_t                     rnType;             // +0x00 always 0x01
    uint8_t                     rnFlags;            // +0x01 flags of some kind (unknown meaning)
    uint16_t                    rnZero;             // +0x02 unknown field, always zero
    int16_t                     bmType;             // +0x04 must be zero, according to MSDN docs. Windows 1.x SDK doesn't say anything about it.
    int16_t                     bmWidth;            // +0x06
    int16_t                     bmHeight;           // +0x08
    int16_t                     bmWidthBytes;       // +0x0A bytes per scanline aka stride. MSDN says must be multiple of 2 because old Windows assumes WORD alignment.
    uint8_t                     bmPlanes;           // +0x0C zero for some reason??
    uint8_t                     bmBitsPixel;        // +0x0D zero for some reason??
};                                                  // =0x0E
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_RTCURSORBITMAP {               // RT_CURSOR resource in Windows 1.x/2.x (same as RT_ICON)
    uint8_t                     rnType;             // +0x00 always 0x03
    uint8_t                     rnFlags;            // +0x01 flags of some kind (unknown meaning)
    uint16_t                    hotspotX;           // +0x02 cursor hotspot X coordinate
    uint16_t                    hotspotY;           // +0x04 cursor hotspot Y coordinate
    int16_t                     bmWidth;            // +0x06
    int16_t                     bmHeight;           // +0x08
    int16_t                     bmWidthBytes;       // +0x0A bytes per scanline aka stride. MSDN says must be multiple of 2 because old Windows assumes WORD alignment.
    uint8_t                     bmPlanes;           // +0x0C zero for some reason??
    uint8_t                     bmBitsPixel;        // +0x0D zero for some reason??
};                                                  // =0x0E
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_BITMAPCOREHEADER {             // such as used in Windows 2.x
    uint32_t                    biSize;             // +0x00
    int32_t                     biWidth;            // +0x04
    int32_t                     biHeight;           // +0x08
    uint16_t                    biPlanes;           // +0x0C
    uint16_t                    biBitCount;         // +0x0E
};                                                  // =0x10
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_BITMAPINFOHEADER {
    uint32_t                    biSize;             // +0x00
    int32_t                     biWidth;            // +0x04
    int32_t                     biHeight;           // +0x08
    uint16_t                    biPlanes;           // +0x0C
    uint16_t                    biBitCount;         // +0x0E
    uint32_t                    biCompression;      // +0x10
    uint32_t                    biSizeImage;        // +0x14
    int32_t                     biXPelsPerMeter;    // +0x18
    int32_t                     biYPelsPerMeter;    // +0x1C
    uint32_t                    biClrUsed;          // +0x20
    uint32_t                    biClrImportant;     // +0x24
};                                                  // =0x28
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_resource_NAMETABLE {
    uint16_t                    wBytesInEntry;      // +0x00 bytes in this name table entry
    uint16_t                    wTypeOrdinal;       // +0x02
    uint16_t                    wIDOrdinal;         // +0x04
/*  char                        szType[]               +0x06 */
/*  char                        szID[]                 +0x06 */
};                                                  // =0x06
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_resource_ACCELERATOR {
    uint8_t                     fFlags;             // +0x00
    uint16_t                    wEvent;             // +0x01
    uint16_t                    wId;                // +0x03
};                                                  // =0x05
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_resource_MENU_HEADER {         // RT_MENU
    uint16_t                    wVersion;           // +0x00 version (0 for Windows 3.0 and later)
    uint16_t                    wReserved;          // +0x02 must be zero
};                                                  // =0x04
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_resource_MENU_POPUP_ITEM {     // RT_MENU popup menu item (fItemFlags & MF_POPUP)
    uint16_t                    fItemFlags;         // +0x00 flags (MF_*) flags you use with the menu API in Windows. MF_POPUP is set for this struct.
/*  char                        szItemText[];          +0x02 null-terminated string, containing text */
};                                                  // =0x02
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_resource_MENU_NORMAL_ITEM {    // RT_MENU normal menu item !(fItemFlags & MF_POPUP)
    uint16_t                    fItemFlags;         // +0x00 flags (MF_*) flags you use with the menu API in Windows. MF_POPUP is NOT set for this struct.
    uint16_t                    wMenuID;            // +0x02 menu item ID (what is sent to WM_COMMAND)
/*  char                        szItemText[];          +0x04 null-terminated string, containing text */
};                                                  // =0x04
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_resource_WIN2X_MENU_POPUP_ITEM { // Windows 2.x RT_MENU popup menu item (fItemFlags & MF_POPUP)
    uint8_t                     fItemFlags;         // +0x00 flags (MF_*) flags you use with the menu API in Windows. MF_POPUP is set for this struct.
/*  char                        szItemText[];          +0x02 null-terminated string, containing text */
};                                                  // =0x02
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_resource_WIN2X_MENU_NORMAL_ITEM { // Windows 2.x RT_MENU normal menu item !(fItemFlags & MF_POPUP)
    uint8_t                     fItemFlags;         // +0x00 flags (MF_*) flags you use with the menu API in Windows. MF_POPUP is NOT set for this struct.
    uint16_t                    wMenuID;            // +0x02 menu item ID (what is sent to WM_COMMAND)
/*  char                        szItemText[];          +0x04 null-terminated string, containing text */
};                                                  // =0x04
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_resource_DIALOG_HEADER {       // RT_DIALOG header
    uint32_t                    lStyle;             // +0x00 window style
    uint8_t                     bNumberOfItems;     // +0x04 number of controls in the dialog
    uint16_t                    x;                  // +0x05 x-coordinate of the upper left corner of the dialog in dialog units (explanation below)
    uint16_t                    y;                  // +0x07 y-coordinate of the lower left corner of the dialog in dialog units (explanation below)
    uint16_t                    cx;                 // +0x09 width of the dialog box in dialog units
    uint16_t                    cy;                 // +0x0B height of the dialog box in dialog units
/* char                         szMenuName[]           +0x0D menu resource, or NUL if no menu, or {uint8_t,uint16} = {0xFF,ordinal} to ref by ordinal */
/* char                         szClassName[]          +0x0D class name for the dialog, or NUL to use default */
/* char                         szCaption[]            +0x0D dialog box caption, NUL terminated */
/*          if (lstyle & DS_SETFONT) */
/* uint16_t                     wPointSize;            +0x0D point size of the font to use with the dialog */
/* char                         szFaceName[]           +0x0D typeface name of the dialog box font */
};                                                  // =0x0D
#pragma pack(pop)
// dialog units, x-coordinate: unit of measurement based on average char width of font divided by 4.
// dialog units, y-coordinate: unit of measurement based on char height of font divided by 8
// x: horizontal distance from the left edge of the parent window
// y: vertical distance from the top of the parent window

#pragma pack(push,1)
struct exe_ne_header_resource_DIALOG_CONTROL {      // RT_DIALOG control element
    uint16_t                    x;                  // +0x00 x-coordinate of upper-left corner of control, dialog units
    uint16_t                    y;                  // +0x02 y-coordinate of upper-left corner of control, dialog units
    uint16_t                    cx;                 // +0x04 width of the control, dialog units
    uint16_t                    cy;                 // +0x06 height of the control, dialog units
    uint16_t                    wID;                // +0x08 ID of control
    uint32_t                    lStyle;             // +0x0A control style
/* union {
 *  uint8_t                     class;                 +0x0E first byte & 0x80, is class type
 *  char                        szClass[];             +0x0E string value of class
 * } */
};                                                  // =0x0E
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_resource_VERSION_BLOCK {       // RT_VERSION block
    uint16_t                    cbBlock;            // +0x00 size of the complete block, including nested blocks
    uint16_t                    cbValue;            // +0x02 size of the abValue member of the struct in bytes
/* char                         szKey[];               +0x02 NUL-terminated string, name of the block */
/* BYTE                         abValue[];             +0x02 content of the block (NUL-terminated string, array of word values, whatever) */
};                                                  // =0x02
#pragma pack(pop)
// Blocks can be nested, in which case cbBlock is the sum of all nested blocks + cbValue.
// One or more nested blocks can exist if cbValue and the length of szKey do not fill the complete block.

#pragma pack(push,1)
struct exe_ne_header_resource_VS_FIXEDFILEINFO_WIN31 { // as defined by Windows 3.1 SDK 
    uint32_t                    dwSignature;        // +0x00 0xFEEF04BD
    uint32_t                    dwStrucVersion;     // +0x04 binary version number of this structure. High-order word is major, low-order word is minor [1]
    uint32_t                    dwFileVersionMS;    // +0x08 high 32-bits of binary version number for the file
    uint32_t                    dwFileVersionLS;    // +0x0C low 32-bits of binary version number for the file
    uint32_t                    dwProductVersionMS; // +0x10 high 32-bits of bianry version number for the product
    uint32_t                    dwProductVersionLS; // +0x14 low 32-bits of binary version number for the product
    uint32_t                    dwFileFlagsMask;    // +0x18 which bits in dwFileFlags are valid
    uint32_t                    dwFileFlags;        // +0x1C attributes of the file
    uint32_t                    dwFileOS;           // +0x20 operating system for which this file is designed
    uint32_t                    dwFileType;         // +0x24 general type of file
    uint32_t                    dwFileSubtype;      // +0x28 function of the file
    uint32_t                    dwFileDateMS;       // +0x2C high 32-bits of binary date/time stamp for the file
    uint32_t                    dwFileDateLS;       // +0x30 low 32-bits of binary date/time stamp for the file
};                                                  // =0x34
#pragma pack(pop)
// [1] dwStrucVersion
//       Windows 3.1 SDK:    Must be greater than 0x00000029

#define exe_ne_header_VS_FF_DEBUG                   0x00000001UL
#define exe_ne_header_VS_FF_PRERELEASE              0x00000002UL
#define exe_ne_header_VS_FF_PATCHED                 0x00000004UL
#define exe_ne_header_VS_FF_PRIVATEBUILD            0x00000008UL
#define exe_ne_header_VS_FF_INFOINFERRED            0x00000010UL
#define exe_ne_header_VS_FF_SPECIALBUILD            0x00000020UL

#define exe_ne_header_VOS_UNKNOWN                   0x00000000L
#define exe_ne_header_VOS_DOS                       0x00010000L
#define exe_ne_header_VOS_OS216                     0x00020000L
#define exe_ne_header_VOS_OS232                     0x00030000L
#define exe_ne_header_VOS_NT                        0x00040000L
#define exe_ne_header_VOS__BASE                     0x00000000L
#define exe_ne_header_VOS__WINDOWS16                0x00000001L
#define exe_ne_header_VOS__PM16                     0x00000002L
#define exe_ne_header_VOS__PM32                     0x00000003L
#define exe_ne_header_VOS__WINDOWS32                0x00000004L
#define exe_ne_header_VOS_DOS_WINDOWS16             0x00010001L
#define exe_ne_header_VOS_DOS_WINDOWS32             0x00010004L
#define exe_ne_header_VOS_OS216_PM16                0x00020002L
#define exe_ne_header_VOS_OS232_PM32                0x00030003L
#define exe_ne_header_VOS_NT_WINDOWS32              0x00040004L

#define exe_ne_header_VFT_UNKNOWN                   0x00000000L
#define exe_ne_header_VFT_APP                       0x00000001L
#define exe_ne_header_VFT_DLL                       0x00000002L
#define exe_ne_header_VFT_DRV                       0x00000003L
#define exe_ne_header_VFT_FONT                      0x00000004L
#define exe_ne_header_VFT_VXD                       0x00000005L
#define exe_ne_header_VFT_STATIC_LIB                0x00000007L

#define exe_ne_header_DS_ABSALIGN                   0x00000001UL
#define exe_ne_header_DS_SYSMODAL                   0x00000002UL
#define exe_ne_header_DS_LOCALEDIT                  0x00000020UL
#define exe_ne_header_DS_SETFONT                    0x00000040UL
#define exe_ne_header_DS_MODALFRAME                 0x00000080UL
#define exe_ne_header_DS_NOIDLEMSG                  0x00000100UL
#define exe_ne_header_WS_TABSTOP                    0x00010000UL
#define exe_ne_header_WS_MAXIMIZEBOX                0x00010000UL
#define exe_ne_header_WS_GROUP                      0x00020000UL
#define exe_ne_header_WS_MINIMIZEBOX                0x00020000UL
#define exe_ne_header_WS_THICKFRAME                 0x00040000UL
#define exe_ne_header_WS_SYSMENU                    0x00080000UL
#define exe_ne_header_WS_HSCROLL                    0x00100000UL
#define exe_ne_header_WS_VSCROLL                    0x00200000UL
#define exe_ne_header_WS_DLGFRAME                   0x00400000UL
#define exe_ne_header_WS_BORDER                     0x00800000UL
#define exe_ne_header_WS_CAPTION                    0x00C00000UL // COMBINED WS_BORDER+WS_DLGFRAME
#define exe_ne_header_WS_MAXIMIZE                   0x01000000UL
#define exe_ne_header_WS_CLIPCHILDREN               0x02000000UL
#define exe_ne_header_WS_CLIPSIBLINGS               0x04000000UL
#define exe_ne_header_WS_DISABLED                   0x08000000UL
#define exe_ne_header_WS_VISIBLE                    0x10000000UL
#define exe_ne_header_WS_MINIMIZE                   0x20000000UL
#define exe_ne_header_WS_CHILD                      0x40000000UL
#define exe_ne_header_WS_POPUP                      0x80000000UL

#define exe_ne_header_MF_GRAYED                     0x0001
#define exe_ne_header_MF_DISABLED                   0x0002
#define exe_ne_header_MF_BITMAP                     0x0004
#define exe_ne_header_MF_CHECKED                    0x0008
#define exe_ne_header_MF_POPUP                      0x0010
#define exe_ne_header_MF_MENUBARBREAK               0x0020
#define exe_ne_header_MF_MENUBREAK                  0x0040
#define exe_ne_header_MF_END                        0x0080
#define exe_ne_header_MF_OWNERDRAW                  0x0100

// In typical Microsoft documentation style, the MSDN 1992 documentation of an RT_ACCELERATOR resource
// conveniently forgot to document that bit 0 defines whether wEvent is a VK_* scan code or ASCII code.
// Notice in most cases that bit is set, but not always. Microsoft Visual C++ 1.5 App Studio reveals
// for example that the mystery cases with bit 0 == 0 in NOTEPAD.EXE are ASCII codes, therefore it is
// not VK_CANCEL but CTRL+C (but without signalling that CTRL is ever held down), not VK_KANJI but
// CTRL+Z, and so on. Sheesh.

#define exe_ne_header_ACCELERATOR_FLAGS_VIRTUAL_KEY             0x01U	// wEvent is a VK scan code
#define exe_ne_header_ACCELERATOR_FLAGS_NOHILITE_TOPLEVEL       0x02U   // top-level menu item not hilighted when accelerator used
#define exe_ne_header_ACCELERATOR_FLAGS_SHIFT                   0x04U   // need SHIFT key down to activate
#define exe_ne_header_ACCELERATOR_FLAGS_CONTROL                 0x08U   // need CTRL key down to activate
#define exe_ne_header_ACCELERATOR_FLAGS_ALT                     0x10U   // need ALT key down to activate
#define exe_ne_header_ACCELERATOR_FLAGS_LAST_ENTRY              0x80U   // entry is last entry in table

#define exe_ne_header_BI_RGB                        0x00000000UL
#define exe_ne_header_BI_RLE8                       0x00000001UL
#define exe_ne_header_BI_RLE4                       0x00000002UL
#define exe_ne_header_BI_BITFIELDS                  0x00000003UL
#define exe_ne_header_BI_JPEG                       0x00000004UL
#define exe_ne_header_BI_PNG                        0x00000005UL

#define exe_ne_header_RT_CURSOR                     0x8001
#define exe_ne_header_RT_BITMAP                     0x8002
#define exe_ne_header_RT_ICON                       0x8003
#define exe_ne_header_RT_MENU                       0x8004
#define exe_ne_header_RT_DIALOG                     0x8005
#define exe_ne_header_RT_STRING                     0x8006
#define exe_ne_header_RT_FONTDIR                    0x8007
#define exe_ne_header_RT_FONT                       0x8008
#define exe_ne_header_RT_ACCELERATOR                0x8009
#define exe_ne_header_RT_RCDATA                     0x800A
#define exe_ne_header_RT_MESSAGETABLE               0x800B
#define exe_ne_header_RT_GROUP_CURSOR               0x800C
#define exe_ne_header_RT_GROUP_ICON                 0x800E
#define exe_ne_header_RT_NAME_TABLE                 0x800F  /* Not used in Windows 3.1, supported in Windows 3.0 but slow */
#define exe_ne_header_RT_VERSION                    0x8010

