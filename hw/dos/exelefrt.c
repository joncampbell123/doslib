
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

void le_header_fixup_record_table_init(struct le_header_fixup_record_table *t) {
    memset(t,0,sizeof(*t));
}

void le_header_fixup_record_table_free_raw(struct le_header_fixup_record_table *t) {
    if (t->raw) free(t->raw);
    t->raw_length = 0;
    t->raw = NULL;
}

void le_header_fixup_record_table_free_table(struct le_header_fixup_record_table *t) {
    if (t->table) free(t->table);
    t->table = NULL;
    t->length = 0;
    t->alloc = 0;
    t->raw_length_parsed = 0;
}

size_t le_header_fixup_record_table_get_raw_entry_length(struct le_header_fixup_record_table *t,const size_t i) {
    if (t->table == NULL || t->raw == NULL || t->raw_length == 0 || t->length == 0)
        return 0;
    if (i >= t->length)
        return 0;
    if ((i+1) == t->length)
        return t->raw_length_parsed - t->table[i];

    return t->table[i+1] - t->table[i];
}

unsigned char *le_header_fixup_record_table_get_raw_entry(struct le_header_fixup_record_table *t,const size_t i) {
    size_t o;

    if (t->table == NULL || t->raw == NULL || t->raw_length == 0 || t->length == 0)
        return NULL;
    if (i >= t->length)
        return NULL;

    o = t->table[i];
    if (o >= t->raw_length)
        return NULL;

    return t->raw + o;
}

unsigned char *le_header_fixup_record_table_alloc_raw(struct le_header_fixup_record_table *t,const size_t len) {
    le_header_fixup_record_table_free_raw(t);
    if (len == 0) return NULL;

    t->raw = malloc(len);
    if (t->raw == NULL) return NULL;
    t->raw_length = len;

    return t->raw;
}

void le_header_fixup_record_table_free(struct le_header_fixup_record_table *t) {
    le_header_fixup_record_table_free_table(t);
    le_header_fixup_record_table_free_raw(t);
}

void le_header_fixup_record_list_init(struct le_header_fixup_record_list *l) {
    memset(l,0,sizeof(*l));
}

void le_header_fixup_record_list_free(struct le_header_fixup_record_list *l) {
    size_t i;

    if (l->table) {
        for (i=0;i < l->length;i++) le_header_fixup_record_table_free(l->table+i);
        free(l->table);
    }

    l->table = NULL;
    l->length = 0;
}

int le_header_fixup_record_list_alloc(struct le_header_fixup_record_list *l,const size_t entries/*number_of_memory_pages*/) {
    size_t i;

    le_header_fixup_record_list_free(l);
    if (entries == 0) return -1;

    l->table = (struct le_header_fixup_record_table*)malloc(sizeof(*(l->table)) * entries);
    if (l->table == NULL) return -1;
    l->length = entries;

    for (i=0;i < entries;i++)
        le_header_fixup_record_table_init(l->table+i);

    return 0;
}

void le_header_fixup_record_table_parse(struct le_header_fixup_record_table *t) {
    unsigned char *base,*scan,*fence,*entry;
    unsigned char src,flags;
    uint8_t srcoff_count;

    /* NTS: srcoff is treated as a signed 16-bit integer for a reason.
     *      first, don't forget these relocations are defined per page,
     *      not overall the image. second, the LX specification says that,
     *      to handle relocations that span pages, a separate fixup
     *      record is specified for each page. From what I can see in
     *      Watcom 32-bit DOS programs, it just emits a fixup record
     *      on the next page with a negative offset.
     *
     *      Don't forget as a relocation relative to the page, positive
     *      values will generally not exceed 4096 (or whatever the page
     *      size) */

    le_header_fixup_record_table_free_table(t);
    if (t->raw == NULL || t->raw_length == 0) return;

    base = t->raw;
    fence = t->raw + t->raw_length;

    t->length = 0;
    t->alloc = 2048;
    t->table = (uint32_t*)malloc(sizeof(uint32_t) * t->alloc);
    if (t->table == NULL) return;

    scan = base;
    while (scan < fence) {
        entry = scan;
        src = *scan++;
        if (src == 0 || scan >= fence) break;
        flags = *scan++;

        if (src & 0xC0) { // bits [7:6]
            fprintf(stderr,"! Unknown source flags set, cannot continue (0x%02x)\n",src);
            break;
        }

        if (src & 0x20) {
            if (scan >= fence) break;
            srcoff_count = *scan++; //number of source offsets. object follows, then array of srcoff
        }
        else {
            srcoff_count = 1;
            scan += 2; // srcoff
        }

        if ((flags&3) == 0) { // internal reference
            if (flags&0x40)
                scan += 2; // object
            else
                scan += 1; // object

            if ((src&0xF) != 0x2) { /* not 16-bit selector fixup */
                if (flags&0x10)
                    scan += 4; // trgoff 32-bit target offset
                else // 16-bit target offset
                    scan += 2; // trgoff 16-bit target offset
            }
        }
        else {
            fprintf(stderr,"! Unknown flags type %u (0x%02x)\n",flags&3,flags);
            break;
        }

        if (src & 0x20)
            scan += 2 * srcoff_count;

        if (scan > fence) break;

        if (t->length >= t->alloc) {
            size_t nl = t->alloc + 2048;
            void *np;

            np = (void*)realloc((void*)t->table,nl * sizeof(uint32_t));
            if (np == NULL) break;

            t->alloc = nl;
            t->table = (uint32_t*)np;
        }

        assert(t->length < t->alloc);
        t->table[t->length++] = (uint32_t)(entry - base);
    }

    t->raw_length_parsed = (uint32_t)(scan - base);
}

void le_header_parseinfo_fixup_record_list_setup_prepare_from_page_table(struct le_header_parseinfo * const p) {
    if (p->le_fixup_page_table != NULL && p->le_fixup_records.table == NULL &&
        p->le_header.fixup_record_table_offset != 0 && p->le_header.number_of_memory_pages != 0 &&
        le_header_fixup_record_list_alloc(&p->le_fixup_records,p->le_header.number_of_memory_pages) == 0) {
        struct le_header_fixup_record_table *frtable;
        unsigned long ofs,sz;
        unsigned int i;

        // NTS: The le_fixup_page_table[] array is number_of_memory_pages+1 elements long
        assert(p->le_fixup_records.table != NULL);
        assert(p->le_fixup_records.length == p->le_header.number_of_memory_pages);
        for (i=0;i < p->le_header.number_of_memory_pages;i++) {
            frtable = p->le_fixup_records.table + i;

            if ((unsigned long)p->le_fixup_page_table[i+1] < (unsigned long)p->le_fixup_page_table[i])
                continue;

            ofs = (unsigned long)p->le_fixup_page_table[i] +
                (unsigned long)p->le_header.fixup_record_table_offset +
                (unsigned long)p->le_header_offset;
            sz = (unsigned long)p->le_fixup_page_table[i+1] -
                (unsigned long)p->le_fixup_page_table[i];

            frtable->file_offset = ofs;
            frtable->file_length = sz;
        }
    }
}

