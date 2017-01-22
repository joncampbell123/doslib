
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

#ifndef O_BINARY
#define O_BINARY (0)
#endif

static unsigned char            opt_sort_ordinal = 0;
static unsigned char            opt_sort_names = 0;

static char*                    src_file = NULL;
static int                      src_fd = -1;

static struct exe_dos_header    exehdr;
static struct exe_dos_layout    exelayout;

static void help(void) {
    fprintf(stderr,"EXENEDMP -i <exe file>\n");
    fprintf(stderr," -sn        Sort names\n");
    fprintf(stderr," -so        Sort by ordinal\n");
}

static struct exe_ne_header_name_entry_table *ne_name_entry_sort_by_table = NULL;
int ne_name_entry_sort_by_name(const void *a,const void *b) {
    struct exe_ne_header_name_entry *ea = (struct exe_ne_header_name_entry *)a;
    struct exe_ne_header_name_entry *eb = (struct exe_ne_header_name_entry *)b;
    unsigned char *pa,*pb;
    unsigned int i;

    pa = ne_name_entry_get_name_base(ne_name_entry_sort_by_table,ea);
    pb = ne_name_entry_get_name_base(ne_name_entry_sort_by_table,eb);

    for (i=0;i < ea->length && i < eb->length;i++) {
        int diff = (int)pa[i] - (int)pb[i];
        if (diff != 0) return diff;
    }

    if (ea->length == eb->length)
        return 0;
    else if (i < ea->length)
        return (int)pa[i];
    else /*if (i < eb->length)*/
        return 0 - (int)pb[i];
}

int ne_name_entry_sort_by_ordinal(const void *a,const void *b) {
    struct exe_ne_header_name_entry *ea = (struct exe_ne_header_name_entry *)a;
    struct exe_ne_header_name_entry *eb = (struct exe_ne_header_name_entry *)b;
    return ne_name_entry_get_ordinal(ne_name_entry_sort_by_table,ea) -
        ne_name_entry_get_ordinal(ne_name_entry_sort_by_table,eb);
}

int exe_ne_header_name_entry_table_parse_raw(struct exe_ne_header_name_entry_table * const t) {
    unsigned char *scan,*base,*fence;
    unsigned int entries = 0;

    exe_ne_header_name_entry_table_free_table(t);
    if (t->raw == NULL || t->raw_length == 0)
        return 0;

    base = t->raw;
    fence = base + t->raw_length;

    /* how many entries? */
    for (scan=base;scan < fence;) {
        /* uint8_t         LENGTH
         * char[length]    TEXT
         * uint16_t        ORDINAL */
        unsigned char len = *scan++; // LENGTH
        if (len == 0) break;
        if ((scan+len+2) > fence) break;

        scan += len + 2; // TEXT + ORDINAL
        entries++;
    }

    t->table = (struct exe_ne_header_name_entry*)malloc(entries * sizeof(*(t->table)));
    if (t->table == NULL) {
        exe_ne_header_name_entry_table_free_table(t);
        return -1;
    }
    t->length = entries;

    /* now scan and index the entries */
    entries = 0;
    for (scan=base;scan < fence && entries < t->length;) {
        /* uint8_t         LENGTH
         * char[length]    TEXT
         * uint16_t        ORDINAL */
        unsigned char len = *scan++; // LENGTH
        if (len == 0) break;
        if ((scan+len+2) > fence) break;

        t->table[entries].length = len;

        t->table[entries].offset = (uint16_t)(scan - base);
        scan += len;    // TEXT

        scan += 2;      // ORDINAL

        entries++;
    }

    t->length = entries;
    return 0;
}

void name_entry_table_sort_by_user_options(struct exe_ne_header_name_entry_table * const t) {
    ne_name_entry_sort_by_table = t;
    if (t->raw == NULL || t->length <= 1)
        return;

    if (opt_sort_ordinal) {
        /* NTS: Do not sort the module name in entry 0 */
        qsort(t->table+1,t->length-1,sizeof(*(t->table)),ne_name_entry_sort_by_ordinal);
    }
    else if (opt_sort_names) {
        /* NTS: Do not sort the module name in entry 0 */
        qsort(t->table+1,t->length-1,sizeof(*(t->table)),ne_name_entry_sort_by_name);
    }
}

unsigned long exe_ne_header_segment_table_get_relocation_table_offset(const struct exe_ne_header_segment_table * const t,
    const struct exe_ne_header_segment_entry * const s) {
    if (!(s->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_RELOCATIONS)) return 0;
    if (s->offset_in_segments == 0) return 0;

    return ((unsigned long)s->offset_in_segments << (unsigned long)t->sector_shift) +
        (unsigned long)s->length;
}

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

void print_imported_name_table(const struct exe_ne_header_imported_name_table * const t) {
    char tmp[255+1];
    unsigned int i;

    if (t->table == NULL || t->length == 0)
        return;

    for (i=0;i < t->length;i++) {
        ne_imported_name_table_entry_get_name(tmp,sizeof(tmp),t,t->table[i]);
        printf("        '%s'\n",tmp);
    }
}

void print_imported_name_table_module_ref_table(const struct exe_ne_header_imported_name_table * const t) {
    char tmp[255+1];
    unsigned int i;

    if (t->module_ref_table == NULL || t->module_ref_table_length == 0)
        return;

    for (i=0;i < t->module_ref_table_length;i++) {
        ne_imported_name_table_entry_get_name(tmp,sizeof(tmp),t,t->module_ref_table[i]);
        printf("        '%s'\n",tmp);
    }
}

void print_name_table(const struct exe_ne_header_name_entry_table * const t) {
    const struct exe_ne_header_name_entry *ent = NULL;
    uint16_t ordinal;
    char tmp[255+1];
    unsigned int i;

    if (t->table == NULL || t->length == 0)
        return;

    for (i=0;i < t->length;i++) {
        ent = t->table + i;
        ordinal = ne_name_entry_get_ordinal(t,ent);
        ne_name_entry_get_name(tmp,sizeof(tmp),t,ent);

        if (i == 0) {
            printf("        Module name:    '%s'\n",tmp);
            if (ordinal != 0)
                printf("        ! WARNING: Module name with ordinal != 0\n");
        }
        else
            printf("        Ordinal #%-5d: '%s'\n",ordinal,tmp);
    }
}

void print_segment_table(const struct exe_ne_header_segment_table * const t) {
    const struct exe_ne_header_segment_entry *segent;
    unsigned int i;

    if (t->table == NULL || t->length == 0)
        return;

    for (i=0;i < t->length;i++) {
        segent = t->table + i; /* C pointer math, becomes (char*)ne_segments + (i * sizeof(*ne_segments)) */

        printf("        Segment #%d:\n",i+1);
        if (segent->offset_in_segments != 0U) {
            printf("            Offset: %u segments = %u << %u = %lu bytes\n",
                segent->offset_in_segments,
                segent->offset_in_segments,
                t->sector_shift,
                (unsigned long)segent->offset_in_segments << (unsigned long)t->sector_shift);
        }
        else {
            printf("            Offset: None (no data)\n");
        }

        printf("            Length: %lu bytes\n",
            (segent->offset_in_segments != 0 && segent->length == 0) ? 0x10000UL : (unsigned long)segent->length);
        printf("            Flags: 0x%04x",
            segent->flags);

        if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_DATA)
            printf(" DATA");
        else
            printf(" CODE");
        if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_ALLOCATED)
            printf(" ALLOCATED");
        if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_LOADED)
            printf(" LOADED");
        if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_MOVEABLE)
            printf(" MOVEABLE");
        else
            printf(" FIXED");
        if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_SHAREABLE)
            printf(" SHAREABLE");
        else
            printf(" NONSHAREABLE");
        if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_PRELOAD)
            printf(" PRELOAD");
        else
            printf(" LOADONCALL");
        if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_RELOCATIONS)
            printf(" HAS_RELOCATIONS");
        if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_DISCARDABLE)
            printf(" DISCARDABLE");

        if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_DATA) {
            if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_RO_EX)
                printf(" READONLY");
            else
                printf(" READWRITE");
        }
        else {
            if (segent->flags & EXE_NE_HEADER_SEGMENT_ENTRY_FLAGS_RO_EX)
                printf(" EXECUTEONLY");
            else
                printf(" READONLY");
        }

        printf("\n");

        printf("            Minimum allocation size: %lu\n",
            (segent->minimum_allocation_size == 0) ? 0x10000UL : (unsigned long)segent->minimum_allocation_size);
    }
}

void print_segment_reloc_table(const struct exe_ne_header_segment_reloc_table * const r,struct exe_ne_header_imported_name_table * const ne_imported_name_table) {
    const union exe_ne_header_segment_relocation_entry *relocent;
    unsigned int relent;
    char tmp2[255+1];
    char tmp[255+1];

    assert(sizeof(relocent) == 8);
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
                printf("                    Refers to module reference #%d '%s', ordinal %d\n",
                    relocent->ordinal.module_reference_index,tmp,
                    relocent->ordinal.ordinal);
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

void print_entry_table_flags(const uint16_t flags) {
    if (flags & 1) printf("EXPORTED ");
    if (flags & 2) printf("USES_GLOBAL_SHARED_DATA_SEGMENT ");
    if (flags & 0xF8) printf("RING_TRANSITION_STACK_WORDS=%u ",flags >> 3);
}

void print_entry_table_locate_name_by_ordinal(const struct exe_ne_header_name_entry_table * const nonresnames,const struct exe_ne_header_name_entry_table *resnames,const unsigned int ordinal) {
    char tmp[255+1];
    unsigned int i;

    if (resnames->table != NULL) {
        for (i=0;i < resnames->length;i++) {
            struct exe_ne_header_name_entry *ent = resnames->table + i;

            if (ne_name_entry_get_ordinal(resnames,ent) == ordinal) {
                ne_name_entry_get_name(tmp,sizeof(tmp),resnames,ent);
                printf("RESIDENT NAME '%s' ",tmp);
                return;
            }
        }
    }

    if (nonresnames->table != NULL) {
        for (i=0;i < nonresnames->length;i++) {
            struct exe_ne_header_name_entry *ent = nonresnames->table + i;

            if (ne_name_entry_get_ordinal(nonresnames,ent) == ordinal) {
                ne_name_entry_get_name(tmp,sizeof(tmp),nonresnames,ent);
                printf(" NONRESIDENT NAME '%s'",tmp);
                return;
            }
        }
    }
}

void print_entry_table(const struct exe_ne_header_entry_table_table * const t,const struct exe_ne_header_name_entry_table * const nonresnames,const struct exe_ne_header_name_entry_table *resnames) {
    const struct exe_ne_header_entry_table_entry *ent;
    unsigned char *rawd;
    unsigned int i;

    if (t->table == NULL || t->length == 0)
        return;

    for (i=0;i < t->length;i++) { /* NTS: ordinal value is i + 1, ordinals are 1-based */
        ent = t->table + i;
        rawd = exe_ne_header_entry_table_table_raw_entry(t,ent);

        printf("        Ordinal #%-5d: ",i + 1); /* ordinal value is i + 1, ordinals are 1-based */
        if (rawd == NULL) {
            printf("N/A (out of range entry data)\n");
        }
        else if (ent->segment_id == 0x00) {
            printf("EMPTY\n");
        }
        else if (ent->segment_id == 0xFF) {
            /* NTS: raw_entry() function guarantees that the data available is large enough to hold this struct */
            struct exe_ne_header_entry_table_movable_segment_entry *ment =
                (struct exe_ne_header_entry_table_movable_segment_entry*)rawd;

            printf("movable segment #%d : 0x%04x",ment->segid,ment->seg_offs);
            print_entry_table_locate_name_by_ordinal(nonresnames,resnames,i + 1);
            printf("\n");
            if (ment->flags != 0) {
                printf("            ");
                print_entry_table_flags(ment->flags);
                printf("\n");
            }
        }
        else if (ent->segment_id == 0xFE) {
            /* NTS: raw_entry() function guarantees that the data available is large enough to hold this struct */
            struct exe_ne_header_entry_table_fixed_segment_entry *fent =
                (struct exe_ne_header_entry_table_fixed_segment_entry*)rawd;

            printf("constant value : 0x%04x",fent->v.seg_offs);
            print_entry_table_locate_name_by_ordinal(nonresnames,resnames,i + 1);
            printf("\n");
            if (fent->flags != 0) {
                printf("            ");
                print_entry_table_flags(fent->flags);
                printf("\n");
            }
        }
        else {
            /* NTS: raw_entry() function guarantees that the data available is large enough to hold this struct */
            struct exe_ne_header_entry_table_fixed_segment_entry *fent =
                (struct exe_ne_header_entry_table_fixed_segment_entry*)rawd;

            printf("fixed segment #%d : 0x%04x",ent->segment_id,fent->v.seg_offs);
            print_entry_table_locate_name_by_ordinal(nonresnames,resnames,i + 1);
            printf("\n");
            if (fent->flags != 0) {
                printf("            ");
                print_entry_table_flags(fent->flags);
                printf("\n");
            }
        }
    }
}

int main(int argc,char **argv) {
    struct exe_ne_header_imported_name_table ne_imported_name_table;
    struct exe_ne_header_entry_table_table ne_entry_table;
    struct exe_ne_header_name_entry_table ne_nonresname;
    struct exe_ne_header_resource_table_t ne_resources;
    struct exe_ne_header_name_entry_table ne_resname;
    struct exe_ne_header_segment_table ne_segments;
    struct exe_ne_header ne_header;
    uint32_t ne_header_offset;
    uint32_t file_size;
    char *a;
    int i;

    assert(sizeof(ne_header) == 0x40);
    memset(&exehdr,0,sizeof(exehdr));
    exe_ne_header_segment_table_init(&ne_segments);
    exe_ne_header_resource_table_init(&ne_resources);
    exe_ne_header_name_entry_table_init(&ne_resname);
    exe_ne_header_name_entry_table_init(&ne_nonresname);
    exe_ne_header_entry_table_table_init(&ne_entry_table);
    exe_ne_header_imported_name_table_init(&ne_imported_name_table);

    for (i=1;i < argc;) {
        a = argv[i++];

        if (*a == '-') {
            do { a++; } while (*a == '-');

            if (!strcmp(a,"h") || !strcmp(a,"help")) {
                help();
                return 1;
            }
            else if (!strcmp(a,"sn")) {
                opt_sort_names = 1;
            }
            else if (!strcmp(a,"so")) {
                opt_sort_ordinal = 1;
            }
            else if (!strcmp(a,"i")) {
                src_file = argv[i++];
                if (src_file == NULL) return 1;
            }
            else {
                fprintf(stderr,"Unknown switch %s\n",a);
                return 1;
            }
        }
        else {
            fprintf(stderr,"Unknown switch %s\n",a);
            return 1;
        }
    }

    assert(sizeof(exehdr) == 0x1C);

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

    printf("File size:                        %lu bytes\n",
        (unsigned long)file_size);
    printf("MS-DOS EXE header:\n");
    printf("    last_block_bytes:             %u bytes\n",
        exehdr.last_block_bytes);
    printf("    exe_file_blocks:              %u bytes\n",
        exehdr.exe_file_blocks);
    printf("  * exe resident size (blocks):   %lu bytes\n",
        (unsigned long)exe_dos_header_file_resident_size(&exehdr));
    printf("                                  ^  x  = %lu x 512 = %lu\n",
        (unsigned long)exehdr.exe_file_blocks,
        (unsigned long)exehdr.exe_file_blocks * 512UL);
    if (exehdr.last_block_bytes != 0U && exehdr.exe_file_blocks != 0U) {
        printf("                                  ^ (x -= 512) = %lu, last block not full 512 bytes\n",
            (unsigned long)exehdr.exe_file_blocks * 512UL - 512UL);
        printf("                                  ^ (x += %lu) = %lu, add last block bytes\n",
            (unsigned long)exehdr.last_block_bytes,
            ((unsigned long)exehdr.exe_file_blocks * 512UL) + (unsigned long)exehdr.last_block_bytes - 512UL);
    }
    printf("    number_of_relocations:        %u entries\n",
        exehdr.number_of_relocations);
    printf("  * size of relocation table:     %lu bytes\n",
        (unsigned long)exehdr.number_of_relocations * 4UL);
    printf("    header_size:                  %u paragraphs\n",
        exehdr.header_size_paragraphs);
    printf("  * header_size:                  %lu bytes\n",
        (unsigned long)exe_dos_header_file_header_size(&exehdr));
    printf("    min_additional_paragraphs:    %u paragraphs\n",
        exehdr.min_additional_paragraphs);
    printf("  * min_additional:               %lu bytes\n",
        (unsigned long)exe_dos_header_bss_size(&exehdr));
    printf("    max_additional_paragraphs:    %u paragraphs\n",
        exehdr.max_additional_paragraphs);
    printf("  * max_additional:               %lu bytes\n",
        (unsigned long)exe_dos_header_bss_max_size(&exehdr));
    printf("    init stack pointer:           base_seg+0x%04X:0x%04X\n",
        exehdr.init_stack_segment,
        exehdr.init_stack_pointer);
    printf("    checksum:                     0x%04X\n",
        exehdr.checksum);
    printf("    init instruction pointer:     base_seg+0x%04X:0x%04X\n",
        exehdr.init_code_segment,
        exehdr.init_instruction_pointer);
    printf("    relocation_table_offset:      %u bytes\n",
        exehdr.relocation_table_offset);
    printf("    overlay number:               %u\n",
        exehdr.overlay_number);

    if (exe_dos_header_to_layout(&exelayout,&exehdr) < 0) {
        fprintf(stderr,"EXE layout not appropriate for Windows NE\n");
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

    printf("Windows or OS/2 NE header:\n");
    printf("    Linker version:               %u.%u\n",
        ne_header.linker_version,
        ne_header.linker_revision);
    printf("    Entry table offset:           %u NE-rel (abs=%lu) bytes\n",
        ne_header.entry_table_offset,
        (unsigned long)ne_header.entry_table_offset + (unsigned long)ne_header_offset);
    printf("    Entry table length:           %lu bytes\n",
        (unsigned long)ne_header.entry_table_length);
    printf("    32-bit file CRC:              0x%08lx\n",
        (unsigned long)ne_header.file_crc);
    printf("    EXE content flags:            0x%04x\n",
        (unsigned int)ne_header.flags);

    printf("        DGROUP type:              %u",
        (unsigned int)ne_header.flags & EXE_NE_HEADER_FLAGS_DGROUPTYPE_MASK);
    switch (ne_header.flags & EXE_NE_HEADER_FLAGS_DGROUPTYPE_MASK) {
        case EXE_NE_HEADER_FLAGS_DGROUPTYPE_NOAUTODATA:
            printf(" NOAUTODATA\n");
            break;
        case EXE_NE_HEADER_FLAGS_DGROUPTYPE_SINGLEDATA:
            printf(" SINGLEDATA\n");
            break;
        case EXE_NE_HEADER_FLAGS_DGROUPTYPE_MULTIPLEDATA:
            printf(" MULTIPLEDATA\n");
            break;
        default:
            printf(" ?\n");
            break;
    };

    if (ne_header.flags & EXE_NE_HEADER_FLAGS_GLOBAL_INIT)
        printf("        GLOBAL_INIT\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_PROTECTED_MODE_ONLY)
        printf("        PROTECTED_MODE_ONLY\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_8086)
        printf("        Has 8086 instructions\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_80286)
        printf("        Has 80286 instructions\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_80386)
        printf("        Has 80386 instructions\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_80x87)
        printf("        Has 80x87 (FPU) instructions\n");

    printf("        Application type:         %u",
        ((unsigned int)ne_header.flags & EXE_NE_HEADER_FLAGS_APPTYPE_MASK) >> EXE_NE_HEADER_FLAGS_APPTYPE_SHIFT);
    switch (ne_header.flags & EXE_NE_HEADER_FLAGS_APPTYPE_MASK) {
        case EXE_NE_HEADER_FLAGS_APPTYPE_NONE:
            printf(" NONE\n");
            break;
        case EXE_NE_HEADER_FLAGS_APPTYPE_FULLSCREEN:
            printf(" FULLSCREEN\n");
            break;
        case EXE_NE_HEADER_FLAGS_APPTYPE_WINPM_COMPAT:
            printf(" WINPM_COMPAT\n");
            break;
        case EXE_NE_HEADER_FLAGS_APPTYPE_WINPM_USES:
            printf(" WINPM_USES\n");
            break;
        default:
            printf(" ?\n");
            break;
    };

    if (ne_header.flags & EXE_NE_HEADER_FLAGS_FIRST_SEGMENT_CODE_APP_LOAD)
        printf("        FIRST_SEGMENT_CODE_APP_LOAD / OS2_FAMILY_APP\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_LINK_ERRORS)
        printf("        Link errors\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_NON_CONFORMING)
        printf("        Non-conforming\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_DLL)
        printf("        DLL or driver\n");

    printf("    AUTODATA segment index:       %u\n",
        ne_header.auto_data_segment_number);
    printf("    Initial heap size:            %u\n",
        ne_header.init_local_heap);
    printf("    Initial stack size:           %u\n",
        ne_header.init_stack_size);
    printf("    CS:IP                         segment #%u : 0x%04x\n",
        ne_header.entry_cs,
        ne_header.entry_ip);
    if (ne_header.entry_ss == 0 && ne_header.entry_sp == 0) {
        printf("    SS:SP                         segment AUTODATA : sizeof(AUTODATA) + sizeof(stack)\n");
    }
    else {
        printf("    SS:SP                         segment #%u : 0x%04x\n",
            ne_header.entry_ss,
            ne_header.entry_sp);
    }
    printf("    Segment table entries:        %u\n",
        ne_header.segment_table_entries);
    printf("    Module ref. table entries:    %u\n",
        ne_header.module_reftable_entries);
    printf("    Non-resident name table len:  %u bytes\n",
        ne_header.nonresident_name_table_length);
    printf("    Segment table offset:         %u NE-rel (abs %lu) bytes\n",
        ne_header.segment_table_offset,
        (unsigned long)ne_header.segment_table_offset + (unsigned long)ne_header_offset);
    printf("    Resource table offset:        %u NE-rel (abs %lu) bytes\n",
        ne_header.resource_table_offset,
        (unsigned long)ne_header.resource_table_offset + (unsigned long)ne_header_offset);
    printf("    Resident name table offset:   %u NE-rel (abs %lu) bytes\n",
        ne_header.resident_name_table_offset,
        (unsigned long)ne_header.resident_name_table_offset + (unsigned long)ne_header_offset);
    printf("    Module ref. table offset:     %u NE-rel (abs %lu) bytes\n",
        ne_header.module_reference_table_offset,
        (unsigned long)ne_header.module_reference_table_offset + (unsigned long)ne_header_offset);
    printf("    Imported name table offset:   %u NE-rel (abs %lu) bytes\n",
        ne_header.imported_name_table_offset,
        (unsigned long)ne_header.imported_name_table_offset + (unsigned long)ne_header_offset);
    printf("    Non-resident name table offset:%lu bytes\n",
        (unsigned long)ne_header.nonresident_name_table_offset);
    printf("    Movable entry points:         %u\n",
        ne_header.movable_entry_points);
    printf("    Sector shift:                 %u (1 sector << %u = %lu bytes)\n",
        ne_header.sector_shift,
        ne_header.sector_shift,
        1UL << (unsigned long)ne_header.sector_shift);
    printf("    Number of resource segments:  %u\n",
        ne_header.resource_segments);
    printf("    Target OS:                    0x%02x ",ne_header.target_os);
    switch (ne_header.target_os) {
        case EXE_NE_HEADER_TARGET_OS_NONE:
            printf("None / Windows 2.x or earlier");
            break;
        case EXE_NE_HEADER_TARGET_OS_OS2:
            printf("OS/2");
            break;
        case EXE_NE_HEADER_TARGET_OS_WINDOWS:
            printf("Windows (3.x and later)");
            break;
        case EXE_NE_HEADER_TARGET_OS_EURO_MSDOS_4:
            printf("European MS-DOS 4.0");
            break;
        case EXE_NE_HEADER_TARGET_OS_WINDOWS_386:
            printf("Windows 386");
            break;
        case EXE_NE_HEADER_TARGET_OS_BOSS:
            printf("Boss (Borland)");
            break;
        default:
            printf("?");
            break;
    }
    printf("\n");

    printf("    Other flags:                  0x%04x\n",ne_header.other_flags);
    if (ne_header.other_flags & EXE_NE_HEADER_OTHER_FLAGS_WIN_WIN2X_IN_3X)
        printf("        Windows 2.x can run in Windows 3.x protected mode\n");
    if (ne_header.other_flags & EXE_NE_HEADER_OTHER_FLAGS_WIN_WIN2X_PROP_FONTS)
        printf("        Windows 2.x / OS/2 app supports proportional fonts\n");
    if (ne_header.other_flags & EXE_NE_HEADER_OTHER_FLAGS_WIN_HAS_FASTLOAD)
        printf("        Has a fast-load / gang-load area\n");

    if (ne_header.sector_shift == 0U) {
        // NTS: One reference suggests that sector_shift == 0 means sector_shift == 9
        printf("* ERROR: Sector shift is zero\n");
        return 1;
    }
    if (ne_header.sector_shift > 16U) {
        printf("* ERROR: Sector shift is too large\n");
        return 1;
    }

    printf("    Fastload offset:              %u sectors (%lu bytes)\n",
        ne_header.fastload_offset_sectors,
        (unsigned long)ne_header.fastload_offset_sectors << (unsigned long)ne_header.sector_shift);
    printf("    Fastload length:              %u sectors (%lu bytes)\n",
        ne_header.fastload_length_sectors,
        (unsigned long)ne_header.fastload_length_sectors << (unsigned long)ne_header.sector_shift);
    printf("    Minimum code swap area size:  %u\n", // unknown units
        ne_header.minimum_code_swap_area_size);
    printf("    Minimum Windows version:      %u.%u\n",
        (ne_header.minimum_windows_version >> 8U),
        ne_header.minimum_windows_version & 0xFFU);
    if (ne_header.minimum_windows_version == 0x30A)
        printf("        * Microsoft Windows 3.1\n");
    else if (ne_header.minimum_windows_version == 0x300)
        printf("        * Microsoft Windows 3.0\n");

    /* this code makes a few assumptions about the header that are used to improve
     * performance when reading the header. if the header violates those assumptions,
     * say so NOW */
    if (ne_header.segment_table_offset < 0x40) { // if the segment table collides with the NE header we want the user to know
        printf("! WARNING: Segment table collides with the NE header (offset %u < 0x40)\n",
            ne_header.segment_table_offset);
    }
    /* even though the NE header specifies key structures by offset from NE header,
     * they seem to follow a strict ordering as described in Windows NE notes.txt.
     * some fields have an offset, not a size, and we assume that we can determine
     * the size of those fields by the difference between offsets. */
    /* RESIDENT_NAME_TABLE_SIZE = module_reference_table_offset - resident_name_table_offset */
    if (ne_header.module_reference_table_offset < ne_header.resident_name_table_offset)
        printf("! WARNING: Module ref. table offset < Resident name table offset\n");
    /* IMPORTED_NAME_TABLE_SIZE = entry_table_offset - imported_name_table_offset */
    if (ne_header.entry_table_offset < ne_header.imported_name_table_offset)
        printf("! WARNING: Entry table offset < Imported name table offset\n");
    /* and finally, we assume we can determine the size of the NE header + resident structures by:
     * 
     * NE_RESIDENT_PORTION_HEADER_SIZE = non_resident_name_table_offset - ne_header_offset
     *
     * and therefore, the NE-relative offsets listed (adjusted to absolute file offset) will be less than the non-resident name table offset.
     *
     * I'm guessing that the reason some offsets are NE-relative and some are absolute, is because Microsoft probably
     * intended to be able to read all the resident portion NE structures at once, before concerning itself with
     * nonresident portions like the nonresident name table.
     *
     * FIXME: Do you suppose some clever NE packing tool might stick the non-resident name table in the MS-DOS stub?
     *        What does Windows do if you do that? Does Windows make the same assumption/optimization when loading NE executables? */
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
        name_entry_table_sort_by_user_options(&ne_nonresname);
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
        name_entry_table_sort_by_user_options(&ne_resname);
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

    /* resource table */
    if (ne_header.resource_table_offset != 0 && ne_header.resident_name_table_offset > ne_header.resource_table_offset &&
        (unsigned long)lseek(src_fd,ne_header.resource_table_offset + ne_header_offset,SEEK_SET) == (ne_header.resource_table_offset + ne_header_offset)) {
        unsigned int raw_length;
        unsigned char *base;

        /* RESOURCE_TABLE_SIZE = resident_name_table_offset - resource_table_offset         (header does not report size, "number of segments" is worthless) */
        raw_length = (unsigned short)(ne_header.resident_name_table_offset - ne_header.resource_table_offset);
        printf("  * Resource table length: %u\n",raw_length);

        base = exe_ne_header_resource_table_alloc_raw(&ne_resources,raw_length);
        if (base != NULL) {
            if ((unsigned int)read(src_fd,base,raw_length) != raw_length)
                exe_ne_header_resource_table_free_raw(&ne_resources);
        }

        exe_ne_header_resource_table_parse(&ne_resources);
    }

    /* imported name table */
    printf("    Imported name table, %u entries:\n",
        (unsigned int)ne_imported_name_table.length);
    print_imported_name_table(&ne_imported_name_table);

    /* module reference name table */
    printf("    Module reference name table, %u entries:\n",
        (unsigned int)ne_imported_name_table.module_ref_table_length);
    print_imported_name_table_module_ref_table(&ne_imported_name_table);

    /* non-resident name table */
    printf("    Non-resident name table, %u entries\n",
        (unsigned int)ne_nonresname.length);
    print_name_table(&ne_nonresname);

    /* resident name table */
    printf("    Resident name table, %u entries\n",
        (unsigned int)ne_resname.length);
    print_name_table(&ne_resname);

    /* segment table */
    printf("    Segment table, %u entries:\n",
        (unsigned int)ne_segments.length);
    print_segment_table(&ne_segments);

    /* segment relocation table */
    {
        struct exe_ne_header_segment_reloc_table ne_relocs;
        struct exe_ne_header_segment_entry *segent;
        unsigned long reloc_offset;
        uint16_t reloc_entries;
        unsigned char *base;
        unsigned int i;

        printf("    Segment relocations:\n");

        exe_ne_header_segment_reloc_table_init(&ne_relocs);
        for (i=0;i < ne_segments.length;i++) {
            segent = ne_segments.table + i; /* C pointer math, becomes (char*)ne_segments + (i * sizeof(*ne_segments)) */
            reloc_offset = exe_ne_header_segment_table_get_relocation_table_offset(&ne_segments,segent);
            if (reloc_offset == 0) continue;

            /* at the start of the relocation struct, is a 16-bit WORD that indicates how many entries are there,
             * followed by an array of relocation entries. */
            if ((unsigned long)lseek(src_fd,reloc_offset,SEEK_SET) != reloc_offset || read(src_fd,&reloc_entries,2) != 2)
                continue;

            printf("        Segment #%d:\n",i+1);
            printf("            Relocation table at: %lu, %u entries\n",reloc_offset,reloc_entries);
            exe_ne_header_segment_reloc_table_free(&ne_relocs);
            if (reloc_entries == 0) continue;

            base = exe_ne_header_segment_reloc_table_alloc_table(&ne_relocs,reloc_entries);
            if (base == NULL) continue;

            {
                size_t rd = exe_ne_header_segment_reloc_table_size(&ne_relocs);
                if (rd == 0) continue;

                if ((size_t)read(src_fd,ne_relocs.table,rd) != rd) {
                    exe_ne_header_segment_reloc_table_free(&ne_relocs);
                    continue;
                }
            }

            print_segment_reloc_table(&ne_relocs,&ne_imported_name_table);
            exe_ne_header_segment_reloc_table_free(&ne_relocs);
        }
    }

    /* entry table */
    printf("    Entry table, %u entries:\n",
        (unsigned int)ne_entry_table.length);
    print_entry_table(&ne_entry_table,&ne_nonresname,&ne_resname);

    printf("    Resource table, 1 << %u = %lu byte alignment:\n",
        exe_ne_header_resource_table_get_shift(&ne_resources),
        1UL << (unsigned long)exe_ne_header_resource_table_get_shift(&ne_resources));
    printf("        %u TYPEINFO entries\n",ne_resources.typeinfo_length);
    {
        const struct exe_ne_header_resource_table_nameinfo *ninfo;
        const struct exe_ne_header_resource_table_typeinfo *tinfo;
        const char *rtTypeIDintstr;
        char tmp[255+1];
        unsigned int ti;
        unsigned int ni;

        for (ti=0;ti < ne_resources.typeinfo_length;ti++) {
            printf("        Typeinfo entry #%d\n",ti+1);

            tinfo = exe_ne_header_resource_table_get_typeinfo_entry(&ne_resources,ti);
            if (tinfo == NULL) {
                printf("            NULL\n");
                continue;
            }

            if (exe_ne_header_resource_table_typeinfo_TYPEID_IS_INTEGER(tinfo->rtTypeID)) {
                printf("            rtTypeID:   INTEGER 0x%04x",
                    exe_ne_header_resource_table_typeinfo_TYPEID_AS_INTEGER(tinfo->rtTypeID));
                rtTypeIDintstr = exe_ne_header_resource_table_typeinfo_TYPEID_INTEGER_name_str(tinfo->rtTypeID);
                if (rtTypeIDintstr != NULL) printf(" %s",rtTypeIDintstr);
                printf("\n");
            }
            else {
                exe_ne_header_resource_table_get_string(tmp,sizeof(tmp),&ne_resources,tinfo->rtTypeID);
                printf("            rtTypeID:   STRING OFFSET 0x%04x '%s'",tinfo->rtTypeID,tmp);
                printf("\n");
            }

            printf("            rtResourceCount: %u\n",tinfo->rtResourceCount);
            for (ni=0;ni < tinfo->rtResourceCount;ni++) {
                ninfo = exe_ne_header_resource_table_get_typeinfo_nameinfo_entry(tinfo,ni);
                if (ninfo == NULL) {
                    printf("                NULL\n");
                    continue;
                }

                printf("            Entry #%d:\n",ni+1);
                printf("                rnOffset:           %u sectors << %u = %lu bytes\n",
                    ninfo->rnOffset,
                    exe_ne_header_resource_table_get_shift(&ne_resources),
                    (unsigned long)ninfo->rnOffset << (unsigned long)exe_ne_header_resource_table_get_shift(&ne_resources));
                printf("                rnLength:           %u bytes\n",
                    ninfo->rnLength);

                printf("                rnFlags:            0x%04x",
                    ninfo->rnFlags);
                if (ninfo->rnFlags & 0x10)
                    printf(" MOVABLE");
                else
                    printf(" FIXED");
                if (ninfo->rnFlags & 0x20)
                    printf(" SHARABLE");
                else
                    printf(" NONSHAREABLE");
                if (ninfo->rnFlags & 0x40)
                    printf(" PRELOAD");
                else
                    printf(" LOADONCALL");
                printf("\n");

                if (exe_ne_header_resource_table_typeinfo_RNID_IS_INTEGER(ninfo->rnID)) {
                    printf("                rnID:               INTEGER 0x%04x\n",
                        exe_ne_header_resource_table_typeinfo_RNID_AS_INTEGER(ninfo->rnID));
                }
                else {
                    exe_ne_header_resource_table_get_string(tmp,sizeof(tmp),&ne_resources,ninfo->rnID);
                    printf("                rnID:               STRING OFFSET 0x%04x '%s'\n",
                        ninfo->rnID,tmp);
                }

                if (ninfo->rnHandle != 0)
                    printf("                rnHandle:           0x%04x\n",
                        ninfo->rnHandle);
                if (ninfo->rnUsage != 0)
                    printf("                rnUsage:            0x%04x\n",
                        ninfo->rnUsage);
            }
        }

        printf("        rscResourceNames, %u entries\n",
            ne_resources.resnames_length);
        for (ni=0;ni < ne_resources.resnames_length;ni++) {
            exe_ne_header_resource_table_get_string(tmp,sizeof(tmp),&ne_resources,
                exe_ne_header_resource_table_get_resname(&ne_resources,ni));
            printf("            '%s'\n",tmp);
        }
    }

    exe_ne_header_imported_name_table_free(&ne_imported_name_table);
    exe_ne_header_entry_table_table_free(&ne_entry_table);
    exe_ne_header_name_entry_table_free(&ne_nonresname);
    exe_ne_header_name_entry_table_free(&ne_resname);
    exe_ne_header_resource_table_free(&ne_resources);
    exe_ne_header_segment_table_free(&ne_segments);
    close(src_fd);
    return 0;
}
