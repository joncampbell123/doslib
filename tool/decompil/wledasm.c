/* NOTES:
 */

#include "minx86dec/types.h"
#include "minx86dec/state.h"
#include "minx86dec/opcodes.h"
#include "minx86dec/coreall.h"
#include "minx86dec/opcodes_str.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

#include <hw/dos/exehdr.h>

/* re-use a little code from the NE parser. */
#include <hw/dos/exenehdr.h>
#include <hw/dos/exenepar.h>
#include <hw/dos/exelehdr.h>
#include <hw/dos/exelepar.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

struct mod_symbol_table {
    char**                  table;      // ordinal i is entry i - 1
    size_t                  length;
    size_t                  alloc;
};

struct mod_symbols_list {
    struct mod_symbol_table*    table;
    size_t                      length;
};

struct dec_label {
    uint16_t                    seg_v;
    uint32_t                    ofs_v;
    char*                       name;
};

void cstr_free(char **l) {
    if (l != NULL) {
        if (*l != NULL) free(*l);
        *l = NULL;
    }
}

void cstr_copy(char **l,const char *s) {
    cstr_free(l);

    if (s != NULL) {
        const size_t len = strlen(s);
        *l = malloc(len+1);
        if (*l != NULL)
            strcpy(*l,s);
    }
}

void mod_symbol_table_free(struct mod_symbol_table *t) {
    unsigned int i;

    if (t->table) {
        for (i=0;i < t->length;i++) cstr_free(&(t->table[i]));
        free(t->table);
    }

    t->table = NULL;
    t->length = 0;
    t->alloc = 0;
}

void mod_symbols_list_free(struct mod_symbols_list *t) {
    unsigned int i;

    if (t->table) {
        for (i=0;i < t->length;i++) mod_symbol_table_free(&(t->table[i]));
        free(t->table);
    }

    t->table = NULL;
    t->length = 0;
}

void dec_label_set_name(struct dec_label *l,const char *s) {
    cstr_copy(&l->name,s);
}

struct dec_label*               dec_label = NULL;
size_t                          dec_label_count = 0;
size_t                          dec_label_alloc = 0;
unsigned long                   dec_ofs;
uint16_t                        dec_cs;

char                            name_tmp[255+1];

uint8_t                         dec_buffer[512];
uint8_t*                        dec_read;
uint8_t*                        dec_end;
char                            arg_c[101];
struct minx86dec_state          dec_st;
struct minx86dec_instruction    dec_i;
minx86_read_ptr_t               iptr;
uint16_t                        entry_cs,entry_ip;
uint16_t                        start_cs,start_ip;
uint32_t                        start_decom,end_decom,entry_ofs;
uint32_t                        current_offset;
unsigned char                   is_vxd = 0;

struct exe_dos_header           exehdr;

char*                           sym_file = NULL;
char*                           label_file = NULL;

char*                           src_file = NULL;
int                             src_fd = -1;

void dec_free_labels() {
    unsigned int i=0;

    if (dec_label == NULL)
        return;

    while (i < dec_label_count) {
        struct dec_label *l = dec_label + i;
        cstr_free(&(l->name));
        i++;
    }

    free(dec_label);
    dec_label = NULL;
}

uint32_t current_offset_minus_buffer() {
    return current_offset - (uint32_t)(dec_end - dec_buffer);
}

static void minx86dec_init_state(struct minx86dec_state *st) {
	memset(st,0,sizeof(*st));
}

static void minx86dec_set_buffer(struct minx86dec_state *st,uint8_t *buf,int sz) {
	st->fence = buf + sz;
	st->prefetch_fence = dec_buffer + sizeof(dec_buffer) - 16;
	st->read_ip = buf;
}

void help() {
    fprintf(stderr,"dosdasm [options]\n");
    fprintf(stderr,"MS-DOS COM/EXE/SYS decompiler\n");
    fprintf(stderr,"    -i <file>        File to decompile\n");
    fprintf(stderr,"    -lf <file>       Text file to define labels\n");
    fprintf(stderr,"    -sym <file>      Module symbols file\n");
}

void print_entry_table_locate_name_by_ordinal(const struct exe_ne_header_name_entry_table * const nonresnames,const struct exe_ne_header_name_entry_table *resnames,const unsigned int ordinal) {
    char tmp[255+1];
    unsigned int i;

    if (resnames->table != NULL) {
        for (i=0;i < resnames->length;i++) {
            struct exe_ne_header_name_entry *ent = resnames->table + i;

            if (ne_name_entry_get_ordinal(resnames,ent) == ordinal) {
                ne_name_entry_get_name(tmp,sizeof(tmp),resnames,ent);
                printf(" RESIDENT NAME '%s' ",tmp);
                return;
            }
        }
    }

    if (nonresnames->table != NULL) {
        for (i=0;i < nonresnames->length;i++) {
            struct exe_ne_header_name_entry *ent = nonresnames->table + i;

            if (ne_name_entry_get_ordinal(nonresnames,ent) == ordinal) {
                ne_name_entry_get_name(tmp,sizeof(tmp),nonresnames,ent);
                printf(" NONRESIDENT NAME '%s' ",tmp);
                return;
            }
        }
    }
}

int parse_argv(int argc,char **argv) {
    char *a;
    int i=1;

    while (i < argc) {
        a = argv[i++];

        if (*a == '-') {
            do { a++; } while (*a == '-');

            if (!strcmp(a,"i")) {
                src_file = argv[i++];
                if (src_file == NULL) return 1;
            }
            else if (!strcmp(a,"lf")) {
                label_file = argv[i++];
                if (label_file == NULL) return 1;
            }
            else if (!strcmp(a,"sym")) {
                sym_file = argv[i++];
                if (sym_file == NULL) return 1;
            }
            else if (!strcmp(a,"h") || !strcmp(a,"help")) {
                help();
                return 1;
            }
            else {
                fprintf(stderr,"Unknown switch %s\n",a);
                return 1;
            }
        }
        else {
            fprintf(stderr,"Unknown arg %s\n",a);
            return 1;
        }
    }

    if (src_file == NULL) {
        fprintf(stderr,"Must specify -i source file\n");
        return 1;
    }

    return 0;
}

void reset_buffer() {
    current_offset = 0;
    dec_read = dec_end = dec_buffer;
}

int refill(struct le_vmap_trackio * const io,struct le_header_parseinfo * const p) {
    const size_t flush = sizeof(dec_buffer) / 2;
    const size_t padding = 16;
    size_t dlen;

    if (dec_end > dec_read)
        dlen = (size_t)(dec_end - dec_read);
    else
        dlen = 0;

    if (dec_read >= (dec_buffer+flush)) {
        assert((dec_read+dlen) <= (dec_buffer+sizeof(dec_buffer)-padding));
        if (dlen != 0) memmove(dec_buffer,dec_read,dlen);
        dec_read = dec_buffer;
        dec_end = dec_buffer + dlen;
    }
    {
        unsigned char *e = dec_buffer + sizeof(dec_buffer) - padding;

        if (dec_end < e) {
            unsigned long clen = end_decom - current_offset;
            dlen = (size_t)(e - dec_end);
            if ((unsigned long)dlen > clen)
                dlen = (size_t)clen;

            if (dlen != 0) {
                int rd = le_trackio_read(dec_end,dlen,src_fd,io,p);
                if (rd > 0) {
                    dec_end += rd;
                    current_offset += (unsigned long)rd;
                }
            }
        }
    }

    return (dec_read < dec_end);
}

struct dec_label *dec_find_label(const uint16_t so,const uint32_t oo) {
    unsigned int i=0;

    if (dec_label == NULL)
        return NULL;

    while (i < dec_label_count) {
        struct dec_label *l = dec_label + i;

        if (l->seg_v == so && l->ofs_v == oo)
            return l;

        i++;
    }

    return NULL;
}

struct dec_label *dec_label_malloc() {
    if (dec_label == NULL)
        return NULL;

    if (dec_label_count >= dec_label_alloc)
        return NULL;

    return dec_label + (dec_label_count++);
}

int dec_label_qsortcb(const void *a,const void *b) {
    const struct dec_label *as = (const struct dec_label*)a;
    const struct dec_label *bs = (const struct dec_label*)b;

    if (as->seg_v < bs->seg_v)
        return -1;
    else if (as->seg_v > bs->seg_v)
        return 1;

    if (as->ofs_v < bs->ofs_v)
        return -1;
    else if (as->ofs_v > bs->ofs_v)
        return 1;

    return 0;
}

int ne_segment_relocs_table_qsort(const void *a,const void *b) {
    const union exe_ne_header_segment_relocation_entry *as =
        (const union exe_ne_header_segment_relocation_entry *)a;
    const union exe_ne_header_segment_relocation_entry *bs =
        (const union exe_ne_header_segment_relocation_entry *)b;

    if (as->r.seg_offset < bs->r.seg_offset)
        return -1;
    else if (as->r.seg_offset > bs->r.seg_offset)
        return 1;

    return 0;
}

void dec_label_sort() {
    if (dec_label == NULL || dec_label_count == 0)
        return;

    qsort(dec_label,dec_label_count,sizeof(*dec_label),dec_label_qsortcb);
}

int main(int argc,char **argv) {
    struct le_header_parseinfo le_parser;
    struct exe_le_header le_header;
    struct le_vmap_trackio io;
    uint32_t le_header_offset;
    struct dec_label *label;
    unsigned int labeli;
    uint32_t file_size;

    assert(sizeof(le_parser.le_header) == EXE_HEADER_LE_HEADER_SIZE);
    le_header_parseinfo_init(&le_parser);
    memset(&exehdr,0,sizeof(exehdr));

    if (parse_argv(argc,argv))
        return 1;

    assert(sizeof(exehdr) == 0x1C);

    dec_label_alloc = 4096;
    dec_label_count = 0;
    dec_label = malloc(sizeof(*dec_label) * dec_label_alloc);
    if (dec_label == NULL) {
        fprintf(stderr,"Failed to alloc label array\n");
        return 1;
    }
    memset(dec_label,0,sizeof(*dec_label) * dec_label_alloc);

    if (src_file == NULL) {
        fprintf(stderr,"No source file specified\n");
        return 1;
    }

    src_fd = open(src_file,O_RDONLY|O_BINARY);
    if (src_fd < 0) {
        fprintf(stderr,"Unable to open '%s', %s\n",src_file,strerror(errno));
        return 1;
    }

    file_size = lseek(src_fd,0,SEEK_END);
    lseek(src_fd,0,SEEK_SET);

    if (read(src_fd,&exehdr,sizeof(exehdr)) != (int)sizeof(exehdr)) {
        fprintf(stderr,"EXE header read error\n");
        return 1;
    }

    if (exehdr.magic != 0x5A4DU/*MZ*/) {
        fprintf(stderr,"EXE header signature missing\n");
        return 1;
    }

    if (!exe_header_can_contain_exe_extension(&exehdr)) {
        fprintf(stderr,"EXE header cannot contain extension\n");
        return 1;
    }

    /* go read the extension */
    if (lseek(src_fd,EXE_HEADER_EXTENSION_OFFSET,SEEK_SET) != EXE_HEADER_EXTENSION_OFFSET ||
        read(src_fd,&le_header_offset,4) != 4) {
        fprintf(stderr,"Cannot read extension\n");
        return 1;
    }
    if ((le_header_offset+EXE_HEADER_LE_HEADER_SIZE) >= file_size) {
        printf("! NE header not present (offset out of range)\n");
        return 0;
    }

    /* go read the extended header */
    if (lseek(src_fd,le_header_offset,SEEK_SET) != le_header_offset ||
        read(src_fd,&le_header,sizeof(le_header)) != sizeof(le_header)) {
        fprintf(stderr,"Cannot read LE header\n");
        return 1;
    }
    if (le_header.signature != EXE_LE_SIGNATURE &&
        le_header.signature != EXE_LX_SIGNATURE) {
        fprintf(stderr,"Not an LE/LX executable\n");
        return 1;
    }
    le_parser.le_header_offset = le_header_offset;
    le_parser.le_header = le_header;

    if (le_header.offset_of_object_table != 0 && le_header.object_table_entries != 0) {
        unsigned long ofs = le_header.offset_of_object_table + (unsigned long)le_parser.le_header_offset;
        unsigned char *base = le_header_parseinfo_alloc_object_table(&le_parser);
        size_t readlen = le_header_parseinfo_get_object_table_buffer_size(&le_parser);

        if ((unsigned long)lseek(src_fd,ofs,SEEK_SET) != ofs || (size_t)read(src_fd,base,readlen) != readlen)
            le_header_parseinfo_free_object_table(&le_parser);
    }

    if (le_header.object_page_map_offset != 0 && le_header.number_of_memory_pages != 0) {
        unsigned long ofs = le_header.object_page_map_offset + (unsigned long)le_parser.le_header_offset;
        unsigned char *base = le_header_parseinfo_alloc_object_page_map_table(&le_parser);
        size_t readlen = le_header_parseinfo_get_object_page_map_table_read_buffer_size(&le_parser);

        if ((unsigned long)lseek(src_fd,ofs,SEEK_SET) != ofs || (size_t)read(src_fd,base,readlen) != readlen)
            le_header_parseinfo_free_object_page_map_table(&le_parser);

        /* "finish" reading by having the library convert the data in-place */
        if (le_parser.le_object_page_map_table != NULL)
            le_header_parseinfo_finish_read_get_object_page_map_table(&le_parser);
    }

    if (le_header.fixup_page_table_offset != 0 && le_header.number_of_memory_pages != 0) {
        unsigned long ofs = le_header.fixup_page_table_offset + (unsigned long)le_header_offset;
        unsigned char *base = le_header_parseinfo_alloc_fixup_page_table(&le_parser);
        size_t readlen = le_header_parseinfo_get_fixup_page_table_buffer_size(&le_parser);

        if (base != NULL) {
            // NTS: This table has one extra entry, so that you can determine the size of each fixup record entry per segment
            //      by the difference between each entry. Entries in the fixup record table (and therefore the offsets in this
            //      table) numerically increase for this reason.
            if ((unsigned long)lseek(src_fd,ofs,SEEK_SET) != ofs || (size_t)read(src_fd,base,readlen) != (size_t)readlen)
                le_header_parseinfo_free_fixup_page_table(&le_parser);

            le_header_parseinfo_fixup_record_list_setup_prepare_from_page_table(&le_parser);
        }
    }

    if (le_parser.le_fixup_records.table != NULL && le_parser.le_fixup_records.length != 0) {
        struct le_header_fixup_record_table *frtable;
        unsigned char *base;
        unsigned int i;

        for (i=0;i < le_header.number_of_memory_pages;i++) {
            frtable = le_parser.le_fixup_records.table + i;

            if (frtable->file_length == 0) continue;

            base = le_header_fixup_record_table_alloc_raw(frtable,frtable->file_length);
            if (base == NULL) continue;

            if (!((unsigned long)lseek(src_fd,frtable->file_offset,SEEK_SET) == (unsigned long)frtable->file_offset &&
                (unsigned long)read(src_fd,base,frtable->file_length) == (unsigned long)frtable->file_length))
                le_header_fixup_record_table_free_raw(frtable);

            if (frtable->raw != NULL)
                le_header_fixup_record_table_parse(frtable);
        }
    }

    /* load resident name table */
    if (le_header.resident_names_table_offset != (uint32_t)0 && le_header.entry_table_offset != (uint32_t)0 &&
        le_header.resident_names_table_offset < le_header.entry_table_offset &&
        (unsigned long)lseek(src_fd,le_header.resident_names_table_offset + le_header_offset,SEEK_SET) == (le_header.resident_names_table_offset + le_header_offset)) {
        uint32_t sz = le_header.entry_table_offset - le_header.resident_names_table_offset;
        unsigned char *base;

        base = exe_ne_header_name_entry_table_alloc_raw(&le_parser.le_resident_names,sz);
        if (base != NULL) {
            if ((unsigned long)read(src_fd,base,sz) != sz)
                exe_ne_header_name_entry_table_free_raw(&le_parser.le_resident_names);
        }

        exe_ne_header_name_entry_table_parse_raw(&le_parser.le_resident_names);
    }

    /* load nonresident name table */
    if (le_header.nonresident_names_table_offset != (uint32_t)0 &&
        le_header.nonresident_names_table_length != (uint32_t)0 &&
        (unsigned long)lseek(src_fd,le_header.nonresident_names_table_offset,SEEK_SET) == le_header.nonresident_names_table_offset) {
        unsigned char *base;

        base = exe_ne_header_name_entry_table_alloc_raw(&le_parser.le_nonresident_names,le_header.nonresident_names_table_length);
        if (base != NULL) {
            if ((unsigned long)read(src_fd,base,le_header.nonresident_names_table_length) != le_header.nonresident_names_table_length)
                exe_ne_header_name_entry_table_free_raw(&le_parser.le_nonresident_names);
        }

        exe_ne_header_name_entry_table_parse_raw(&le_parser.le_nonresident_names);
    }

    if (le_header.entry_table_offset != (uint32_t)0) {
        unsigned long ofs = le_parser.le_header.entry_table_offset + le_parser.le_header_offset;
        uint32_t readlen = le_exe_header_entry_table_size(&le_parser.le_header);
        unsigned char *base = le_header_entry_table_alloc(&le_parser.le_entry_table,readlen);

        if (base != NULL) {
            // NTS: This table has one extra entry, so that you can determine the size of each fixup record entry per segment
            //      by the difference between each entry. Entries in the fixup record table (and therefore the offsets in this
            //      table) numerically increase for this reason.
            if ((unsigned long)lseek(src_fd,ofs,SEEK_SET) != ofs || (size_t)read(src_fd,base,readlen) != (size_t)readlen)
                le_header_entry_table_free(&le_parser.le_entry_table);
        }

        if (le_parser.le_entry_table.raw != NULL)
            le_header_entry_table_parse(&le_parser.le_entry_table);
    }

    if (le_header.initial_object_cs_number != 0) {
        if ((label=dec_label_malloc()) != NULL) {
            dec_label_set_name(label,"LE entry point");

            label->seg_v =
                le_header.initial_object_cs_number;
            label->ofs_v =
                le_header.initial_eip;
        }
    }

    if (le_parser.le_entry_table.table != NULL) {
        struct le_header_entry_table_entry *ent;
        unsigned char *raw;
        unsigned int i,mx;

        mx = le_parser.le_entry_table.length;
        for (i=0;i < mx;i++) {
            ent = le_parser.le_entry_table.table + i;
            raw = le_header_entry_table_get_raw_entry(&le_parser.le_entry_table,i); /* parser makes sure there is sufficient space for struct given type */
            if (raw == NULL) continue;

            if (ent->type == 2) {
                uint16_t offset;
                uint8_t flags;

                flags = *raw++;
                offset = *((uint16_t*)raw); raw += 2;
                assert(raw <= (le_parser.le_entry_table.raw+le_parser.le_entry_table.raw_length));

                if ((label=dec_label_malloc()) != NULL) {
                    char tmp[256];

                    sprintf(tmp,"Entry ordinal #%u",i+1);
                    dec_label_set_name(label,tmp);

                    label->seg_v =
                        ent->object;
                    label->ofs_v =
                        offset;
                }
            }
            else if (ent->type == 3) {
                uint32_t offset;
                uint8_t flags;

                flags = *raw++;
                offset = *((uint32_t*)raw); raw += 4;
                assert(raw <= (le_parser.le_entry_table.raw+le_parser.le_entry_table.raw_length));

                if ((label=dec_label_malloc()) != NULL) {
                    char tmp[256];

                    sprintf(tmp,"Entry ordinal #%u",i+1);
                    dec_label_set_name(label,tmp);

                    label->seg_v =
                        ent->object;
                    label->ofs_v =
                        offset;
                }
            }
        }
    }

    {
        /* if this is a Windows VXD, then we want to locate the DDB block for more fun */
        uint16_t object=0;
        uint32_t offset=0;

        if (le_parser_is_windows_vxd(&le_parser,&object,&offset)) {
            struct windows_vxd_ddb_win31 *ddb_31;
            unsigned char ddb[256];
            unsigned int i;
            char tmp[9];
            int rd;

            printf("* This appears to be a 32-bit Windows 386/VXD driver\n");
            printf("    VXD DDB block in Object #%u : 0x%08lx\n",
                (unsigned int)object,(unsigned long)offset);

            label = dec_find_label(object,offset);
            if (label != NULL) {
                label->seg_v = ~0;
                label->ofs_v = ~0;
                dec_label_set_name(label,"VXD DDB entry point");
            }

            if (le_segofs_to_trackio(&io,object,offset,&le_parser)) {
                uint32_t page_number = io.page_number; // remember this, we'll need it later

                printf("        File offset %lu (0x%lX) (page #%lu at %lu + page offset 0x%lX / 0x%lX)\n",
                        (unsigned long)io.file_ofs + (unsigned long)io.page_ofs,
                        (unsigned long)io.file_ofs + (unsigned long)io.page_ofs,
                        (unsigned long)io.page_number,
                        (unsigned long)io.file_ofs,
                        (unsigned long)io.page_ofs,
                        (unsigned long)io.page_size);

                // now read it
                rd = le_trackio_read(ddb,sizeof(ddb),src_fd,&io,&le_parser);
                if (rd >= (int)sizeof(*ddb_31)) {
                    ddb_31 = (struct windows_vxd_ddb_win31*)ddb;

                    /* wait---some VXDs have this structure in place but leave the fields empty (zeroed out) in the raw object data.
                     * it turns out that the fields are filled in by relocation (fixup) data. some VXDs will seem to have empty fields
                     * unless we apply relocations. */
                    if (le_parser.le_fixup_records.table != NULL && le_parser.le_fixup_records.length != 0 &&
                        le_parser.le_object_table != NULL && object > 0 && object <= le_header.object_table_entries) {
                        struct exe_le_header_object_table_entry *objent = le_parser.le_object_table + object - 1;
                        struct le_header_fixup_record_table *frtable;
                        unsigned int srcoff_count,srcoff_i;
                        unsigned char flags,src;
                        unsigned char *raw;
                        uint32_t soffset;
                        uint16_t srcoff;
                        unsigned int ti;
                        uint32_t page;

                        for (page=page_number;page <= io.page_number;page++) { // <- in case the DDB struct spans two pages
                            if (page > le_parser.le_fixup_records.length)
                                break;
                            if (page == 0)
                                continue;

                            frtable = le_parser.le_fixup_records.table + page - 1; // <- page numbers are 1-based
                            for (ti=0;ti < (unsigned int)frtable->length;ti++) {
                                raw = le_header_fixup_record_table_get_raw_entry(frtable,ti);
                                if (raw == NULL) continue;

                                // caller ensures the record is long enough
                                src = *raw++;
                                flags = *raw++;

                                if (src & 0xC0)
                                    continue;

                                if (src & 0x20) {
                                    srcoff_count = *raw++; //number of source offsets. object follows, then array of srcoff
                                }
                                else {
                                    srcoff_count = 1;
                                    srcoff = *((int16_t*)raw); raw += 2;
                                }

                                if ((flags&3) == 0) { // internal reference
                                    uint16_t tobject;
                                    uint32_t trgoff;

                                    if (flags&0x40) {
                                        tobject = *((uint16_t*)raw); raw += 2;
                                    }
                                    else {
                                        tobject = *raw++;
                                    }

                                    if ((src&0xF) != 0x2) { /* not 16-bit selector fixup */
                                        if (flags&0x10) { // 32-bit target offset
                                            trgoff = *((uint32_t*)raw); raw += 4;
                                        }
                                        else { // 16-bit target offset
                                            trgoff = *((uint16_t*)raw); raw += 2;
                                        }
                                    }
                                    else {
                                        trgoff = 0;
                                    }

                                    if (src & 0x20) {
                                        for (srcoff_i=0;srcoff_i < srcoff_count;srcoff_i++) {
                                            srcoff = *((int16_t*)raw); raw += 2;

                                            // what is the relocation relative to the struct we just read?
                                            soffset = ((uint32_t)srcoff +
                                                    (((uint32_t)page - (uint32_t)objent->page_map_index) * le_parser.le_header.memory_page_size)) - offset;

                                            // if it's within range, and in the same object, patch
                                            if ((soffset+4UL) <= (uint32_t)rd && tobject == object)
                                                *((uint32_t*)(ddb+soffset)) = trgoff;
                                        }
                                    }
                                    else {
                                        // what is the relocation relative to the struct we just read?
                                        soffset = ((uint32_t)srcoff +
                                                (((uint32_t)page - (uint32_t)objent->page_map_index) * le_parser.le_header.memory_page_size)) - offset;

                                        // if it's within range, and in the same object, patch
                                        if ((soffset+4UL) <= (uint32_t)rd && tobject == object)
                                            *((uint32_t*)(ddb+soffset)) = trgoff;
                                    }
                                }
                                else {
                                    continue;
                                }
                            }
                        }
                    }

                    printf("        Windows 386/VXD DDB structure:\n");
                    printf("            DDB_Next:               0x%08lX\n",(unsigned long)ddb_31->DDB_Next);
                    printf("            DDB_SDK_Version:        %u.%u (0x%04X)\n",
                            ddb_31->DDB_SDK_Version>>8,
                            ddb_31->DDB_SDK_Version&0xFFU,
                            ddb_31->DDB_SDK_Version);
                    printf("            DDB_Req_Device_Number:  0x%04x\n",ddb_31->DDB_Req_Device_Number);
                    printf("            DDB_Dev_*_Version:      %u.%u\n",ddb_31->DDB_Dev_Major_Version,ddb_31->DDB_Dev_Minor_Version);
                    printf("            DDB_Flags:              0x%04x\n",ddb_31->DDB_Flags);

                    memcpy(tmp,ddb_31->DDB_Name,8); tmp[8] = 0;
                    printf("            DDB_Name:               \"%s\"\n",tmp);

                    printf("            DDB_Init_Order:         0x%08lx\n",(unsigned long)ddb_31->DDB_Init_Order);
                    printf("            DDB_Control_Proc:       0x%08lx\n",(unsigned long)ddb_31->DDB_Control_Proc);
                    printf("            DDB_V86_API_Proc:       0x%08lx\n",(unsigned long)ddb_31->DDB_V86_API_Proc);
                    printf("            DDB_PM_API_Proc:        0x%08lx\n",(unsigned long)ddb_31->DDB_PM_API_Proc);
                    printf("            DDB_V86_API_CSIP:       %04X:%04X\n",
                            (unsigned int)(ddb_31->DDB_V86_API_CSIP >> 16UL),
                            (unsigned int)(ddb_31->DDB_V86_API_CSIP & 0xFFFFUL));
                    printf("            DDB_PM_API_CSIP:        %04X:%04X\n",
                            (unsigned int)(ddb_31->DDB_PM_API_CSIP >> 16UL),
                            (unsigned int)(ddb_31->DDB_PM_API_CSIP & 0xFFFFUL));
                    printf("            DDB_Reference_Data:     0x%08lx\n",(unsigned long)ddb_31->DDB_Reference_Data);
                    printf("            DDB_Service_Table_Ptr:  0x%08lx\n",(unsigned long)ddb_31->DDB_Service_Table_Ptr);
                    printf("            DDB_Service_Table_Size: 0x%08lx\n",(unsigned long)ddb_31->DDB_Service_Table_Size);

                    is_vxd = 1;

                    if (ddb_31->DDB_Control_Proc != 0) {
                        if ((label=dec_label_malloc()) != NULL) {
                            dec_label_set_name(label,"VXD DDB_Control_Proc");

                            label->seg_v =
                                object;
                            label->ofs_v =
                                ddb_31->DDB_Control_Proc;
                        }
                    }

                    if (ddb_31->DDB_V86_API_Proc != 0) {
                        if ((label=dec_label_malloc()) != NULL) {
                            dec_label_set_name(label,"VXD DDB_V86_API_Proc");

                            label->seg_v =
                                object;
                            label->ofs_v =
                                ddb_31->DDB_V86_API_Proc;
                        }
                    }

                    if (ddb_31->DDB_PM_API_Proc != 0) {
                        if ((label=dec_label_malloc()) != NULL) {
                            dec_label_set_name(label,"VXD DDB_PM_API_Proc");

                            label->seg_v =
                                object;
                            label->ofs_v =
                                ddb_31->DDB_PM_API_Proc;
                        }
                    }

                    // go dump the service table
                    // NTS: Some VXD drivers indicate a table size, but the offset (ptr) is zero. dumping from that location shows more zeros.
                    //      What is the meaning of that, exactly? Is it a clever way of defining the table such that the VXD can patch it with
                    //      entry points later?
                    //
                    //      Examples:
                    //        - Creative Sound Blaster 16 drivers for Windows 3.1 (VSBPD.386)
                    //        - Microsoft PCI VXD driver (pci.vxd)
                    if (ddb_31->DDB_Service_Table_Size != 0 && le_segofs_to_trackio(&io,object,ddb_31->DDB_Service_Table_Ptr,&le_parser)) {
                        uint32_t ptr;

                        printf("            DDB service table:\n");
                        for (i=0;i < (unsigned int)ddb_31->DDB_Service_Table_Size;i++) {
                            if (le_trackio_read((unsigned char*)(&ptr),sizeof(uint32_t),src_fd,&io,&le_parser) != sizeof(uint32_t))
                                break;

                            if (ddb_31->DDB_Service_Table_Ptr != 0) {
                                label = dec_find_label(object,ptr);
                                if (label == NULL) {
                                    if ((label=dec_label_malloc()) != NULL) {
                                        char tmp[256];

                                        sprintf(tmp,"VXD service call #0x%04x",i);
                                        dec_label_set_name(label,tmp);

                                        label->seg_v =
                                            object;
                                        label->ofs_v =
                                            ptr;
                                    }
                                }
                            }

                            printf("                0x%08lX\n",(unsigned long)ptr);
                        }
                    }
                }
            }
        }
    }
 
    /* first pass: decompilation */
    {
        struct exe_le_header_object_table_entry *ent;
        unsigned int inscount;
        unsigned int los = 0;

        while (los < dec_label_count) {
            label = dec_label + los;
            if (label->seg_v == 0 || label->seg_v > le_parser.le_header.object_table_entries) {
                los++;
                continue;
            }

            ent = le_parser.le_object_table + label->seg_v - 1;
            if (!(ent->object_flags & LE_HEADER_OBJECT_TABLE_ENTRY_FLAGS_EXECUTABLE)) {
                los++;
                continue;
            }

            if (!le_segofs_to_trackio(&io,label->seg_v,label->ofs_v,&le_parser)) {
                los++;
                continue;
            }

            inscount = 0;
            reset_buffer();
            dec_ofs = 0;
            entry_ip = 0;
            dec_cs = label->seg_v;
            start_decom = 0;
            entry_cs = dec_cs;
            current_offset = label->ofs_v;
            dec_read = dec_end = dec_buffer;
            end_decom = ent->virtual_segment_size;
            refill(&io,&le_parser);
            minx86dec_init_state(&dec_st);
            dec_st.data32 = dec_st.addr32 =
                (ent->object_flags & LE_HEADER_OBJECT_TABLE_ENTRY_FLAGS_386_BIG_DEFAULT) ? 1 : 0;

            printf("* NE segment #%d : 0x%04lx 1st pass from '%s'\n",
                (unsigned int)dec_cs,(unsigned long)label->ofs_v,label->name);

            do {
                uint32_t ofs = (uint32_t)(dec_read - dec_buffer) + current_offset_minus_buffer();
                uint32_t ip = ofs + entry_ip - dec_ofs;
                unsigned int c;

                if (!refill(&io,&le_parser)) break;

                minx86dec_set_buffer(&dec_st,dec_read,(int)(dec_end - dec_read));
                minx86dec_init_instruction(&dec_i);
                dec_st.ip_value = ip;
                minx86dec_decodeall(&dec_st,&dec_i);
                assert(dec_i.end >= dec_read);
                assert(dec_i.end <= (dec_buffer+sizeof(dec_buffer)));

                printf("%04lX:%04lX  ",(unsigned long)dec_cs,(unsigned long)dec_st.ip_value);

                {
                    unsigned char *e = dec_i.end;

                    // INT 20h VXDCALL
                    if (is_vxd && dec_i.opcode == MXOP_INT && dec_i.argc == 1 &&
                            dec_i.argv[0].regtype == MX86_RT_IMM && dec_i.argv[0].value == 0x20)
                        e += 2+2;

                    for (c=0,iptr=dec_i.start;iptr != e;c++)
                        printf("%02X ",*iptr++);
                }

                if (dec_i.rep != MX86_REP_NONE) {
                    for (;c < 6;c++)
                        printf("   ");

                    switch (dec_i.rep) {
                        case MX86_REPE:
                            printf("REP   ");
                            break;
                        case MX86_REPNE:
                            printf("REPNE ");
                            break;
                        default:
                            break;
                    };
                }
                else {
                    for (;c < 8;c++)
                        printf("   ");
                }

                // Special instruction:
                //   Windows VXDs use INT 20h followed by two WORDs to call other VXDs.
                if (is_vxd && dec_i.opcode == MXOP_INT && dec_i.argc == 1 &&
                        dec_i.argv[0].regtype == MX86_RT_IMM && dec_i.argv[0].value == 0x20) {
                    // INT 20h WORD, WORD
                    // the decompiler should have set the instruction pointer at the first WORD now.
                    uint16_t vxd_device,vxd_service;

                    vxd_service = *((uint16_t*)dec_i.end); dec_i.end += 2;
                    vxd_device = *((uint16_t*)dec_i.end); dec_i.end += 2;

                    printf("VxDCall  Device=0x%04X Service=0x%04X",
                            vxd_device,vxd_service);
                }
                else {
                    printf("%-8s ",opcode_string[dec_i.opcode]);

                    for (c=0;c < (unsigned int)dec_i.argc;) {
                        minx86dec_regprint(&dec_i.argv[c],arg_c);
                        printf("%s",arg_c);
                        if (++c < (unsigned int)dec_i.argc) printf(",");
                    }
                }
                if (dec_i.lock) printf("  ; LOCK#");
                printf("\n");

                dec_read = dec_i.end;

                if ((dec_i.opcode == MXOP_JMP || dec_i.opcode == MXOP_CALL || dec_i.opcode == MXOP_JCXZ ||
                    (dec_i.opcode >= MXOP_JO && dec_i.opcode <= MXOP_JG)) && dec_i.argc == 1 &&
                    dec_i.argv[0].regtype == MX86_RT_IMM) {
                    /* make a label of the target */
                    /* 1st arg is target offset */
                    uint32_t toffset = dec_i.argv[0].value;

                    printf("Target: 0x%04lx\n",(unsigned long)toffset);

                    label = dec_find_label(dec_cs,toffset);
                    if (label == NULL) {
                        if ((label=dec_label_malloc()) != NULL) {
                            if (dec_i.opcode == MXOP_JMP || dec_i.opcode == MXOP_JCXZ ||
                                    (dec_i.opcode >= MXOP_JO && dec_i.opcode <= MXOP_JG))
                                dec_label_set_name(label,"JMP target");
                            else if (dec_i.opcode == MXOP_CALL)
                                dec_label_set_name(label,"CALL target");

                            label->seg_v =
                                dec_cs;
                            label->ofs_v =
                                toffset;
                        }
                    }

                    if (dec_i.opcode == MXOP_JMP)
                        break;
                }
                else if (dec_i.opcode == MXOP_JMP)
                    break;
                else if (dec_i.opcode == MXOP_RET || dec_i.opcode == MXOP_RETF)
                    break;
                else if (dec_i.opcode == MXOP_CALL_FAR || dec_i.opcode == MXOP_JMP_FAR) {
                    if (dec_i.opcode == MXOP_JMP_FAR)
                        break;
                }

                if (++inscount >= 1024)
                    break;
            } while(1);

            los++;
        }
    }

    /* sort labels */
    dec_label_sort();

    {
        struct dec_label *label;
        unsigned int i;

        printf("* Labels (%u labels):\n",(unsigned int)dec_label_count);
        for (i=0;i < dec_label_count;i++) {
            label = dec_label + i;

            printf("    %04X:%08lX '%s'\n",
                (unsigned int)label->seg_v,
                (unsigned long)label->ofs_v,
                label->name);
        }
    }

    /* second pass decompiler */
    if (le_parser.le_object_table != NULL) {
        struct exe_le_header_object_table_entry *ent;
        unsigned int i;

        for (i=0;i < le_parser.le_header.object_table_entries;i++) {
            ent = le_parser.le_object_table + i;

            printf("* LE object #%u (%u-bit)\n",
                i + 1,
                (ent->object_flags & LE_HEADER_OBJECT_TABLE_ENTRY_FLAGS_386_BIG_DEFAULT) ? 32 : 16);
            if (!(ent->object_flags & LE_HEADER_OBJECT_TABLE_ENTRY_FLAGS_EXECUTABLE))
                continue;

            if (!le_segofs_to_trackio(&io,i + 1,0,&le_parser))
                continue;

            reset_buffer();
            labeli = 0;
            dec_ofs = 0;
            entry_ip = 0;
            dec_cs = i + 1;
            start_decom = 0;
            entry_cs = dec_cs;
            current_offset = 0;
            dec_read = dec_end = dec_buffer;
            end_decom = ent->virtual_segment_size;
            refill(&io,&le_parser);
            minx86dec_init_state(&dec_st);
            dec_st.data32 = dec_st.addr32 =
                (ent->object_flags & LE_HEADER_OBJECT_TABLE_ENTRY_FLAGS_386_BIG_DEFAULT) ? 1 : 0;

            do {
                uint32_t ofs = (uint32_t)(dec_read - dec_buffer) + current_offset_minus_buffer();
                uint32_t ip = ofs + entry_ip - dec_ofs;
                unsigned char dosek = 0;
                unsigned int c;

                while (labeli < dec_label_count) {
                    label = dec_label + labeli;
                    if (label->seg_v != dec_cs) {
                        labeli++;
                        continue;
                    }
                    if (ip < label->ofs_v)
                        break;

                    labeli++;
                    ip = label->ofs_v;
                    dec_cs = label->seg_v;

                    printf("Label '%s' at %04lx:%04lx\n",
                            label->name ? label->name : "",
                            (unsigned long)label->seg_v,
                            (unsigned long)label->ofs_v);

                    label = dec_label + labeli;
                    dosek = 1;
                }

                if (dosek) {
                    reset_buffer();
                    current_offset = ofs;
                    if (!le_segofs_to_trackio(&io,i + 1,ip,&le_parser))
                        break;
                }

                if (!refill(&io,&le_parser)) break;

                minx86dec_set_buffer(&dec_st,dec_read,(int)(dec_end - dec_read));
                minx86dec_init_instruction(&dec_i);
                dec_st.ip_value = ip;
                minx86dec_decodeall(&dec_st,&dec_i);
                assert(dec_i.end >= dec_read);
                assert(dec_i.end <= (dec_buffer+sizeof(dec_buffer)));

                printf("%04lX:%04lX  ",(unsigned long)dec_cs,(unsigned long)dec_st.ip_value);

                {
                    unsigned char *e = dec_i.end;

                    // INT 20h VXDCALL
                    if (is_vxd && dec_i.opcode == MXOP_INT && dec_i.argc == 1 &&
                        dec_i.argv[0].regtype == MX86_RT_IMM && dec_i.argv[0].value == 0x20)
                        e += 2+2;

                    for (c=0,iptr=dec_i.start;iptr != e;c++)
                        printf("%02X ",*iptr++);
                }

                if (dec_i.rep != MX86_REP_NONE) {
                    for (;c < 6;c++)
                        printf("   ");

                    switch (dec_i.rep) {
                        case MX86_REPE:
                            printf("REP   ");
                            break;
                        case MX86_REPNE:
                            printf("REPNE ");
                            break;
                        default:
                            break;
                    };
                }
                else {
                    for (;c < 8;c++)
                        printf("   ");
                }

                // Special instruction:
                //   Windows VXDs use INT 20h followed by two WORDs to call other VXDs.
                if (is_vxd && dec_i.opcode == MXOP_INT && dec_i.argc == 1 &&
                    dec_i.argv[0].regtype == MX86_RT_IMM && dec_i.argv[0].value == 0x20) {
                    // INT 20h WORD, WORD
                    // the decompiler should have set the instruction pointer at the first WORD now.
                    uint16_t vxd_device,vxd_service;

                    vxd_service = *((uint16_t*)dec_i.end); dec_i.end += 2;
                    vxd_device = *((uint16_t*)dec_i.end); dec_i.end += 2;

                    printf("VxDCall  Device=0x%04X Service=0x%04X",
                        vxd_device,vxd_service);
                }
                else {
                    printf("%-8s ",opcode_string[dec_i.opcode]);

                    for (c=0;c < (unsigned int)dec_i.argc;) {
                        minx86dec_regprint(&dec_i.argv[c],arg_c);
                        printf("%s",arg_c);
                        if (++c < (unsigned int)dec_i.argc) printf(",");
                    }
                }
                if (dec_i.lock) printf("  ; LOCK#");
                printf("\n");

                dec_read = dec_i.end;
            } while(1);

        }
    }

    le_header_parseinfo_free(&le_parser);
    dec_free_labels();
    close(src_fd);
    return 0;
}

