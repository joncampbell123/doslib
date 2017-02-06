
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>

#include <hw/dos/exehdr.h>

/* re-use a little code from the NE parser. */
#include <hw/dos/exenehdr.h>
#include <hw/dos/exenepar.h>
#include <hw/dos/exelehdr.h>
#include <hw/dos/exelepar.h>

void le_header_object_table_loaded_linear_free(struct le_header_parseinfo * const h) {
    if (h->le_object_table_loaded_linear) free(h->le_object_table_loaded_linear);
    h->le_object_table_loaded_linear = NULL;
}

uint32_t *le_header_object_table_loaded_linear_alloc(struct le_header_parseinfo * const h) {
    if (h->le_object_table_loaded_linear == NULL) {
        h->le_object_table_loaded_linear = malloc(sizeof(uint32_t) * h->le_header.object_table_entries);
        if (h->le_object_table_loaded_linear != NULL)
            memset(h->le_object_table_loaded_linear,0,sizeof(uint32_t) * h->le_header.object_table_entries);
    }

    return h->le_object_table_loaded_linear;
}

void le_header_object_table_loaded_linear_generate(struct le_header_parseinfo * const h) {
    struct exe_le_header_object_table_entry *ent;
    uint32_t next,cur;
    uint32_t *list;
    size_t i;

    list = le_header_object_table_loaded_linear_alloc(h);
    if (list == NULL) return;

    cur = 0;
    next = 0;
    h->le_object_flat_32bit = 0;
    for (i=0;i < h->le_header.object_table_entries;i++) {
        ent = h->le_object_table + i;

        /* the general assumption of this code is that 32-bit segments are loaded into a
         * flat memory address space. in order for this to work, we have to pick the first
         * 32-bit segment and base ALL decoding from that segment. */
        if (h->le_object_flat_32bit == 0) {
            if (ent->object_flags & LE_HEADER_OBJECT_TABLE_ENTRY_FLAGS_386_BIG_DEFAULT)
                h->le_object_flat_32bit = i + 1;
        }

        /* some VXDs have relocation offsets, use them.
         * other VXDs have relocation offsets set to zero (Windows 95 VXDs). fill in our own. */
        if (ent->page_map_index != 0)
            cur = ((uint32_t)ent->page_map_index - (uint32_t)1) * (uint32_t)h->le_header.memory_page_size;
        else
            cur = next;

        if (cur < ent->relocation_base_address)
            cur = ent->relocation_base_address;

        cur += h->le_header.memory_page_size - 1;
        cur -= cur % h->le_header.memory_page_size;

        h->le_object_table_loaded_linear[i] = cur;

        next = cur + ((uint32_t)ent->page_map_entries * (uint32_t)h->le_header.memory_page_size);
    }
}

void le_header_parseinfo_init(struct le_header_parseinfo * const h) {
    memset(h,0,sizeof(*h));
}

void le_header_parseinfo_free(struct le_header_parseinfo * const h) {
    exe_ne_header_name_entry_table_free(&h->le_nonresident_names);
    exe_ne_header_name_entry_table_free(&h->le_resident_names);
    le_header_fixup_record_list_free(&h->le_fixup_records);
    le_header_parseinfo_free_object_page_map_table(h);
    le_header_entry_table_free(&h->le_entry_table);
    le_header_parseinfo_free_fixup_page_table(h);
    le_header_object_table_loaded_linear_free(h);
    le_header_parseinfo_free_object_table(h);
}

