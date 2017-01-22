
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

void exe_ne_header_entry_table_table_init(struct exe_ne_header_entry_table_table * const t) {
    memset(t,0,sizeof(*t));
}

void exe_ne_header_entry_table_table_free_table(struct exe_ne_header_entry_table_table * const t) {
    if (t->table) free(t->table);
    t->table = NULL;
    t->length = 0;
}

void exe_ne_header_entry_table_table_free_raw(struct exe_ne_header_entry_table_table * const t) {
    if (t->raw && t->raw_ownership) free(t->raw);
    t->raw_length = 0;
    t->raw = 0;
}

unsigned char *exe_ne_header_entry_table_table_alloc_raw(struct exe_ne_header_entry_table_table * const t,const size_t length) {
    if (t->raw != NULL) {
        if (length == t->raw_length)
            return t->raw;

        exe_ne_header_entry_table_table_free_raw(t);
    }

    assert(t->raw == NULL);
    if (length == 0)
        return NULL;

    t->raw = malloc(length);
    if (t->raw == NULL)
        return NULL;

    t->raw_length = length;
    t->raw_ownership = 1;
    return t->raw;
}

void exe_ne_header_entry_table_table_free(struct exe_ne_header_entry_table_table * const t) {
    exe_ne_header_entry_table_table_free_table(t);
    exe_ne_header_entry_table_table_free_raw(t);
}

void exe_ne_header_entry_table_table_parse_raw(struct exe_ne_header_entry_table_table * const t) {
    unsigned char nument,seg_id;
    unsigned char *scan,*fence;
    unsigned int entries = 0;
    unsigned int scanst;
    unsigned int i;

    exe_ne_header_entry_table_table_free_table(t);
    if (t->raw == NULL || t->raw_length <= 2) return;
    fence = t->raw + t->raw_length;

    /* 2-BYTE     number, seg_id
     *     for i in 0 to number
     *        <entry, length varies by segment type>
     */

    /* first, how many entries? the "run-length" encoding used for common segment index/type makes this easier. */
    for (scan=t->raw;scan < fence;) {
        nument = *scan++;
        if (nument == 0 || scan >= fence) break;
        seg_id = *scan++;
        if (scan >= fence) break;

        if (seg_id == 0x00) {
            /* nothing to do or skip */
        }
        else if (seg_id == 0xFE) {
            /* constant entries are 3 bytes (BYTE FLAGS + WORD CONSTANT) */
            scan += (unsigned int)nument * 3U;
        }
        else if (seg_id == 0xFF) {
            /* movable segment refs are 6 bytes */
            scan += (unsigned int)nument * 6U;
        }
        else {
            /* fixed segment refs are 3 bytes */
            scan += (unsigned int)nument * 3U;
        }

        entries += nument;
    }

    if (entries == 0)
        return;

    t->table = (struct exe_ne_header_entry_table_entry*)malloc(entries * sizeof(*(t->table)));
    if (t->table == NULL) return;
    t->length = entries;

    /* scan and copy indexes to */
    entries = 0;
    for (scan=t->raw;scan < fence && entries < t->length;) {
        nument = *scan++;
        if (nument == 0 || scan >= fence) break;
        seg_id = *scan++;
        if (scan >= fence) break;

        if (seg_id == 0x00)
            scanst = 0; /* no bytes, empty */
        else if (seg_id == 0xFE)
            scanst = 3; /* constant, 3 bytes */
        else if (seg_id == 0xFF)
            scanst = 6; /* movable segment ref, 6 bytes */
        else
            scanst = 3; /* fixed segment ref, 3 bytes */

        for (i=0;i < nument && entries < t->length && scan < fence;i++,entries++,scan += scanst) {
            t->table[entries].segment_id = seg_id;
            t->table[entries].offset = (uint16_t)(scan - t->raw);
        }
    }

    t->length = entries;
}

size_t exe_ne_header_entry_table_table_raw_entry_size(const struct exe_ne_header_entry_table_entry * const ent) {
    if (ent->segment_id == 0xFF)
        return 6;
    else if (ent->segment_id != 0) // 0x01-0xFE
        return 3;

    return 0;
}

unsigned char *exe_ne_header_entry_table_table_raw_entry(const struct exe_ne_header_entry_table_table * const t,const struct exe_ne_header_entry_table_entry * const ent) {
    if (t->raw == NULL || t->raw_length == 0)
        return NULL;

    if ((ent->offset+exe_ne_header_entry_table_table_raw_entry_size(ent)) > t->raw_length)
        return NULL;

    return t->raw + ent->offset;
}

