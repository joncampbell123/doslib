/* NOTES:
 *
 * - If you want an example of a NE image that uses "OSFIXUP", the following files might be useful
 *
 *   MSVIDC.DRV               Windows 3.11 + Video For Windows              "Microsoft Video 1" codec
 *
 * - NE images apparently have 0x10 empty bytes in the first segment
 *
 * - Reading WINE source code reveals other ugly NE features that I have yet to see in the wild:
 *     * "Self loading" executables (ewww). There's a specific structure in segment #1 that
 *       points the OS into entry points to make this happen, we should decompile that someday.
 *
 *     * Iterated segments. My best guess from WINE's implementation someone at Microsoft copied
 *       the way "iterated segments" are formatted in the OMF object format and put it into the
 *       NE format.
 *
 * - Windows 3.11 OLE2.DLL has entry points that point into a code segment where there's no code,
 *   but various unknown data. It doesn't seem to have iterated segment data or use "self loading"
 *   so I can only guess that perhaps OleInitialize() does something funny like decompress
 *   subroutines into that code segment via a CS to DS alias. We'll see.
 *
 * - DISPDIB.DLL as expected appears to make some calls to KERNEL and USER along with some paths
 *   that call INT 10h directly. There are also plenty of INT 2F AF=0x4F calls in there as well.
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
#include <hw/dos/exenehdr.h>
#include <hw/dos/exenepar.h>

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
    uint16_t                    seg_v,ofs_v;
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

int refill() {
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
                int rd = read(src_fd,dec_end,dlen);
                if (rd > 0) {
                    dec_end += rd;
                    current_offset += (unsigned long)rd;
                }
            }
        }
    }

    return (dec_read < dec_end);
}

struct dec_label *dec_find_label(const uint16_t so,const uint16_t oo) {
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

const char *mod_symbols_list_lookup(
    const struct mod_symbols_list * const mod_syms,
    unsigned int mod_ref_idx,unsigned int mod_ordinal) {
    struct mod_symbol_table *tbl;

    if (mod_ref_idx == 0 || mod_ordinal == 0)
        return NULL;
    if (mod_syms == NULL)
        return NULL;
    if (mod_syms->length == 0)
        return NULL;
    if (mod_ref_idx > mod_syms->length)
        return NULL;

    tbl = mod_syms->table + mod_ref_idx - 1;
    if (tbl->table == NULL)
        return NULL;
    if (tbl->length == 0)
        return NULL;
    if (mod_ordinal > tbl->length)
        return NULL;

    return tbl->table[mod_ordinal - 1];
}
 
void print_segment_reloc_table(const struct exe_ne_header_segment_reloc_table * const r,struct exe_ne_header_imported_name_table * const ne_imported_name_table,const struct mod_symbols_list * const mod_syms) {
    const union exe_ne_header_segment_relocation_entry *relocent;
    unsigned int relent;
    char tmp2[255+1];
    char tmp[255+1];

    assert(sizeof(*relocent) == 8);
    for (relent=0;relent < r->length;relent++) {
        relocent = r->table + relent;

        printf("                At 0x%04x: ",relocent->r.seg_offset);
        switch (relocent->r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_MASK) {
            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_INTERNAL_REFERENCE:
                printf("Internal ref");
                break;
            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_ORDINAL:
                printf("Import by ordinal");
                break;
            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_NAME:
                printf("Import by name");
                break;
            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_OSFIXUP:
                printf("OSFIXUP");
                break;
        }
        if (relocent->r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_ADDITIVE)
            printf(" (ADDITIVE)");
        printf(" ");

        switch (relocent->r.reloc_address_type&EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_MASK) {
            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_OFFSET_LOBYTE:
                printf("addr=OFFSET_LOBYTE");
                break;
            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_SEGMENT:
                printf("addr=SEGMENT");
                break;
            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_FAR_POINTER:
                printf("addr=FAR_POINTER(16:16)");
                break;
            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_OFFSET:
                printf("addr=OFFSET");
                break;
            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_FAR48_POINTER:
                printf("addr=FAR_POINTER(16:32)");
                break;
            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_OFFSET32:
                printf("addr=OFFSET32");
                break;
            default:
                printf("addr=0x%02x",relocent->r.reloc_address_type);
                break;
        }
        printf("\n");

        switch (relocent->r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_MASK) {
            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_INTERNAL_REFERENCE:
                if (relocent->intref.segment_index == 0xFF) {
                    printf("                    Refers to movable segment, entry ordinal #%d\n",
                        relocent->movintref.entry_ordinal);
                }
                else {
                    printf("                    Refers to segment #%d : 0x%04x\n",
                        relocent->intref.segment_index,
                        relocent->intref.seg_offset);
                }
                break;
            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_ORDINAL:
                ne_imported_name_table_entry_get_module_ref_name(tmp,sizeof(tmp),ne_imported_name_table,relocent->ordinal.module_reference_index);
                printf("                    Refers to module reference #%d '%s', ordinal %d",
                    relocent->ordinal.module_reference_index,tmp,
                    relocent->ordinal.ordinal);
                {
                    const char *sym = mod_symbols_list_lookup(
                            mod_syms,
                            relocent->ordinal.module_reference_index,
                            relocent->ordinal.ordinal);
                    if (sym != NULL) printf(" '%s'",sym);
                }
                printf("\n");
                break;
            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_NAME:
                ne_imported_name_table_entry_get_name(tmp2,sizeof(tmp2),ne_imported_name_table,relocent->name.imported_name_offset);
                ne_imported_name_table_entry_get_module_ref_name(tmp,sizeof(tmp),ne_imported_name_table,relocent->name.module_reference_index);
                printf("                    Refers to module reference #%d '%s', imp name offset %d '%s'\n",
                    relocent->name.module_reference_index,tmp,
                    relocent->name.imported_name_offset,tmp2);
                break;
            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_OSFIXUP:
                printf("                    OSFIXUP type=0x%04x\n",
                    relocent->osfixup.fixup);
                break;
        }
    }
}

void get_entry_name_by_ordinal(char *tmp,size_t tmplen,const struct exe_ne_header_name_entry_table * const nonresnames,const struct exe_ne_header_name_entry_table *resnames,const unsigned int ordinal) {
    unsigned int i;

    tmp[0] = 0;
    if (tmplen <= 1) return;

    if (resnames->table != NULL) {
        for (i=0;i < resnames->length;i++) {
            struct exe_ne_header_name_entry *ent = resnames->table + i;

            if (ne_name_entry_get_ordinal(resnames,ent) == ordinal) {
                ne_name_entry_get_name(tmp,tmplen,resnames,ent);
                return;
            }
        }
    }

    if (nonresnames->table != NULL) {
        for (i=0;i < nonresnames->length;i++) {
            struct exe_ne_header_name_entry *ent = nonresnames->table + i;

            if (ne_name_entry_get_ordinal(nonresnames,ent) == ordinal) {
                ne_name_entry_get_name(tmp,tmplen,nonresnames,ent);
                return;
            }
        }
    }
}

void print_relocation_farptr(
    const struct exe_ne_header_imported_name_table *ne_imported_name_table,
    const struct exe_ne_header_entry_table_table *ne_entry_table,
    const struct exe_ne_header_name_entry_table *ne_nonresname,
    const struct exe_ne_header_name_entry_table *ne_resname,
    const union exe_ne_header_segment_relocation_entry *relocent,
    const struct mod_symbols_list * const mod_syms) {
    // caller has established relocation is 2-byte SEGMENT value.
    switch (relocent->r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_MASK) {
        case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_INTERNAL_REFERENCE:
            if (relocent->intref.segment_index == 0xFF) {
                get_entry_name_by_ordinal(name_tmp,sizeof(name_tmp),ne_nonresname,ne_resname,relocent->movintref.entry_ordinal);

                if (name_tmp[0] != 0)
                    printf("entry %s ordinal #%d",
                            name_tmp,relocent->movintref.entry_ordinal);
                else
                    printf("entry ordinal #%d",
                            relocent->movintref.entry_ordinal);

                /* ordinal is 1-based */
                if (relocent->movintref.entry_ordinal > 0 &&
                    relocent->movintref.entry_ordinal <= ne_entry_table->length) {
                    struct exe_ne_header_entry_table_entry *ent = ne_entry_table->table + relocent->movintref.entry_ordinal - 1;
                    unsigned char *rawd = exe_ne_header_entry_table_table_raw_entry(ne_entry_table,ent);
                    if (rawd != NULL) {
                        if (ent->segment_id == 0xFF) {
                            struct exe_ne_header_entry_table_movable_segment_entry *ment =
                                (struct exe_ne_header_entry_table_movable_segment_entry*)rawd;

                            printf(" -- segment #%d -- 0x%04x : 0x%04x",
                                    ment->segid,
                                    ment->segid,
                                    ment->seg_offs);
                        }
                        else {
                            /* NTS: raw_entry() function guarantees that the data available is large enough to hold this struct */
                            struct exe_ne_header_entry_table_fixed_segment_entry *fent =
                                (struct exe_ne_header_entry_table_fixed_segment_entry*)rawd;

                            printf(" -- segment #%d -- 0x%04x : 0x%04x",
                                    ent->segment_id,
                                    ent->segment_id,
                                    fent->v.seg_offs);
                        }
                    }
                }
            }
            else {
                printf("segment #%d : 0x%04X",
                        relocent->intref.segment_index,
                        relocent->intref.seg_offset);
            }
            break;
        case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_ORDINAL:
            ne_imported_name_table_entry_get_module_ref_name(name_tmp,sizeof(name_tmp),ne_imported_name_table,relocent->ordinal.module_reference_index);
            printf("module reference #%d '%s', ordinal %d",
                    relocent->ordinal.module_reference_index,name_tmp,
                    relocent->ordinal.ordinal);
            {
                const char *sym = mod_symbols_list_lookup(
                    mod_syms,
                    relocent->ordinal.module_reference_index,
                    relocent->ordinal.ordinal);
                if (sym != NULL) printf(" '%s'",sym);
            }
            break;
        case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_NAME:
            ne_imported_name_table_entry_get_module_ref_name(name_tmp,sizeof(name_tmp),ne_imported_name_table,relocent->ordinal.module_reference_index);
            printf("module reference #%d '%s', imp name offset %d",
                    relocent->name.module_reference_index,name_tmp,
                    relocent->name.imported_name_offset);

            ne_imported_name_table_entry_get_name(name_tmp,sizeof(name_tmp),ne_imported_name_table,relocent->name.imported_name_offset);
            if (name_tmp[0] != 0) printf(" '%s'",name_tmp);
            break;
        case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_OSFIXUP:
            printf("OSFIXUP type=0x%04x",
                    relocent->osfixup.fixup);
            break;
    }
}

void print_relocation_segment(
    const struct exe_ne_header_imported_name_table *ne_imported_name_table,
    const struct exe_ne_header_entry_table_table *ne_entry_table,
    const struct exe_ne_header_name_entry_table *ne_nonresname,
    const struct exe_ne_header_name_entry_table *ne_resname,
    const union exe_ne_header_segment_relocation_entry *relocent,
    const struct mod_symbols_list * const mod_syms) {
    // caller has established relocation is 2-byte SEGMENT value.
    switch (relocent->r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_MASK) {
        case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_INTERNAL_REFERENCE:
            if (relocent->intref.segment_index == 0xFF) {
                get_entry_name_by_ordinal(name_tmp,sizeof(name_tmp),ne_nonresname,ne_resname,relocent->movintref.entry_ordinal);

                if (name_tmp[0] != 0)
                    printf("segment of entry %s ordinal #%d",
                            name_tmp,relocent->movintref.entry_ordinal);
                else
                    printf("segment of entry ordinal #%d",
                            relocent->movintref.entry_ordinal);

                /* ordinal is 1-based */
                if (relocent->movintref.entry_ordinal > 0 &&
                    relocent->movintref.entry_ordinal <= ne_entry_table->length) {
                    struct exe_ne_header_entry_table_entry *ent = ne_entry_table->table + relocent->movintref.entry_ordinal - 1;
                    unsigned char *rawd = exe_ne_header_entry_table_table_raw_entry(ne_entry_table,ent);
                    if (rawd != NULL) {
                        if (ent->segment_id == 0xFF) {
                            struct exe_ne_header_entry_table_movable_segment_entry *ment =
                                (struct exe_ne_header_entry_table_movable_segment_entry*)rawd;

                            printf(" -- segment #%d -- 0x%04x",
                                    ment->segid,
                                    ment->segid);
                        }
                        else {
                            /* NTS: raw_entry() function guarantees that the data available is large enough to hold this struct */
                            printf(" -- segment #%d -- 0x%04x",
                                    ent->segment_id,
                                    ent->segment_id);
                        }
                    }
                }
            }
            else {
                printf("segment #%d=0x%04X",
                        relocent->intref.segment_index,
                        relocent->intref.segment_index);
            }
            break;
        case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_ORDINAL:
            ne_imported_name_table_entry_get_module_ref_name(name_tmp,sizeof(name_tmp),ne_imported_name_table,relocent->ordinal.module_reference_index);
            printf("segment of module reference #%d '%s', ordinal %d",
                    relocent->ordinal.module_reference_index,name_tmp,
                    relocent->ordinal.ordinal);
            {
                const char *sym = mod_symbols_list_lookup(
                    mod_syms,
                    relocent->ordinal.module_reference_index,
                    relocent->ordinal.ordinal);
                if (sym != NULL) printf(" '%s'",sym);
            }
            break;
        case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_NAME:
            ne_imported_name_table_entry_get_module_ref_name(name_tmp,sizeof(name_tmp),ne_imported_name_table,relocent->ordinal.module_reference_index);
            printf("segment of module reference #%d '%s', imp name offset %d",
                    relocent->name.module_reference_index,name_tmp,
                    relocent->name.imported_name_offset);
            break;
        case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_OSFIXUP:
            printf("segment of OSFIXUP type=0x%04x ???",
                    relocent->osfixup.fixup);
            break;
    }
}

void print_relocation_offset(
    const struct exe_ne_header_imported_name_table *ne_imported_name_table,
    const struct exe_ne_header_entry_table_table *ne_entry_table,
    const struct exe_ne_header_name_entry_table *ne_nonresname,
    const struct exe_ne_header_name_entry_table *ne_resname,
    const union exe_ne_header_segment_relocation_entry *relocent,
    const struct mod_symbols_list * const mod_syms) {
    // caller has established relocation is 2-byte SEGMENT value.
    switch (relocent->r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_MASK) {
        case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_INTERNAL_REFERENCE:
            if (relocent->intref.segment_index == 0xFF) {
                get_entry_name_by_ordinal(name_tmp,sizeof(name_tmp),ne_nonresname,ne_resname,relocent->movintref.entry_ordinal);

                if (name_tmp[0] != 0)
                    printf("offset of entry %s ordinal #%d",
                            name_tmp,relocent->movintref.entry_ordinal);
                else
                    printf("offset of entry ordinal #%d",
                            relocent->movintref.entry_ordinal);

                /* ordinal is 1-based */
                if (relocent->movintref.entry_ordinal > 0 &&
                    relocent->movintref.entry_ordinal <= ne_entry_table->length) {
                    struct exe_ne_header_entry_table_entry *ent = ne_entry_table->table + relocent->movintref.entry_ordinal - 1;
                    unsigned char *rawd = exe_ne_header_entry_table_table_raw_entry(ne_entry_table,ent);
                    if (rawd != NULL) {
                        if (ent->segment_id == 0xFF) {
                            struct exe_ne_header_entry_table_movable_segment_entry *ment =
                                (struct exe_ne_header_entry_table_movable_segment_entry*)rawd;

                            printf(" -- 0x%04x",
                                    ment->seg_offs);
                        }
                        else {
                            /* NTS: raw_entry() function guarantees that the data available is large enough to hold this struct */
                            struct exe_ne_header_entry_table_fixed_segment_entry *fent =
                                (struct exe_ne_header_entry_table_fixed_segment_entry*)rawd;

                            printf(" -- 0x%04x",
                                    fent->v.seg_offs);
                        }
                    }
                }
            }
            else {
                printf("NOTIMPL");
            }
            break;
        case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_ORDINAL:
            ne_imported_name_table_entry_get_module_ref_name(name_tmp,sizeof(name_tmp),ne_imported_name_table,relocent->ordinal.module_reference_index);
            printf("offset of module reference #%d '%s', ordinal %d",
                    relocent->ordinal.module_reference_index,name_tmp,
                    relocent->ordinal.ordinal);
            {
                const char *sym = mod_symbols_list_lookup(
                    mod_syms,
                    relocent->ordinal.module_reference_index,
                    relocent->ordinal.ordinal);
                if (sym != NULL) printf(" '%s'",sym);
            }
            break;
        case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_NAME:
            ne_imported_name_table_entry_get_module_ref_name(name_tmp,sizeof(name_tmp),ne_imported_name_table,relocent->ordinal.module_reference_index);
            printf("offset of module reference #%d '%s', imp name offset %d",
                    relocent->name.module_reference_index,name_tmp,
                    relocent->name.imported_name_offset);
            break;
        case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_OSFIXUP:
            printf("offset of OSFIXUP type=0x%04x ???",
                    relocent->osfixup.fixup);
            break;
    }
}

int main(int argc,char **argv) {
    struct exe_ne_header_segment_reloc_table *ne_segment_relocs = NULL;
    struct exe_ne_header_imported_name_table ne_imported_name_table;
    struct exe_ne_header_entry_table_table ne_entry_table;
    struct exe_ne_header_name_entry_table ne_nonresname;
    struct exe_ne_header_resource_table_t ne_resources;
    struct exe_ne_header_name_entry_table ne_resname;
    struct exe_ne_header_segment_table ne_segments;
    struct mod_symbols_list mod_syms;
    struct exe_ne_header ne_header;
    uint32_t ne_header_offset;
    struct dec_label *label;
    unsigned int segmenti;
    unsigned int reloci;
    unsigned int labeli;
    uint32_t file_size;
    int c;

    assert(sizeof(ne_header) == 0x40);
    memset(&exehdr,0,sizeof(exehdr));
    memset(&mod_syms,0,sizeof(mod_syms));
    exe_ne_header_segment_table_init(&ne_segments);
    exe_ne_header_resource_table_init(&ne_resources);
    exe_ne_header_name_entry_table_init(&ne_resname);
    exe_ne_header_name_entry_table_init(&ne_nonresname);
    exe_ne_header_entry_table_table_init(&ne_entry_table);
    exe_ne_header_imported_name_table_init(&ne_imported_name_table);

    if (parse_argv(argc,argv))
        return 1;

    dec_label_alloc = 4096;
    dec_label_count = 0;
    dec_label = malloc(sizeof(*dec_label) * dec_label_alloc);
    if (dec_label == NULL) {
        fprintf(stderr,"Failed to alloc label array\n");
        return 1;
    }
    memset(dec_label,0,sizeof(*dec_label) * dec_label_alloc);

    src_fd = open(src_file,O_RDONLY|O_BINARY);
    if (src_fd < 0) {
        fprintf(stderr,"Unable to open %s, %s\n",src_file,strerror(errno));
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
        read(src_fd,&ne_header_offset,4) != 4) {
        fprintf(stderr,"Cannot read extension\n");
        return 1;
    }
    printf("    EXE extension (if exists) at: %lu\n",(unsigned long)ne_header_offset);
    if ((ne_header_offset+EXE_HEADER_NE_HEADER_SIZE) >= file_size) {
        printf("! NE header not present (offset out of range)\n");
        return 0;
    }

    /* go read the extended header */
    if (lseek(src_fd,ne_header_offset,SEEK_SET) != ne_header_offset ||
        read(src_fd,&ne_header,sizeof(ne_header)) != sizeof(ne_header)) {
        fprintf(stderr,"Cannot read NE header\n");
        return 1;
    }
    if (ne_header.signature != EXE_NE_SIGNATURE) {
        fprintf(stderr,"Not an NE executable\n");
        return 1;
    }

    if (ne_header.nonresident_name_table_offset < (ne_header_offset + 0x40UL))
        printf("! WARNING: Non-resident name table offset too small (would overlap NE header)\n");
    else {
        unsigned long min_offset = ne_header.segment_table_offset + (sizeof(struct exe_ne_header_segment_entry) * ne_header.segment_table_entries);
        if (min_offset < ne_header.resource_table_offset)
            min_offset = ne_header.resource_table_offset;
        if (min_offset < ne_header.resident_name_table_offset)
            min_offset = ne_header.resident_name_table_offset;
        if (min_offset < (ne_header.module_reference_table_offset+(2UL * ne_header.module_reftable_entries)))
            min_offset = (ne_header.module_reference_table_offset+(2UL * ne_header.module_reftable_entries));
        if (min_offset < ne_header.imported_name_table_offset)
            min_offset = ne_header.imported_name_table_offset;
        if (min_offset < (ne_header.entry_table_offset+ne_header.entry_table_length))
            min_offset = (ne_header.entry_table_offset+ne_header.entry_table_length);

        min_offset += ne_header_offset;
        if (ne_header.nonresident_name_table_offset < min_offset) {
            printf("! WARNING: Non-resident name table offset overlaps NE resident tables (%lu < %lu)\n",
                (unsigned long)ne_header.nonresident_name_table_offset,min_offset);
        }
    }

    if (ne_header.segment_table_offset > ne_header.resource_table_offset)
        printf("! WARNING: segment table offset > resource table offset");
    if (ne_header.resource_table_offset > ne_header.resident_name_table_offset)
        printf("! WARNING: resource table offset > resident name table offset");
    if (ne_header.resident_name_table_offset > ne_header.module_reference_table_offset)
        printf("! WARNING: resident name table offset > module reference table offset");
    if (ne_header.module_reference_table_offset > ne_header.imported_name_table_offset)
        printf("! WARNING: module reference table offset > imported name table offset");
    if (ne_header.imported_name_table_offset > ne_header.entry_table_offset)
        printf("! WARNING: imported name table offset > entry table offset");

    /* load segment table */
    if (ne_header.segment_table_entries != 0 && ne_header.segment_table_offset != 0 &&
        (unsigned long)lseek(src_fd,ne_header.segment_table_offset + ne_header_offset,SEEK_SET) == ((unsigned long)ne_header.segment_table_offset + ne_header_offset)) {
        unsigned char *base;
        size_t rawlen;

        base = exe_ne_header_segment_table_alloc_table(&ne_segments,ne_header.segment_table_entries,ne_header.sector_shift);
        if (base != NULL) {
            rawlen = exe_ne_header_segment_table_size(&ne_segments);
            if (rawlen != 0) {
                if ((size_t)read(src_fd,base,rawlen) != rawlen) {
                    printf("    ! Unable to read segment table\n");
                    exe_ne_header_segment_table_free_table(&ne_segments);
                }
            }
        }
    }

    /* load nonresident name table */
    if (ne_header.nonresident_name_table_offset != 0 && ne_header.nonresident_name_table_length != 0 &&
        (unsigned long)lseek(src_fd,ne_header.nonresident_name_table_offset,SEEK_SET) == ne_header.nonresident_name_table_offset) {
        unsigned char *base;

        printf("  * Nonresident name table length: %u\n",ne_header.nonresident_name_table_length);
        base = exe_ne_header_name_entry_table_alloc_raw(&ne_nonresname,ne_header.nonresident_name_table_length);
        if (base != NULL) {
            if ((unsigned long)read(src_fd,base,ne_nonresname.raw_length) != ne_nonresname.raw_length)
                exe_ne_header_name_entry_table_free_raw(&ne_nonresname);
        }

        exe_ne_header_name_entry_table_parse_raw(&ne_nonresname);
    }

    /* load resident name table */
    if (ne_header.resident_name_table_offset != 0 && ne_header.module_reference_table_offset > ne_header.resident_name_table_offset &&
        (unsigned long)lseek(src_fd,ne_header.resident_name_table_offset + ne_header_offset,SEEK_SET) == (ne_header.resident_name_table_offset + ne_header_offset)) {
        unsigned int raw_length;
        unsigned char *base;

        /* RESIDENT_NAME_TABLE_SIZE = module_reference_table_offset - resident_name_table_offset */
        raw_length = (unsigned short)(ne_header.module_reference_table_offset - ne_header.resident_name_table_offset);
        printf("  * Resident name table length: %u\n",raw_length);

        base = exe_ne_header_name_entry_table_alloc_raw(&ne_resname,raw_length);
        if (base != NULL) {
            if ((unsigned long)read(src_fd,base,ne_resname.raw_length) != ne_resname.raw_length)
                exe_ne_header_name_entry_table_free_raw(&ne_resname);
        }

        exe_ne_header_name_entry_table_parse_raw(&ne_resname);
    }

    /* load imported name table */
    if (ne_header.imported_name_table_offset != 0 && ne_header.entry_table_offset > ne_header.imported_name_table_offset &&
        (unsigned long)lseek(src_fd,ne_header.imported_name_table_offset + ne_header_offset,SEEK_SET) == (ne_header.imported_name_table_offset + ne_header_offset)) {
        unsigned int raw_length;
        unsigned char *base;

        /* IMPORTED_NAME_TABLE_SIZE = entry_table_offset - imported_name_table_offset       (header does not report size of imported name table) */
        raw_length = (unsigned short)(ne_header.entry_table_offset - ne_header.imported_name_table_offset);
        printf("  * Imported name table length: %u\n",raw_length);

        base = exe_ne_header_imported_name_table_alloc_raw(&ne_imported_name_table,raw_length);
        if (base != NULL) {
            if ((unsigned long)read(src_fd,base,ne_imported_name_table.raw_length) != ne_imported_name_table.raw_length)
                exe_ne_header_imported_name_table_free_raw(&ne_imported_name_table);
        }

        exe_ne_header_imported_name_table_parse_raw(&ne_imported_name_table);
    }

    /* load module reference table */
    if (ne_header.module_reference_table_offset != 0 && ne_header.module_reftable_entries != 0 &&
        (unsigned long)lseek(src_fd,ne_header.module_reference_table_offset + ne_header_offset,SEEK_SET) == (ne_header.module_reference_table_offset + ne_header_offset)) {
        uint16_t *base;

        printf("  * Module reference table length: %u\n",ne_header.module_reftable_entries * 2);

        base = exe_ne_header_imported_name_table_alloc_module_ref_table(&ne_imported_name_table,ne_header.module_reftable_entries);
        if (base != NULL) {
            if ((unsigned long)read(src_fd,base,ne_imported_name_table.module_ref_table_length*sizeof(uint16_t)) !=
                (ne_imported_name_table.module_ref_table_length*sizeof(uint16_t)))
                exe_ne_header_imported_name_table_free_module_ref_table(&ne_imported_name_table);
        }
    }

    /* entry table */
    if (ne_header.entry_table_offset != 0 && ne_header.entry_table_length != 0 &&
        lseek(src_fd,ne_header.entry_table_offset + ne_header_offset,SEEK_SET) == (ne_header.entry_table_offset + ne_header_offset)) {
        unsigned char *base;

        base = exe_ne_header_entry_table_table_alloc_raw(&ne_entry_table,ne_header.entry_table_length);
        if (base != NULL) {
            if (read(src_fd,base,ne_header.entry_table_length) != ne_header.entry_table_length)
                exe_ne_header_entry_table_table_free_raw(&ne_entry_table);
        }

        exe_ne_header_entry_table_table_parse_raw(&ne_entry_table);
    }

    if (sym_file != NULL && ne_imported_name_table.length != 0 && ne_imported_name_table.module_ref_table_length != 0) {
        int current_module_index = -1;
        char *current_module = NULL;
        unsigned int i;

        assert(mod_syms.table == NULL && mod_syms.length == 0);
        mod_syms.length = ne_imported_name_table.module_ref_table_length;
        mod_syms.table = malloc(sizeof(*mod_syms.table) * mod_syms.length);
        if (mod_syms.table == NULL) return 1;
        memset(mod_syms.table,0,sizeof(*mod_syms.table) * mod_syms.length);

        FILE *fp = fopen(sym_file,"r");
        if (fp == NULL) {
            fprintf(stderr,"Failed to open sym file, %s\n",sym_file);
            return 1;
        }

        while (!feof(fp) && !ferror(fp)) {
            char *s;

            memset(dec_buffer,0,sizeof(dec_buffer));
            if (fgets((char*)dec_buffer,sizeof(dec_buffer)-1,fp) == NULL)
                break;

            s = (char*)dec_buffer;
            {
                char *e = s + strlen(s) - 1;
                while (e > (char*)dec_buffer && (*e == '\r' || *e == '\n')) *e-- = 0;
            }

            while (*s == ' ' || *s == '\t') s++;

            if (!strncasecmp(s,"module ",7)) {
                s += 7;
                while (*s == ' ' || *s == '\t') s++;

                i = 0;
                current_module_index = -1;
                cstr_copy(&current_module,s);
                while (i < ne_imported_name_table.module_ref_table_length) {
                    ne_imported_name_table_entry_get_module_ref_name(name_tmp,sizeof(name_tmp),
                        &ne_imported_name_table,i + 1);

                    if (name_tmp[0] != 0 && strcasecmp(name_tmp,current_module) == 0) {
                        current_module_index = i;
                        break;
                    }

                    i++;
                }

                if (current_module_index >= 0)
                    printf("Sym file parsing module index %u for '%s'\n",current_module_index + 1,current_module);
            }
            else if (current_module_index < 0) {
            }
            else if (!strncasecmp(s,"ordinal.",8)) {
                struct mod_symbol_table *tbl = mod_syms.table + current_module_index;
                unsigned int ordinal;

                s += 8;
                ordinal = (unsigned int)strtoul(s,&s,10);

                if (tbl->table == NULL) {
                    tbl->alloc = 256;
                    tbl->length = 0;
                    tbl->table = malloc(sizeof(*tbl->table) * tbl->alloc);
                    if (tbl->table == NULL) tbl->alloc = tbl->length = 0;
                }
                if (tbl->table != NULL) {
                    if (ordinal != 0 && ordinal <= 8192) {
                        size_t nl = (ordinal + 255 + 1) & (~255);
                        if (nl < tbl->alloc) nl = tbl->alloc;

                        if (tbl->alloc < nl) {
                            void *np = realloc((void*)tbl->table,sizeof(*tbl->table) * nl);
                            if (np == NULL) break;
                            tbl->table = np;
                            tbl->alloc = nl;
                        }

                        while (tbl->length <= (ordinal - 1))
                            tbl->table[tbl->length++] = NULL;

                        assert(tbl->length < tbl->alloc);
                        assert((ordinal - 1) < tbl->alloc);
                        assert((ordinal - 1) <= tbl->length);

                        if (!strncasecmp(s,".name=",6)) {
                            s += 6;

                            if (tbl->table[ordinal - 1] != NULL)
                                printf("* WARNING: symfile redefines ordinal %u in module %s. '%s' to '%s'\n",
                                    ordinal,current_module,
                                    tbl->table[ordinal - 1],
                                    s);

                            cstr_copy(&(tbl->table[ordinal - 1]),s);
                        }
                    }
                }
            }
        }

        cstr_free(&current_module);
        fclose(fp);
    }

    if (label_file != NULL) {
        FILE *fp = fopen(label_file,"r");
        if (fp == NULL) {
            fprintf(stderr,"Failed to open label file, %s\n",label_file);
            return 1;
        }

        while (!feof(fp) && !ferror(fp)) {
            char *s;

            memset(dec_buffer,0,sizeof(dec_buffer));
            if (fgets((char*)dec_buffer,sizeof(dec_buffer)-1,fp) == NULL)
                break;

            s = (char*)dec_buffer;
            {
                char *e = s + strlen(s) - 1;
                while (e > (char*)dec_buffer && (*e == '\r' || *e == '\n')) *e-- = 0;
            }

            while (*s == ' ') s++;
            if (*s == ';' || *s == '#') continue;

            // seg:off label
            if (isxdigit(*s)) {
                uint16_t so,oo;

                so = (uint16_t)strtoul(s,&s,16);
                if (*s == ':') {
                    s++;
                    oo = (uint16_t)strtoul(s,&s,16);
                    while (*s == '\t' || *s == ' ') s++;

                    if ((label=dec_label_malloc()) != NULL) {
                        dec_label_set_name(label,s);
                        label->seg_v =
                            so;
                        label->ofs_v =
                            oo;
                    }
                }
            }
        }

        fclose(fp);
    }

    printf("* Entry point %04X:%04X\n",
        ne_header.entry_cs,
        ne_header.entry_ip);

    // load and alloc relocations
    if (ne_segments.length != 0) {
        unsigned int i;

        ne_segment_relocs = malloc(sizeof(*ne_segment_relocs) * ne_segments.length);
        if (ne_segment_relocs != NULL) {
            for (i=0;i < ne_segments.length;i++)
                exe_ne_header_segment_reloc_table_init(&ne_segment_relocs[i]);
        }
    }

    if (ne_segment_relocs) {
        struct exe_ne_header_segment_entry *segent;
        unsigned long reloc_offset;
        uint16_t reloc_entries;
        unsigned char *base;
        unsigned int i,j;

        for (i=0;i < ne_segments.length;i++) {
            segent = ne_segments.table + i; /* C pointer math, becomes (char*)ne_segments + (i * sizeof(*ne_segments)) */
            reloc_offset = exe_ne_header_segment_table_get_relocation_table_offset(&ne_segments,segent);
            if (reloc_offset == 0) continue;

            /* at the start of the relocation struct, is a 16-bit WORD that indicates how many entries are there,
             * followed by an array of relocation entries. */
            if ((unsigned long)lseek(src_fd,reloc_offset,SEEK_SET) != reloc_offset || read(src_fd,&reloc_entries,2) != 2)
                continue;

            if (reloc_entries == 0) continue;

            base = exe_ne_header_segment_reloc_table_alloc_table(&ne_segment_relocs[i],reloc_entries);
            if (base == NULL) continue;

            {
                size_t rd = exe_ne_header_segment_reloc_table_size(&ne_segment_relocs[i]);
                if (rd == 0) continue;

                if ((size_t)read(src_fd,ne_segment_relocs[i].table,rd) != rd) {
                    exe_ne_header_segment_reloc_table_free(&ne_segment_relocs[i]);
                    continue;
                }
            }

            printf("* Segment #%u, %u relocations\n",i+1,ne_segment_relocs[i].length);

            /* OK, here's why I tend to be so cyncial about Microsoft formats and documentation.
             *
             * If you believe the MSDN documentation or any other documentation out there, all
             * you have to do is read this nice simple table and follow it to know where to
             * patch the segment with relocations.
             *
             * BUUUUUUT like most Microsoft formats it always turns out what they document is
             * not all that you need to know to implement it properly AND of course fails to
             * cover all relocation patch sites AND there's always some requisite undocumented
             * magic BS you have to do to actually implement properly.
             *
             * This is no exception.
             *
             * There is absolutely no mention of this in the MSDN documentation I have, nor
             * on any website covering the New Executable format, but according to what the
             * WINE developers have painfully dug out and implemented, the relocation table
             * entry doesn't cover all entry points. Instead, it points to the first CALL FAR
             * site affected by the relocation, and you're supposed to follow a linked list
             * using the WORD value you later patch inside the x86 instruction until that
             * WORD value is 0xFFFF to patch sites with the same entry point.
             *
             * So if you think that it's just random chance these relocation entries point
             * at something like this:
             *
             *    CALL FAR 0x0000:0xFFFF
             *
             * and sometimes something like this:
             *
             *    CALL FAR 0x0000:0x321
             *
             * That's why. In the example above, 0x321 would be the offset of another site
             * to patch with the same relocation. At that location, the WORD value (before
             * patching) would be the next link in the chain, or 0xFFFF to end the chain.
             *
             * For obvious reasons of course, this does not apply to additive relocations.
             *
             * That's a pretty crucial detail to leave out, Microsoft!
             *
             * I guess it does kind of make sense. If many points in the segment have the
             * same relocation why make duplicate entries in the table when you can use
             * the empty bytes of the x86 instruction you're patching to make a linked
             * list instead? Still, would have been nice to document! */
            {
                struct exe_ne_header_segment_reloc_table *reloc = &ne_segment_relocs[i];
                uint32_t segment_ofs = (uint32_t)segent->offset_in_segments << (uint32_t)ne_segments.sector_shift;
                union exe_ne_header_segment_relocation_entry *newlist = NULL;
                size_t newlist_count = 0,newlist_alloc = 0;

                assert(reloc->table != NULL);
                for (j=0;j < reloc->length;j++) {
                    union exe_ne_header_segment_relocation_entry *relocent = reloc->table + j;
                    unsigned long site = segment_ofs + relocent->r.seg_offset;
                    int patience = 32767; // to prevent runaway lists
                    uint16_t plink,nlink;

                    /* additive relocations are not subject to this linked list system.
                     * how could they? unlike non-additive relocations the contents of the instruction do matter! */
                    if (relocent->r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_ADDITIVE)
                        continue;

                    printf("    Relocation entry #%u scanning linked list at 0x%04x\n",j+1,relocent->r.seg_offset);
                    do {
                        plink = nlink;

                        if ((unsigned long)lseek(src_fd,site,SEEK_SET) != site)
                            break;
                        if ((size_t)read(src_fd,&nlink,sizeof(nlink)) != sizeof(nlink))
                            break;
                        if (nlink == 0xFFFF || nlink == plink) // end of the list
                            break;
                        if (--patience == 0) // in case of runaway lists by corruption
                            break;

                        printf("        Linked list to 0x%04x\n",nlink);
                        site = segment_ofs + nlink;

                        /* allocate new list entry */
                        if (newlist == NULL) {
                            newlist_count = 0;
                            newlist_alloc = 1024;
                            newlist = malloc(newlist_alloc * sizeof(*newlist));
                            if (newlist == NULL) break;
                        }
                        else if (newlist_count >= newlist_alloc) {
                            size_t nl = newlist_alloc + 4096;
                            void *np;

                            np = realloc((void*)newlist,nl * sizeof(*newlist));
                            if (np == NULL) break;
                            newlist = np;
                            newlist_alloc = nl;
                        }

                        assert(newlist != NULL);
                        assert(newlist_count < newlist_alloc);
                        newlist[newlist_count] = *relocent;
                        newlist[newlist_count].r.seg_offset = nlink;
                        newlist_count++;
                    } while (1);
                }

                if (newlist != NULL) {
                    size_t nl = newlist_count + reloc->length;
                    void *np;

                    np = realloc((void*)(reloc->table),nl * sizeof(*newlist));
                    if (np != NULL) {
                        reloc->table = np;
                        memcpy(reloc->table + reloc->length,newlist,newlist_count * sizeof(*newlist));
                        reloc->length += newlist_count;
                    }

                    free(newlist);
                }
            }

            if (ne_segment_relocs[i].length != 0)
                qsort(ne_segment_relocs[i].table,ne_segment_relocs[i].length,
                    sizeof(const union exe_ne_header_segment_relocation_entry *),
                    ne_segment_relocs_table_qsort);

            print_segment_reloc_table(&ne_segment_relocs[i],&ne_imported_name_table,&mod_syms);
        }
    }

    if (ne_header.entry_cs >= 1 && ne_header.entry_cs <= ne_segments.length) {
        if ((label=dec_label_malloc()) != NULL) {
            dec_label_set_name(label,"Entry point NE .EXE");
            label->seg_v =
                ne_header.entry_cs;
            label->ofs_v =
                ne_header.entry_ip;
        }
    }

    if (ne_entry_table.table != NULL && ne_entry_table.length != 0) {
        const struct exe_ne_header_entry_table_entry *ent;
        unsigned char *rawd;
        unsigned int i;

        for (i=0;i < ne_entry_table.length;i++) { /* NTS: ordinal value is i + 1, ordinals are 1-based */
            ent = ne_entry_table.table + i;
            rawd = exe_ne_header_entry_table_table_raw_entry(&ne_entry_table,ent);
            if (rawd == NULL) continue;
            if (ent->segment_id == 0x00) continue;

            get_entry_name_by_ordinal(name_tmp,sizeof(name_tmp),&ne_nonresname,&ne_resname,i + 1);

            if (name_tmp[0] != 0)
                snprintf((char*)dec_buffer,sizeof(dec_buffer),"Entry %s ordinal #%u",name_tmp,i + 1);
            else
                snprintf((char*)dec_buffer,sizeof(dec_buffer),"Entry ordinal #%u",i + 1);

            if (ent->segment_id == 0xFF) {
                /* NTS: raw_entry() function guarantees that the data available is large enough to hold this struct */
                struct exe_ne_header_entry_table_movable_segment_entry *ment =
                    (struct exe_ne_header_entry_table_movable_segment_entry*)rawd;

                if ((label=dec_label_malloc()) != NULL) {
                    dec_label_set_name(label,(char*)dec_buffer);
                    label->seg_v =
                        ment->segid;
                    label->ofs_v =
                        ment->seg_offs;
                }
            }
            else if (ent->segment_id == 0xFE) {
            }
            else {
                /* NTS: raw_entry() function guarantees that the data available is large enough to hold this struct */
                struct exe_ne_header_entry_table_fixed_segment_entry *fent =
                    (struct exe_ne_header_entry_table_fixed_segment_entry*)rawd;

                if ((label=dec_label_malloc()) != NULL) {
                    dec_label_set_name(label,(char*)dec_buffer);
                    label->seg_v =
                        ent->segment_id;
                    label->ofs_v =
                        fent->v.seg_offs;
                }
            }
        }
    }

    /* first pass: decompilation */
    {
        unsigned int los = 0;
        unsigned int inscount;

        while (los < dec_label_count) {
            label = dec_label + los;
            if (label->seg_v == 0) {
                los++;
                continue;
            }

            segmenti = label->seg_v - 1;
            {
                const struct exe_ne_header_segment_entry *segent = ne_segments.table + segmenti;
                uint32_t segment_ofs = (uint32_t)segent->offset_in_segments << (uint32_t)ne_segments.sector_shift;
                struct exe_ne_header_segment_reloc_table *reloc;
                uint32_t segment_sz;

                if (segent->offset_in_segments == 0) {
                    los++;
                    continue;
                }

                segment_sz =
                    (segent->length == 0 ? 0x10000UL : segent->length);
                dec_cs = segmenti + 1;
                dec_ofs = 0;

                if ((uint32_t)lseek(src_fd,segment_ofs + label->ofs_v,SEEK_SET) != (segment_ofs + label->ofs_v))
                    return 1;

                printf("* NE segment #%d (0x%lx bytes @0x%lx) 1st pass\n",
                        segmenti + 1,(unsigned long)segment_sz,(unsigned long)segment_ofs);

                reloci = 0;
                inscount = 0;
                entry_ip = 0;
                reset_buffer();
                entry_cs = dec_cs;
                current_offset = segment_ofs + label->ofs_v;
                end_decom = segment_ofs + segment_sz;
                minx86dec_init_state(&dec_st);
                dec_read = dec_end = dec_buffer;
                dec_st.data32 = dec_st.addr32 = 0;

                if (ne_segment_relocs)
                    reloc = &ne_segment_relocs[segmenti];
                else
                    reloc = NULL;

                if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_DATA) {
                }
                else {
                    do {
                        uint32_t ofs = (uint32_t)(dec_read - dec_buffer) + current_offset_minus_buffer() - segment_ofs;
                        uint32_t ip = ofs + entry_ip - dec_ofs;
                        size_t inslen;

                        if (!refill()) break;

                        minx86dec_set_buffer(&dec_st,dec_read,(int)(dec_end - dec_read));
                        minx86dec_init_instruction(&dec_i);
                        dec_st.ip_value = ip;
                        minx86dec_decodeall(&dec_st,&dec_i);
                        assert(dec_i.end >= dec_read);
                        assert(dec_i.end <= (dec_buffer+sizeof(dec_buffer)));
                        inslen = (size_t)(dec_i.end - dec_i.start);

                        if (reloc) {
                            while (reloci < reloc->length && reloc->table[reloci].r.seg_offset < ip)
                                reloci++;
                        }

                        printf("%04lX:%04lX @0x%08lX ",(unsigned long)dec_cs,(unsigned long)dec_st.ip_value,(unsigned long)(dec_read - dec_buffer) + current_offset_minus_buffer());
                        for (c=0,iptr=dec_i.start;iptr != dec_i.end;c++)
                            printf("%02X ",*iptr++);

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
                        printf("%-8s ",opcode_string[dec_i.opcode]);

                        for (c=0;c < dec_i.argc;) {
                            minx86dec_regprint(&dec_i.argv[c],arg_c);
                            printf("%s",arg_c);
                            if (++c < dec_i.argc) printf(",");
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
                            uint32_t noffset = segment_ofs + toffset;

                            printf("Target: 0x%04lx @0x%08lx\n",(unsigned long)toffset,(unsigned long)noffset);

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
                            if (reloc && reloci < reloc->length) {
                                const union exe_ne_header_segment_relocation_entry *relocent = reloc->table + reloci;
                                const uint32_t o = relocent->r.seg_offset;

                                if (o >= ip && o < (ip + inslen)) {
                                    if (relocent->r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_ADDITIVE) {
                                    }
                                    else {
                                        if ((relocent->r.reloc_address_type&EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_MASK) ==
                                            EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_SEGMENT) {
                                            if ((relocent->r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_MASK) ==
                                                EXE_NE_HEADER_SEGMENT_RELOC_TYPE_INTERNAL_REFERENCE) {
                                                if (dec_i.argv[0].segment == MX86_SEG_IMM && dec_i.argv[0].regtype == MX86_RT_IMM) {
                                                    /* we can and should track internal references.
                                                     * do not track movable entry ordinal refs, because we already added those entry points to the label list */
                                                    if (relocent->intref.segment_index != 0xFF) {
                                                        label = dec_find_label(relocent->intref.segment_index,dec_i.argv[0].value);
                                                        if (label == NULL) {
                                                            if ((label=dec_label_malloc()) != NULL) {
                                                                if (dec_i.opcode == MXOP_JMP_FAR)
                                                                    dec_label_set_name(label,"JMP FAR target");
                                                                else if (dec_i.opcode == MXOP_CALL_FAR)
                                                                    dec_label_set_name(label,"CALL FAR target");

                                                                label->seg_v =
                                                                    relocent->intref.segment_index;
                                                                label->ofs_v =
                                                                    dec_i.argv[0].value;

                                                                printf("Target: 0x%04lx:0x%04lx internal ref, relocation by segment value\n",
                                                                    (unsigned long)label->seg_v,
                                                                    (unsigned long)label->ofs_v);
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            if (dec_i.opcode == MXOP_JMP_FAR)
                                break;
                        }

                        if (++inscount >= 1024)
                            break;
                    } while(1);
                }
            }

            los++;
        }
    }

    /* sort labels */
    dec_label_sort();

    /* second pass: decompilation */
    for (segmenti=0;segmenti < ne_segments.length;segmenti++) {
        const struct exe_ne_header_segment_entry *segent = ne_segments.table + segmenti;
        uint32_t segment_ofs = (uint32_t)segent->offset_in_segments << (uint32_t)ne_segments.sector_shift;
        struct exe_ne_header_segment_reloc_table *reloc;
        uint32_t segment_sz;

        if (segent->offset_in_segments == 0)
            continue;

        segment_sz =
            (segent->length == 0 ? 0x10000UL : segent->length);
        dec_cs = segmenti + 1;
        dec_ofs = 0;

        if ((uint32_t)lseek(src_fd,segment_ofs,SEEK_SET) != segment_ofs)
            return 1;

        printf("* NE segment #%d (0x%lx bytes @0x%lx)\n",
            segmenti + 1,(unsigned long)segment_sz,(unsigned long)segment_ofs);

        labeli = 0;
        reloci = 0;
        entry_ip = 0;
        reset_buffer();
        entry_cs = dec_cs;
        current_offset = segment_ofs;
        end_decom = segment_ofs + segment_sz;
        minx86dec_init_state(&dec_st);
        dec_read = dec_end = dec_buffer;
        dec_st.data32 = dec_st.addr32 = 0;

        if (ne_segment_relocs)
            reloc = &ne_segment_relocs[segmenti];
        else
            reloc = NULL;

        if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_DATA) {
            unsigned int col = 0;

            do {
                uint32_t ofs = (uint32_t)(dec_read - dec_buffer) + current_offset_minus_buffer() - segment_ofs;
                uint32_t ip = ofs + entry_ip - dec_ofs;

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
                    ofs = segment_ofs + ip;

                    if (col != 0) {
                        printf("\n");
                        col = 0;
                    }

                    printf("Label '%s' at %04lx:%04lx @0x%08lx\n",
                            label->name ? label->name : "",
                            (unsigned long)label->seg_v,
                            (unsigned long)label->ofs_v,
                            (unsigned long)ofs);

                    label = dec_label + labeli;
                }

                if (!refill()) break;

                if (reloc) {
                    while (reloci < reloc->length && reloc->table[reloci].r.seg_offset < ip)
                        reloci++;
                }

                /* if any part of the instruction is affected by EXE relocations, say so */
                if (reloc && reloci < reloc->length) {
                    const union exe_ne_header_segment_relocation_entry *relocent = reloc->table + reloci;
                    const uint32_t o = relocent->r.seg_offset;

                    if (o == ip) {
                        if (col != 0) {
                            printf("\n");
                            col = 0;
                        }
                        printf("%04lX:%04lX @0x%08lX ",
                                (unsigned long)dec_cs,
                                (unsigned long)ip,
                                (unsigned long)(dec_read - dec_buffer) + current_offset_minus_buffer());

                        printf(" <--- EXE relocation ");

                        switch (relocent->r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_MASK) {
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_INTERNAL_REFERENCE:
                                printf("Internal ref");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_ORDINAL:
                                printf("Import by ordinal");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_NAME:
                                printf("Import by name");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_OSFIXUP:
                                printf("OSFIXUP");
                                break;
                        }
                        if (relocent->r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_ADDITIVE)
                            printf(" (ADDITIVE)");
                        printf(" ");

                        switch (relocent->r.reloc_address_type&EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_MASK) {
                            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_OFFSET_LOBYTE:
                                printf("addr=OFFSET_LOBYTE");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_SEGMENT:
                                printf("addr=SEGMENT");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_FAR_POINTER:
                                printf("addr=FAR_POINTER(16:16)");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_OFFSET:
                                printf("addr=OFFSET");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_FAR48_POINTER:
                                printf("addr=FAR_POINTER(16:32)");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_OFFSET32:
                                printf("addr=OFFSET32");
                                break;
                            default:
                                printf("addr=0x%02x",relocent->r.reloc_address_type);
                                break;
                        }
                        printf("\n");

                        switch (relocent->r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_MASK) {
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_INTERNAL_REFERENCE:
                                if (relocent->intref.segment_index == 0xFF) {
                                    get_entry_name_by_ordinal(name_tmp,sizeof(name_tmp),&ne_nonresname,&ne_resname,relocent->movintref.entry_ordinal);

                                    printf("                    Refers to movable segment, entry ordinal #%d",
                                            relocent->movintref.entry_ordinal);
                                    if (name_tmp[0] != 0)
                                        printf(" '%s'",name_tmp);
                                    printf("\n");
                                }
                                else {
                                    printf("                    Refers to segment #%d : 0x%04x\n",
                                            relocent->intref.segment_index,
                                            relocent->intref.seg_offset);
                                }
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_ORDINAL:
                                ne_imported_name_table_entry_get_module_ref_name(name_tmp,sizeof(name_tmp),&ne_imported_name_table,relocent->ordinal.module_reference_index);
                                printf("                    Refers to module reference #%d '%s', ordinal %d",
                                        relocent->ordinal.module_reference_index,name_tmp,
                                        relocent->ordinal.ordinal);
                                {
                                    const char *sym = mod_symbols_list_lookup(
                                        &mod_syms,
                                        relocent->ordinal.module_reference_index,
                                        relocent->ordinal.ordinal);
                                    if (sym != NULL) printf(" '%s'",sym);
                                }
                                printf("\n");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_NAME:
                                ne_imported_name_table_entry_get_module_ref_name(name_tmp,sizeof(name_tmp),&ne_imported_name_table,relocent->ordinal.module_reference_index);
                                printf("                    Refers to module reference #%d '%s', imp name offset %d",
                                        relocent->name.module_reference_index,name_tmp,
                                        relocent->name.imported_name_offset);

                                ne_imported_name_table_entry_get_name(name_tmp,sizeof(name_tmp),&ne_imported_name_table,relocent->name.imported_name_offset);
                                printf(" '%s'\n",
                                        name_tmp);
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_OSFIXUP:
                                printf("                    OSFIXUP type=0x%04x\n",
                                        relocent->osfixup.fixup);
                                break;
                        }

                        col = 0;
                        printf("                    at +%u bytes (%04x:%04x)\n",
                                (unsigned int)(o - ip),
                                (unsigned int)dec_cs,
                                (unsigned int)dec_st.ip_value + (unsigned int)(o - ip));
                    }
                }

                if (col == 0) {
                    printf("%04lX:%04lX @0x%08lX ",
                        (unsigned long)dec_cs,
                        (unsigned long)ip,
                        (unsigned long)(dec_read - dec_buffer) + current_offset_minus_buffer());
                }

                while (col < ((unsigned int)(ip & 0xF))) {
                    printf("   ");
                    col++;
                }

                assert(dec_read < dec_end);
                printf("%02X ",*dec_read++);
                col++;

                if (col >= 16) {
                    printf("\n");
                    col = 0;
                }
            } while(1);

            if (col != 0) {
                printf("\n");
                col = 0;
            }
        }
        else {
            do {
                uint32_t ofs = (uint32_t)(dec_read - dec_buffer) + current_offset_minus_buffer() - segment_ofs;
                uint32_t ip = ofs + entry_ip - dec_ofs;
                unsigned char reloc_ann = 0;
                unsigned char dosek = 0;
                size_t inslen;

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
                    ofs = segment_ofs + ip;

                    printf("Label '%s' at %04lx:%04lx @0x%08lx\n",
                            label->name ? label->name : "",
                            (unsigned long)label->seg_v,
                            (unsigned long)label->ofs_v,
                            (unsigned long)ofs);

                    label = dec_label + labeli;
                    dosek = 1;
                }

                if (dosek) {
                    reset_buffer();
                    current_offset = ofs;
                    if ((uint32_t)lseek(src_fd,current_offset,SEEK_SET) != current_offset)
                        return 1;
                }

                if (!refill()) break;

                minx86dec_set_buffer(&dec_st,dec_read,(int)(dec_end - dec_read));
                minx86dec_init_instruction(&dec_i);
                dec_st.ip_value = ip;
                minx86dec_decodeall(&dec_st,&dec_i);
                assert(dec_i.end >= dec_read);
                assert(dec_i.end <= (dec_buffer+sizeof(dec_buffer)));
                inslen = (size_t)(dec_i.end - dec_i.start);

                if (reloc) {
                    while (reloci < reloc->length && reloc->table[reloci].r.seg_offset < ip)
                        reloci++;
                }

                printf("%04lX:%04lX @0x%08lX ",(unsigned long)dec_cs,(unsigned long)dec_st.ip_value,(unsigned long)(dec_read - dec_buffer) + current_offset_minus_buffer());
                for (c=0,iptr=dec_i.start;iptr != dec_i.end;c++)
                    printf("%02X ",*iptr++);

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
                printf("%-8s ",opcode_string[dec_i.opcode]);

                if (reloc && reloci < reloc->length) {
                    const union exe_ne_header_segment_relocation_entry *relocent = reloc->table + reloci;
                    const uint32_t o = relocent->r.seg_offset;

                    if (o >= ip && o < (ip + inslen)) {
                        if ((dec_i.opcode == MXOP_JMP_FAR || dec_i.opcode == MXOP_CALL_FAR) && dec_i.argc == 1 &&
                            dec_i.argv[0].segment == MX86_SEG_IMM && dec_i.argv[0].regtype == MX86_RT_IMM) {

                            switch (relocent->r.reloc_address_type&EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_MASK) {
                                case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_SEGMENT:
                                    if (o == (ip + 1 + 2)) { // CALL/JMP FAR segment relocation affecting segment portion
                                        printf("<");
                                        print_relocation_segment(&ne_imported_name_table,&ne_entry_table,&ne_nonresname,&ne_resname,relocent,&mod_syms);
                                        if (relocent->r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_ADDITIVE) {
                                            printf(" + 0x%04x>:0x%04x",
                                                (unsigned int)dec_i.argv[0].segval,
                                                (unsigned int)dec_i.argv[0].value);
                                        }
                                        else {
                                            printf(">:0x%04x",
                                                (unsigned int)dec_i.argv[0].value);
                                        }

                                        reloc_ann = 1;
                                    }
                                    break;
                                case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_FAR_POINTER:
                                    if (o == (ip + 1)) {
                                        if (!(relocent->r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_ADDITIVE)) {
                                            printf("<");
                                            print_relocation_farptr(&ne_imported_name_table,&ne_entry_table,&ne_nonresname,&ne_resname,relocent,&mod_syms);
                                            printf(">");
                                        }

                                        reloc_ann = 1;
                                    }
                                    break;
                            };
                        }
                        if ((dec_i.opcode == MXOP_PUSH) && dec_i.argc == 1 &&
                            dec_i.argv[0].regtype == MX86_RT_IMM) {

                            switch (relocent->r.reloc_address_type&EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_MASK) {
                                case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_SEGMENT:
                                    {
                                        printf("<");
                                        print_relocation_segment(&ne_imported_name_table,&ne_entry_table,&ne_nonresname,&ne_resname,relocent,&mod_syms);
                                        if (relocent->r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_ADDITIVE) {
                                            printf(" + 0x%04x>",
                                                (unsigned int)dec_i.argv[0].value);
                                        }
                                        else {
                                            printf(">");
                                        }
                                    }
                                    break;
                                case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_OFFSET:
                                    {
                                        printf("<");
                                        print_relocation_offset(&ne_imported_name_table,&ne_entry_table,&ne_nonresname,&ne_resname,relocent,&mod_syms);
                                        if (relocent->r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_ADDITIVE) {
                                            printf(" + 0x%04x>",
                                                (unsigned int)dec_i.argv[0].value);
                                        }
                                        else {
                                            printf(">");
                                        }
                                    }
                                    break;
                            };

                            reloc_ann = 1;
                        }
                        if ((dec_i.opcode == MXOP_MOV || dec_i.opcode == MXOP_ADD || dec_i.opcode == MXOP_SUB ||
                             dec_i.opcode == MXOP_CMP || dec_i.opcode == MXOP_XOR) && dec_i.argc == 2 &&
                            dec_i.argv[1].regtype == MX86_RT_IMM) {

                            for (c=0;c < 1;) {
                                minx86dec_regprint(&dec_i.argv[c],arg_c);
                                printf("%s",arg_c);
                                if (++c < dec_i.argc) printf(",");
                            }

                            switch (relocent->r.reloc_address_type&EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_MASK) {
                                case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_SEGMENT:
                                    {
                                        printf("<");
                                        print_relocation_segment(&ne_imported_name_table,&ne_entry_table,&ne_nonresname,&ne_resname,relocent,&mod_syms);
                                        if (relocent->r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_ADDITIVE) {
                                            printf(" + 0x%04x>",
                                                (unsigned int)dec_i.argv[0].value);
                                        }
                                        else {
                                            printf(">");
                                        }
                                    }
                                    break;
                                case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_OFFSET:
                                    {
                                        printf("<");
                                        print_relocation_offset(&ne_imported_name_table,&ne_entry_table,&ne_nonresname,&ne_resname,relocent,&mod_syms);
                                        if (relocent->r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_ADDITIVE) {
                                            printf(" + 0x%04x>",
                                                (unsigned int)dec_i.argv[0].value);
                                        }
                                        else {
                                            printf(">");
                                        }
                                    }
                                    break;
                            };

                            reloc_ann = 1;
                        }
                    }
                }

                if (!reloc_ann) {
                    for (c=0;c < dec_i.argc;) {
                        minx86dec_regprint(&dec_i.argv[c],arg_c);
                        printf("%s",arg_c);
                        if (++c < dec_i.argc) printf(",");
                    }
                }
                if (dec_i.lock) printf("  ; LOCK#");
                printf("\n");

                /* if any part of the instruction is affected by EXE relocations, say so */
                if (reloc && reloci < reloc->length) {
                    const union exe_ne_header_segment_relocation_entry *relocent = reloc->table + reloci;
                    const uint32_t o = relocent->r.seg_offset;

                    if (o >= ip && o < (ip + inslen)) {
                        printf("             ^ EXE relocation ");

                        switch (relocent->r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_MASK) {
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_INTERNAL_REFERENCE:
                                printf("Internal ref");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_ORDINAL:
                                printf("Import by ordinal");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_NAME:
                                printf("Import by name");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_OSFIXUP:
                                printf("OSFIXUP");
                                break;
                        }
                        if (relocent->r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_ADDITIVE)
                            printf(" (ADDITIVE)");
                        printf(" ");

                        switch (relocent->r.reloc_address_type&EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_MASK) {
                            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_OFFSET_LOBYTE:
                                printf("addr=OFFSET_LOBYTE");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_SEGMENT:
                                printf("addr=SEGMENT");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_FAR_POINTER:
                                printf("addr=FAR_POINTER(16:16)");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_OFFSET:
                                printf("addr=OFFSET");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_FAR48_POINTER:
                                printf("addr=FAR_POINTER(16:32)");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_ADDR_TYPE_OFFSET32:
                                printf("addr=OFFSET32");
                                break;
                            default:
                                printf("addr=0x%02x",relocent->r.reloc_address_type);
                                break;
                        }
                        printf("\n");

                        switch (relocent->r.reloc_type&EXE_NE_HEADER_SEGMENT_RELOC_TYPE_MASK) {
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_INTERNAL_REFERENCE:
                                if (relocent->intref.segment_index == 0xFF) {
                                    get_entry_name_by_ordinal(name_tmp,sizeof(name_tmp),&ne_nonresname,&ne_resname,relocent->movintref.entry_ordinal);

                                    printf("                    Refers to movable segment, entry ordinal #%d",
                                            relocent->movintref.entry_ordinal);
                                    if (name_tmp[0] != 0)
                                        printf(" '%s'",name_tmp);
                                    printf("\n");
                                }
                                else {
                                    printf("                    Refers to segment #%d : 0x%04x\n",
                                            relocent->intref.segment_index,
                                            relocent->intref.seg_offset);
                                }
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_ORDINAL:
                                ne_imported_name_table_entry_get_module_ref_name(name_tmp,sizeof(name_tmp),&ne_imported_name_table,relocent->ordinal.module_reference_index);
                                printf("                    Refers to module reference #%d '%s', ordinal %d",
                                        relocent->ordinal.module_reference_index,name_tmp,
                                        relocent->ordinal.ordinal);
                                {
                                    const char *sym = mod_symbols_list_lookup(
                                        &mod_syms,
                                        relocent->ordinal.module_reference_index,
                                        relocent->ordinal.ordinal);
                                    if (sym != NULL) printf(" '%s'",sym);
                                }
                                printf("\n");
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_IMPORTED_NAME:
                                ne_imported_name_table_entry_get_module_ref_name(name_tmp,sizeof(name_tmp),&ne_imported_name_table,relocent->ordinal.module_reference_index);
                                printf("                    Refers to module reference #%d '%s', imp name offset %d",
                                        relocent->name.module_reference_index,name_tmp,
                                        relocent->name.imported_name_offset);

                                ne_imported_name_table_entry_get_name(name_tmp,sizeof(name_tmp),&ne_imported_name_table,relocent->name.imported_name_offset);
                                printf(" '%s'\n",
                                        name_tmp);
                                break;
                            case EXE_NE_HEADER_SEGMENT_RELOC_TYPE_OSFIXUP:
                                printf("                    OSFIXUP type=0x%04x\n",
                                        relocent->osfixup.fixup);
                                break;
                        }

                        printf("                    at +%u bytes (%04x:%04x)\n",
                                (unsigned int)(o - ip),
                                (unsigned int)dec_cs,
                                (unsigned int)dec_st.ip_value + (unsigned int)(o - ip));
                    }
                }

                dec_read = dec_i.end;
            } while(1);
        }
    }

    if (ne_segment_relocs) {
        unsigned int i;

        for (i=0;i < ne_segments.length;i++)
            exe_ne_header_segment_reloc_table_free(&ne_segment_relocs[i]);

        free(ne_segment_relocs);
    }

    mod_symbols_list_free(&mod_syms);
    exe_ne_header_imported_name_table_free(&ne_imported_name_table);
    exe_ne_header_entry_table_table_free(&ne_entry_table);
    exe_ne_header_name_entry_table_free(&ne_nonresname);
    exe_ne_header_name_entry_table_free(&ne_resname);
    exe_ne_header_resource_table_free(&ne_resources);
    exe_ne_header_segment_table_free(&ne_segments);
    dec_free_labels();
    close(src_fd);
	return 0;
}

