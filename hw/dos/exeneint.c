
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

void ne_imported_name_table_entry_get_name(char *dst,size_t dstmax,const struct exe_ne_header_imported_name_table * const t,const uint16_t offset) {
    unsigned char *raw;
    size_t cpy;

    dst[0] = 0;
    if (dstmax == 0) return;
    if (t->raw == NULL || t->raw_length == 0) return;
    if (offset >= t->raw_length) return;

    raw = t->raw + offset;
    cpy = *raw++;
    if ((raw+cpy) > (t->raw+t->raw_length)) return;
    if (cpy > (dstmax - 1)) cpy = dstmax - 1;

    dst[cpy] = 0;
    if (cpy != 0) memcpy(dst,raw,cpy);
}

void ne_imported_name_table_entry_get_module_ref_name(char *dst,size_t dstmax,const struct exe_ne_header_imported_name_table * const t,const uint16_t index) {
    if (index == 0 || t->module_ref_table == NULL || index > t->module_ref_table_length) {
        dst[0] = 0;
        return;
    }

    ne_imported_name_table_entry_get_name(dst,dstmax,t,t->module_ref_table[index-1]);
}

void exe_ne_header_imported_name_table_init(struct exe_ne_header_imported_name_table * const t) {
    memset(t,0,sizeof(*t));
}

void exe_ne_header_imported_name_table_free_module_ref_table(struct exe_ne_header_imported_name_table * const t) {
    if (t->module_ref_table) free(t->module_ref_table);
    t->module_ref_table = NULL;
    t->module_ref_table_length = 0;
}

uint16_t *exe_ne_header_imported_name_table_alloc_module_ref_table(struct exe_ne_header_imported_name_table * const t,const size_t entries) {
    exe_ne_header_imported_name_table_free_module_ref_table(t);

    if (entries == 0)
        return NULL;

    t->module_ref_table = (uint16_t*)malloc(entries * sizeof(uint16_t));
    if (t->module_ref_table == NULL)
        return NULL;

    t->module_ref_table_length = entries;
    return t->module_ref_table;
}

void exe_ne_header_imported_name_table_free_table(struct exe_ne_header_imported_name_table * const t) {
    if (t->table) free(t->table);
    t->table = NULL;
    t->length = 0;
}

void exe_ne_header_imported_name_table_free_raw(struct exe_ne_header_imported_name_table * const t) {
    if (t->raw && t->raw_ownership) free(t->raw);
    t->raw_length = 0;
    t->raw = 0;
}

unsigned char *exe_ne_header_imported_name_table_alloc_raw(struct exe_ne_header_imported_name_table * const t,const size_t length) {
    if (t->raw != NULL) {
        if (length == t->raw_length)
            return t->raw;

        exe_ne_header_imported_name_table_free_module_ref_table(t);
        exe_ne_header_imported_name_table_free_table(t);
        exe_ne_header_imported_name_table_free_raw(t);
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

void exe_ne_header_imported_name_table_free(struct exe_ne_header_imported_name_table * const t) {
    exe_ne_header_imported_name_table_free_module_ref_table(t);
    exe_ne_header_imported_name_table_free_table(t);
    exe_ne_header_imported_name_table_free_raw(t);
}

int exe_ne_header_imported_name_table_parse_raw(struct exe_ne_header_imported_name_table * const t) {
    unsigned char *scan,*base,*fence;
    unsigned int entries = 0;

    exe_ne_header_imported_name_table_free_table(t);
    if (t->raw == NULL || t->raw_length == 0)
        return 0;

    base = t->raw;
    fence = base + t->raw_length;

    /* how many entries? */
    for (scan=base;scan < fence;) {
        /* uint8_t         LENGTH
         * char[length]    TEXT */
        unsigned char len = *scan++; // LENGTH (0 does NOT mean stop)
        if ((scan+len) > fence) break;

        scan += len;
        entries++;
    }

    t->table = (uint16_t*)malloc(entries * sizeof(*(t->table)));
    if (t->table == NULL) {
        exe_ne_header_imported_name_table_free_table(t);
        return -1;
    }
    t->length = entries;

    /* now scan and index the entries */
    entries = 0;
    for (scan=base;scan < fence && entries < t->length;) {
        /* uint8_t         LENGTH
         * char[length]    TEXT */
        unsigned char len = *scan++; // LENGTH
        if ((scan+len) > fence) break;

        t->table[entries] = (uint16_t)((scan-1) - base);
        scan += len;    // TEXT
        entries++;
    }
    t->length = entries;
    return 0;
}

