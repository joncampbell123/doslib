
struct exe_ne_header_resource_table_t {
    unsigned char*                                  raw;
    size_t                                          raw_length;
    unsigned char                                   raw_ownership;

    uint16_t*                                       typeinfo;
    uint16_t                                        typeinfo_length;

    uint16_t*                                       resnames;
    uint16_t                                        resnames_length;
};

struct exe_ne_header_imported_name_table {
    uint16_t*                                       table;
    unsigned int                                    length;
    unsigned char*                                  raw;
    size_t                                          raw_length;
    unsigned char                                   raw_ownership;
    /* module reference table (relies on this data) */
    uint16_t*                                       module_ref_table;
    unsigned int                                    module_ref_table_length; /* in entries of type uint16_t */
};

struct exe_ne_header_segment_reloc_table {
    union exe_ne_header_segment_relocation_entry*   table;
    unsigned int                                    length;
};

struct exe_ne_header_segment_table {
    struct exe_ne_header_segment_entry*             table;
    unsigned int                                    length;
    unsigned int                                    sector_shift;
};

struct exe_ne_header_name_entry_table {
    struct exe_ne_header_name_entry*                table;
    unsigned int                                    length;
    unsigned char*                                  raw;
    size_t                                          raw_length;
    unsigned char                                   raw_ownership;
};

#pragma pack(push,1)
struct exe_ne_header_name_entry {
    uint8_t         length;
    uint16_t        offset;             // 16-bit because resident/nonresident name table is limited to 64KB
};
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_entry_table_entry {
    uint8_t         segment_id;         // 0x00 = empty  0xFF = movable  0xFE = constant   anything else = segment index
    uint16_t        offset;             // 16-bit offset to entry
};
#pragma pack(pop)

struct exe_ne_header_entry_table_table {
    struct exe_ne_header_entry_table_entry*     table;
    unsigned int                                length;
    unsigned char*                              raw;
    size_t                                      raw_length;
    unsigned char                               raw_ownership;
};

#pragma pack(push,1)
struct exe_ne_header_entry_table_movable_segment_entry {
    uint8_t             flags;          // +0x00
    uint16_t            int3f;          // +0x01
    uint8_t             segid;          // +0x03
    uint16_t            seg_offs;       // +0x04
};                                      // =0x06
#pragma pack(pop)

#pragma pack(push,1)
struct exe_ne_header_entry_table_fixed_segment_entry {
    uint8_t             flags;          // +0x00
    union {
        uint16_t        seg_offs;       // +0x01 (fixed segment, segment_id 0x01 to 0xFD)
        uint16_t        const_value;    // +0x01 (constant value, segment_id 0xFE)
    } v;
};                                      // =0x03
#pragma pack(pop)

static inline uint16_t exe_ne_header_resource_table_typeinfo_TYPEID_IS_INTEGER(const uint16_t typeID) {
    return (typeID & 0x8000U);
}

static inline uint16_t exe_ne_header_resource_table_typeinfo_TYPEID_AS_INTEGER(const uint16_t typeID) {
    return (typeID & 0x7FFFU);
}

static inline uint16_t exe_ne_header_resource_table_typeinfo_RNID_IS_INTEGER(const uint16_t rnID) {
    return (rnID & 0x8000U);
}

static inline uint16_t exe_ne_header_resource_table_typeinfo_RNID_AS_INTEGER(const uint16_t rnID) {
    return (rnID & 0x7FFFU);
}

static inline uint16_t exe_ne_header_resource_table_get_shift(const struct exe_ne_header_resource_table_t * const t) {
    const struct exe_ne_header_resource_table *h = (const struct exe_ne_header_resource_table*)t->raw;
    if (h == NULL) return 0;
    return h->rscAlignShift;
}

const char *exe_ne_header_resource_table_typeinfo_TYPEID_INTEGER_name_str(const uint16_t typeID);
void exe_ne_header_resource_table_get_string(char *dst,size_t dstmax,const struct exe_ne_header_resource_table_t * const t,const uint16_t ofs);
void exe_ne_header_resource_table_init(struct exe_ne_header_resource_table_t * const t);
void exe_ne_header_resource_table_free_resnames(struct exe_ne_header_resource_table_t * const t);
void exe_ne_header_resource_table_free_typeinfo(struct exe_ne_header_resource_table_t * const t);
void exe_ne_header_resource_table_free_raw(struct exe_ne_header_resource_table_t * const t);
void exe_ne_header_resource_table_free(struct exe_ne_header_resource_table_t * const t);
const struct exe_ne_header_resource_table_typeinfo *exe_ne_header_resource_table_get_typeinfo_entry(const struct exe_ne_header_resource_table_t * const t,const unsigned int idx);
const struct exe_ne_header_resource_table_nameinfo *exe_ne_header_resource_table_get_typeinfo_nameinfo_entry(const struct exe_ne_header_resource_table_typeinfo * const tinfo,const unsigned int idx);
uint16_t exe_ne_header_resource_table_get_resname(const struct exe_ne_header_resource_table_t * const t,const unsigned int idx);
void exe_ne_header_resource_table_parse(struct exe_ne_header_resource_table_t * const t);
unsigned char *exe_ne_header_resource_table_alloc_raw(struct exe_ne_header_resource_table_t * const t,const size_t length);

void ne_imported_name_table_entry_get_name(char *dst,size_t dstmax,const struct exe_ne_header_imported_name_table * const t,const uint16_t offset);
void ne_imported_name_table_entry_get_module_ref_name(char *dst,size_t dstmax,const struct exe_ne_header_imported_name_table * const t,const uint16_t index);
void exe_ne_header_imported_name_table_init(struct exe_ne_header_imported_name_table * const t);
void exe_ne_header_imported_name_table_free_module_ref_table(struct exe_ne_header_imported_name_table * const t);
uint16_t *exe_ne_header_imported_name_table_alloc_module_ref_table(struct exe_ne_header_imported_name_table * const t,const size_t entries);
void exe_ne_header_imported_name_table_free_table(struct exe_ne_header_imported_name_table * const t);
void exe_ne_header_imported_name_table_free_raw(struct exe_ne_header_imported_name_table * const t);
unsigned char *exe_ne_header_imported_name_table_alloc_raw(struct exe_ne_header_imported_name_table * const t,const size_t length);
void exe_ne_header_imported_name_table_free(struct exe_ne_header_imported_name_table * const t);
int exe_ne_header_imported_name_table_parse_raw(struct exe_ne_header_imported_name_table * const t);

void exe_ne_header_segment_reloc_table_init(struct exe_ne_header_segment_reloc_table * const t);
void exe_ne_header_segment_reloc_table_free_table(struct exe_ne_header_segment_reloc_table * const t);
size_t exe_ne_header_segment_reloc_table_size(struct exe_ne_header_segment_reloc_table * const t);
unsigned char *exe_ne_header_segment_reloc_table_alloc_table(struct exe_ne_header_segment_reloc_table * const t,const unsigned int entries);
void exe_ne_header_segment_reloc_table_free(struct exe_ne_header_segment_reloc_table * const t);

void exe_ne_header_segment_table_init(struct exe_ne_header_segment_table * const t);
void exe_ne_header_segment_table_free_table(struct exe_ne_header_segment_table * const t);
size_t exe_ne_header_segment_table_size(struct exe_ne_header_segment_table * const t);
unsigned char *exe_ne_header_segment_table_alloc_table(struct exe_ne_header_segment_table * const t,const unsigned int entries,const unsigned int shift);
void exe_ne_header_segment_table_free(struct exe_ne_header_segment_table * const t);
unsigned long exe_ne_header_segment_table_get_relocation_table_offset(const struct exe_ne_header_segment_table * const t,const struct exe_ne_header_segment_entry * const s);

void exe_ne_header_name_entry_table_init(struct exe_ne_header_name_entry_table * const t);
void exe_ne_header_name_entry_table_free_table(struct exe_ne_header_name_entry_table * const t);
void exe_ne_header_name_entry_table_free_raw(struct exe_ne_header_name_entry_table * const t);
unsigned char *exe_ne_header_name_entry_table_alloc_raw(struct exe_ne_header_name_entry_table * const t,const size_t length);
void exe_ne_header_name_entry_table_free(struct exe_ne_header_name_entry_table * const t);
uint16_t ne_name_entry_get_ordinal(const struct exe_ne_header_name_entry_table * const t,const struct exe_ne_header_name_entry * const ent);
unsigned char *ne_name_entry_get_name_base(const struct exe_ne_header_name_entry_table * const t,const struct exe_ne_header_name_entry * const ent);
void ne_name_entry_get_name(char *dst,size_t dstmax,const struct exe_ne_header_name_entry_table * const t,const struct exe_ne_header_name_entry * const ent);
int exe_ne_header_name_entry_table_parse_raw(struct exe_ne_header_name_entry_table * const t);

int ne_name_entry_sort_by_name(const void *a,const void *b);
int ne_name_entry_sort_by_ordinal(const void *a,const void *b);

void exe_ne_header_entry_table_table_init(struct exe_ne_header_entry_table_table * const t);
void exe_ne_header_entry_table_table_free_table(struct exe_ne_header_entry_table_table * const t);
void exe_ne_header_entry_table_table_free_raw(struct exe_ne_header_entry_table_table * const t);
unsigned char *exe_ne_header_entry_table_table_alloc_raw(struct exe_ne_header_entry_table_table * const t,const size_t length);
void exe_ne_header_entry_table_table_free(struct exe_ne_header_entry_table_table * const t);
void exe_ne_header_entry_table_table_parse_raw(struct exe_ne_header_entry_table_table * const t);
size_t exe_ne_header_entry_table_table_raw_entry_size(const struct exe_ne_header_entry_table_entry * const ent);
unsigned char *exe_ne_header_entry_table_table_raw_entry(const struct exe_ne_header_entry_table_table * const t,const struct exe_ne_header_entry_table_entry * const ent);

extern struct exe_ne_header_name_entry_table *ne_name_entry_sort_by_table;

unsigned int exe_ne_header_is_WINOLDBITMAP(const unsigned char *data/*at least 4 bytes*/,const size_t len);
unsigned int exe_ne_header_is_WINOLDICON(const unsigned char *data/*at least 4 bytes*/,const size_t len);

unsigned int exe_ne_header_BITMAPINFOHEADER_get_palette_count(const struct exe_ne_header_BITMAPINFOHEADER *bmphdr);

const char *exe_ne_header_RT_DIALOG_ClassID_to_string(const uint8_t c);

