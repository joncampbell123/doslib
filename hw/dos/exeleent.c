
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

// the entry table is the only part that can't be determined by any other way than
// differences in offset. to complicate matters, some fields are set to zero by
// linkers instead of NE-style where all offsets just match the prior field's offset
// to signal no table.
uint32_t le_exe_header_entry_table_size(struct exe_le_header * const h) {
    // NTS: This searches in header field order, do not re-order.
    if (h->module_directives_table_offset >= h->entry_table_offset)
        return h->module_directives_table_offset - h->entry_table_offset;
    if (h->fixup_page_table_offset >= h->entry_table_offset)
        return h->fixup_page_table_offset - h->entry_table_offset;
    if (h->fixup_record_table_offset >= h->entry_table_offset)
        return h->fixup_record_table_offset - h->entry_table_offset;
    if (h->imported_modules_name_table_offset >= h->entry_table_offset)
        return h->imported_modules_name_table_offset - h->entry_table_offset;
    if (h->imported_procedure_name_table_offset >= h->entry_table_offset)
        return h->imported_procedure_name_table_offset - h->entry_table_offset;
    if (h->per_page_checksum_table_offset >= h->entry_table_offset)
        return h->per_page_checksum_table_offset - h->entry_table_offset;
    if (h->data_pages_offset >= h->entry_table_offset)
        return h->data_pages_offset - h->entry_table_offset;

    return 0;
}

void le_header_entry_table_free_table(struct le_header_entry_table *t) {
    if (t->table) free(t->table);
    t->table = NULL;
    t->length = 0;
}

void le_header_entry_table_free_raw(struct le_header_entry_table *t) {
    if (t->raw) free(t->raw);
    t->raw_length = 0;
    t->raw = NULL;
}

void le_header_entry_table_free(struct le_header_entry_table *t) {
    le_header_entry_table_free_table(t);
    le_header_entry_table_free_raw(t);
}

unsigned char *le_header_entry_table_get_raw_entry(struct le_header_entry_table *t,size_t i) {
    size_t o;

    if (t->table == NULL || t->raw == NULL)
        return NULL;
    if (t->length == 0 || t->raw_length == 0)
        return NULL;
    if (i >= t->length)
        return NULL;

    o = t->table[i].offset;
    if (o >= t->raw_length)
        return NULL;

    return t->raw + o;
}

unsigned char *le_header_entry_table_alloc(struct le_header_entry_table *t,size_t sz) {
    le_header_entry_table_free_table(t);
    if (t->raw == NULL) {
        t->raw = malloc(sz);
        if (t->raw == NULL) return NULL;
        t->raw_length = sz;
    }

    return t->raw;
}

void le_header_entry_table_parse(struct le_header_entry_table * const t) {
    struct le_header_entry_table_entry *ent;
    unsigned char *base,*scan,*fence;
    unsigned int i,ordinal;
    unsigned char cnt,typ;
    uint16_t object;

    le_header_entry_table_free_table(t);
    if (t->raw == NULL || t->raw_length == 0)
        return;

    base = t->raw;
    fence = base + t->raw_length;

    /* first pass: count items */
    ordinal = 0;
    scan = base;
    while (scan < fence) {
        cnt = *scan++;
        if (cnt == 0 || scan >= fence) break;
        typ = *scan++;
        if (scan >= fence) break;

        if (typ == 0) {
            ordinal += cnt;
        }
        else if (typ == 1) { // 16-bit entry point
            scan += 2; // OBJECT
            scan += (1 * 2) * cnt; // (FLAGS + OFFSET) x cnt
            ordinal += cnt;
        }
        else if (typ == 3) { // 32-bit entry point
            scan += 2; // OBJECT
            scan += (1 * 4) * cnt; // (FLAGS + OFFSET) x cnt
            ordinal += cnt;
        }
        else {
            fprintf(stderr,"Entry point parsing: Unknown block cnt=%u typ=0x%02x\n",cnt,typ);
            break;
        }
    }

    if (ordinal == 0)
        return;

    t->table = (struct le_header_entry_table_entry*)malloc(sizeof(*(t->table)) * ordinal);
    if (t->table == NULL) return;
    t->length = ordinal;

    ordinal = 0;
    scan = base;
    while (scan < fence) {
        cnt = *scan++;
        if (cnt == 0 || scan >= fence) break;
        typ = *scan++;
        if (scan >= fence) break;

        if (typ == 0) {
            for (i=0;i < cnt && ordinal < t->length;i++) {
                ent = t->table + (ordinal++);
                ent->type = typ;
                ent->object = 0;
                ent->offset = (uint32_t)(scan - base);
            }
        }
        else if (typ == 1) {
            if ((scan+2) > fence) break;
            object = *((uint16_t*)scan); scan += 2;

            for (i=0;i < cnt && ordinal < t->length;i++) {
                if ((scan+1+2) > fence) break;

                ent = t->table + (ordinal++);
                ent->type = typ;
                ent->object = object;
                ent->offset = (uint32_t)(scan - base);

                scan++;             // flags
                scan += 2;          // offset
            }
        }
        else if (typ == 3) {
            if ((scan+2) > fence) break;
            object = *((uint16_t*)scan); scan += 2;

            for (i=0;i < cnt && ordinal < t->length;i++) {
                if ((scan+1+4) > fence) break;

                ent = t->table + (ordinal++);
                ent->type = typ;
                ent->object = object;
                ent->offset = (uint32_t)(scan - base);

                scan++;             // flags
                scan += 4;          // offset
            }
        }
        else {
            break;
        }
    }

    t->length = ordinal;
}

