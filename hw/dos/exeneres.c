
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

void exe_ne_header_resource_table_get_string(char *dst,size_t dstmax,const struct exe_ne_header_resource_table_t * const t,const uint16_t ofs) {
    unsigned char len;
    unsigned char *d;

    dst[0] = 0;
    if (dstmax == 0 || t->raw == NULL) return;
    if (ofs >= t->raw_length) return;
    d = t->raw + ofs;
    len = *d++;
    if ((d+len) > (t->raw+t->raw_length)) return;
    if (len > (dstmax-1)) len = dstmax-1;
    dst[len] = 0;
    if (len != 0) memcpy(dst,d,len);
}

void exe_ne_header_resource_table_init(struct exe_ne_header_resource_table_t * const t) {
    memset(t,0,sizeof(*t));
}

void exe_ne_header_resource_table_free_resnames(struct exe_ne_header_resource_table_t * const t) {
    if (t->resnames) free(t->resnames);
    t->resnames = NULL;
}

void exe_ne_header_resource_table_free_typeinfo(struct exe_ne_header_resource_table_t * const t) {
    if (t->typeinfo) free(t->typeinfo);
    t->typeinfo = NULL;
}

void exe_ne_header_resource_table_free_raw(struct exe_ne_header_resource_table_t * const t) {
    if (t->raw && t->raw_ownership) free(t->raw);
    t->raw_length = 0;
    t->raw = NULL;
}

void exe_ne_header_resource_table_free(struct exe_ne_header_resource_table_t * const t) {
    exe_ne_header_resource_table_free_resnames(t);
    exe_ne_header_resource_table_free_typeinfo(t);
    exe_ne_header_resource_table_free_raw(t);
}

const struct exe_ne_header_resource_table_typeinfo *exe_ne_header_resource_table_get_typeinfo_entry(const struct exe_ne_header_resource_table_t * const t,const unsigned int idx) {
    if (t->raw == NULL || t->raw_length < sizeof(struct exe_ne_header_resource_table)) return NULL;
    if (t->typeinfo == NULL) return NULL;
    if (idx >= t->typeinfo_length) return NULL;
    return (const struct exe_ne_header_resource_table_typeinfo*)(t->raw + t->typeinfo[idx]);
}

const struct exe_ne_header_resource_table_nameinfo *exe_ne_header_resource_table_get_typeinfo_nameinfo_entry(const struct exe_ne_header_resource_table_typeinfo * const tinfo,const unsigned int idx) {
    if (idx >= tinfo->rtResourceCount) return NULL;
    return ((const struct exe_ne_header_resource_table_nameinfo*)((unsigned char*)tinfo + sizeof(*tinfo))) + idx;
}

uint16_t exe_ne_header_resource_table_get_resname(const struct exe_ne_header_resource_table_t * const t,const unsigned int idx) {
    if (t->raw == NULL || t->resnames == NULL) return (uint16_t)(~0U);
    if (idx >= t->resnames_length) return (uint16_t)(~0U);
    return t->resnames[idx];
}

void exe_ne_header_resource_table_parse(struct exe_ne_header_resource_table_t * const t) {
    unsigned char *scan,*fence,*p;
    unsigned int entries;

    exe_ne_header_resource_table_free_resnames(t);
    exe_ne_header_resource_table_free_typeinfo(t);
    if (t->raw == NULL || t->raw_length < sizeof(struct exe_ne_header_resource_table)) return;

    scan = t->raw;
    fence = scan + t->raw_length;

    /* step: struct exe_ne_header_resource_table */
    scan += sizeof(struct exe_ne_header_resource_table);
    if (scan >= fence) return;

    /* how many: struct exe_ne_header_resource_table_typeinfo */
    p = scan;
    entries = 0;
    while ((scan+2) <= fence) {
        struct exe_ne_header_resource_table_typeinfo *ti =
            (struct exe_ne_header_resource_table_typeinfo*)scan;

        if (ti->rtTypeID == 0) {
            scan += 2;  /* the last entry is rtTypeID == 0, 2 bytes only sizeof(rtTypeID) */
            break;
        }

        /* if rtTypeID != 0, then the entry is the full struct size */
        if ((scan+sizeof(*ti)) > fence) break;
        scan += sizeof(*ti);
        scan += sizeof(struct exe_ne_header_resource_table_nameinfo) * ti->rtResourceCount;
        entries++;
    }

    if (entries != 0) {
        t->typeinfo = (uint16_t*)malloc(entries * sizeof(uint16_t));
        if (t->typeinfo == NULL) return;
        t->typeinfo_length = entries;

        entries = 0;
        while ((p+2) <= fence) {
            struct exe_ne_header_resource_table_typeinfo *ti =
                (struct exe_ne_header_resource_table_typeinfo*)p;

            if (ti->rtTypeID == 0) {
                p += 2;  /* the last entry is rtTypeID == 0, 2 bytes only sizeof(rtTypeID) */
                break;
            }

            if (entries >= t->typeinfo_length)
                break;

            /* if rtTypeID != 0, then the entry is the full struct size */
            if ((p+sizeof(*ti)) > fence) break;
            t->typeinfo[entries] = (uint16_t)(p - t->raw);
            p += sizeof(*ti);
            p += sizeof(struct exe_ne_header_resource_table_nameinfo) * ti->rtResourceCount;
            entries++;
        }

        t->typeinfo_length = entries;
    }

    /* how many: resource names */
    p = scan;
    entries = 0;
    while (scan < fence) {
        unsigned char len = *scan++;
        if (len == 0) break;
        scan += len;
        entries++;
    }

    if (entries != 0) {
        t->resnames = (uint16_t*)malloc(entries * sizeof(uint16_t));
        if (t->resnames == NULL) return;
        t->resnames_length = entries;

        entries = 0;
        while (p < fence) {
            unsigned char len = *p++;
            if (len == 0) break;
            if (entries >= t->resnames_length) break;
            t->resnames[entries] = (uint16_t)((p - 1) - t->raw);
            p += len;
            entries++;
        }

        t->resnames_length = entries;
    }
}

unsigned char *exe_ne_header_resource_table_alloc_raw(struct exe_ne_header_resource_table_t * const t,const size_t length) {
    exe_ne_header_resource_table_free_raw(t);

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

