
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

void le_header_parseinfo_free_object_page_map_table(struct le_header_parseinfo * const h) {
    if (h->le_object_page_map_table) free(h->le_object_page_map_table);
    h->le_object_page_map_table = NULL;
}

void le_header_parseinfo_finish_read_get_object_page_map_table(struct le_header_parseinfo * const h) {
    /* the caller read the raw data into memory.
     * we need to convert it in place to the exe_le_header_parseinfo_object_page_table_entry format.
     * conversion depend on whether it's the LE or LX format.
     *
     * There is NO protection against calling this function twice! */
    struct exe_le_header_parseinfo_object_page_table_entry *d;
    unsigned int i;

    if (h->le_header.number_of_memory_pages == 0)
        return;

    i = h->le_header.number_of_memory_pages - 1;
    d = (struct exe_le_header_parseinfo_object_page_table_entry*)h->le_object_page_map_table + i;

    /* convert backwards, in place */
    if (h->le_header.signature == EXE_LE_SIGNATURE) {
        struct exe_le_header_object_page_table_entry *s =
            ((struct exe_le_header_object_page_table_entry*)h->le_object_page_map_table) + i;

        assert(sizeof(*s) <= sizeof(*d));

        do {
            struct exe_le_header_object_page_table_entry se = *s; /* because we're converting in-place */

            d->page_data_offset =
                (((uint32_t)se.page_data_offset - (uint32_t)1) * (uint32_t)h->le_header.memory_page_size) +
                (uint32_t)h->le_header.data_pages_offset;
            d->data_size =
                (uint16_t)h->le_header.memory_page_size;
            d->flags =
                se.flags;

            d--; s--;
        } while ((i--) != 0);
    }
    else if (h->le_header.signature == EXE_LX_SIGNATURE) {
        uint32_t pshf =
            (uint32_t)exe_le_PAGE_SHIFT(&h->le_header);
        struct exe_lx_header_object_page_table_entry *s =
            ((struct exe_lx_header_object_page_table_entry*)h->le_object_page_map_table) + i;

        assert(sizeof(*s) <= sizeof(*d));

        do {
            struct exe_lx_header_object_page_table_entry se = *s; /* because we're converting in-place */

            d->page_data_offset =
                ((uint32_t)se.page_data_offset << pshf) +
                (uint32_t)h->le_header.data_pages_offset;
            d->data_size =
                se.data_size;
            d->flags =
                se.flags;

            d--; s--;
        } while ((i--) != 0);
    }
}

size_t le_header_parseinfo_get_object_page_map_table_read_buffer_size(struct le_header_parseinfo * const h) {
    if (h->le_header.signature == EXE_LE_SIGNATURE)
        return sizeof(struct exe_le_header_object_page_table_entry) * h->le_header.number_of_memory_pages;
    else if (h->le_header.signature == EXE_LX_SIGNATURE)
        return sizeof(struct exe_lx_header_object_page_table_entry) * h->le_header.number_of_memory_pages;

    return 0;
}

size_t le_header_parseinfo_get_object_page_map_table_buffer_size(struct le_header_parseinfo * const h) {
    return sizeof(struct exe_le_header_parseinfo_object_page_table_entry) * h->le_header.number_of_memory_pages;
}

unsigned char *le_header_parseinfo_alloc_object_page_map_table(struct le_header_parseinfo * const h) {
    const size_t sz = le_header_parseinfo_get_object_page_map_table_buffer_size(h);

    if (h->le_object_page_map_table == NULL)
        h->le_object_page_map_table = (struct exe_le_header_parseinfo_object_page_table_entry*)malloc(sz);

    return (unsigned char*)(h->le_object_page_map_table);
}

