
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

void le_header_parseinfo_free_object_table(struct le_header_parseinfo * const h) {
    if (h->le_object_table) free(h->le_object_table);
    h->le_object_table = NULL;
}

size_t le_header_parseinfo_get_object_table_buffer_size(struct le_header_parseinfo * const h) {
    return sizeof(struct exe_le_header_object_table_entry) * h->le_header.object_table_entries;
}

unsigned char *le_header_parseinfo_alloc_object_table(struct le_header_parseinfo * const h) {
    const size_t sz = le_header_parseinfo_get_object_table_buffer_size(h);

    if (h->le_object_table == NULL)
        h->le_object_table = (struct exe_le_header_object_table_entry*)malloc(sz);

    return (unsigned char*)(h->le_object_table);
}

