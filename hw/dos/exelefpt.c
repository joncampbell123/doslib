
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

uint32_t le_header_parseinfo_fixup_page_table_entries(const struct le_header_parseinfo * const h) {
    if (h->le_fixup_page_table != NULL)
        return h->le_header.number_of_memory_pages + (uint32_t)1;

    return (uint32_t)0;
}

void le_header_parseinfo_free_fixup_page_table(struct le_header_parseinfo * const h) {
    if (h->le_fixup_page_table) free(h->le_fixup_page_table);
    h->le_fixup_page_table = NULL;
}

size_t le_header_parseinfo_get_fixup_page_table_buffer_size(struct le_header_parseinfo * const h) {
    return sizeof(uint32_t) * ((unsigned long)h->le_header.number_of_memory_pages + 1UL);
}

unsigned char *le_header_parseinfo_alloc_fixup_page_table(struct le_header_parseinfo * const h) {
    const size_t sz = le_header_parseinfo_get_fixup_page_table_buffer_size(h);

    if (h->le_fixup_page_table == NULL)
        h->le_fixup_page_table = (uint32_t*)malloc(sz);

    return (unsigned char*)(h->le_fixup_page_table);
}

