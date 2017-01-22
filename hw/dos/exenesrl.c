
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

#include <hw/dos/exehdr.h>
#include <hw/dos/exenehdr.h>
#include <hw/dos/exenepar.h>

void exe_ne_header_segment_reloc_table_init(struct exe_ne_header_segment_reloc_table * const t) {
    memset(t,0,sizeof(*t));
}

void exe_ne_header_segment_reloc_table_free_table(struct exe_ne_header_segment_reloc_table * const t) {
    if (t->table) free(t->table);
    t->table = NULL;
    t->length = 0;
}

size_t exe_ne_header_segment_reloc_table_size(struct exe_ne_header_segment_reloc_table * const t) {
    if (t->table == NULL) return 0;
    return t->length * sizeof(*(t->table));
}

unsigned char *exe_ne_header_segment_reloc_table_alloc_table(struct exe_ne_header_segment_reloc_table * const t,const unsigned int entries) {
    exe_ne_header_segment_reloc_table_free_table(t);
    if (entries == 0) return NULL;

    assert(sizeof(*(t->table)) == 8);
    t->table = malloc(entries * sizeof(*(t->table)));
    if (t->table == NULL) return NULL;
    t->length = entries;
    return (unsigned char*)(t->table); /* <- so that the caller can read the segment table into our array */
}

void exe_ne_header_segment_reloc_table_free(struct exe_ne_header_segment_reloc_table * const t) {
    exe_ne_header_segment_reloc_table_free_table(t);
}

