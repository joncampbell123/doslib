
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
    le_header_parseinfo_free_object_table(h);
}

