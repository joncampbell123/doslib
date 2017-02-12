
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

uint32_t le_header_parseinfo_guess_le_header_size(struct le_header_parseinfo * const p) {
    uint32_t sz = ~((uint32_t)0UL);

    // assumption is that the LE header runs from the LE signature to the offset of
    // whichever table comes first after the LE header.
#define X(x) if (p->le_header.x != (uint32_t)0 && sz > p->le_header.x) sz = p->le_header.x
    X(offset_of_object_table);
    X(object_page_map_offset);
    X(object_iterate_data_map_offset);
    X(resource_table_offset);
    X(resident_names_table_offset);
    X(entry_table_offset);
    X(module_directives_table_offset);
    X(fixup_page_table_offset);
    X(fixup_record_table_offset);
    X(imported_modules_name_table_offset);
    X(imported_procedure_name_table_offset);
    X(per_page_checksum_table_offset);
#undef X

    if (sz < EXE_HEADER_LE_HEADER_SIZE)
        sz = EXE_HEADER_LE_HEADER_SIZE;

    return sz;
}

