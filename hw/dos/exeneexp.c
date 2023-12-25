
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>

#include <hw/dos/exehdr.h>
#include <hw/dos/exenehdr.h>
#include <hw/dos/exenepar.h>

#ifndef O_BINARY
#define O_BINARY (0)
#endif

static char*                    src_file_real = NULL;
static char*                    src_file = NULL;
static int                      src_fd = -1;

static struct exe_dos_header    exehdr;

static void help(void) {
    fprintf(stderr,"EXENEEXP -i <exe file>\n");
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

        if (i != 0)
            printf("    ORDINAL %u: %s\n",ordinal,tmp);
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
                printf("%s",tmp);
                printf("\n    ORDINAL.%u.TYPE=resident",ordinal);
                return;
            }
        }
    }

    if (nonresnames->table != NULL) {
        for (i=0;i < nonresnames->length;i++) {
            struct exe_ne_header_name_entry *ent = nonresnames->table + i;

            if (ne_name_entry_get_ordinal(nonresnames,ent) == ordinal) {
                ne_name_entry_get_name(tmp,sizeof(tmp),nonresnames,ent);
                printf("%s",tmp);
                printf("\n    ORDINAL.%u.TYPE=nonresident",ordinal);
                return;
            }
        }
    }

    printf("\n    ORDINAL.%u.TYPE=",ordinal);
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
        if (rawd == NULL) {
            printf("    ORDINAL.%u.TYPE=empty\n",i + 1);
        }
        else if (ent->segment_id == 0x00) {
            printf("    ORDINAL.%u.TYPE=empty\n",i + 1);
        }
        else {
            printf("    ORDINAL.%u.NAME=",i + 1);
            print_entry_table_locate_name_by_ordinal(nonresnames,resnames,i + 1);
            /* has generated .TYPE by now */
            if (ent->segment_id == 0xFE) {
                struct exe_ne_header_entry_table_fixed_segment_entry *fent =
                    (struct exe_ne_header_entry_table_fixed_segment_entry*)rawd;

                if (!(fent->flags & 1)) printf(" private");
                printf(" constant=0x%04x",fent->v.seg_offs);
            }
            else if (ent->segment_id == 0xFF) {
                struct exe_ne_header_entry_table_movable_segment_entry *ment =
                    (struct exe_ne_header_entry_table_movable_segment_entry*)rawd;

                if (!(ment->flags & 1)) printf(" private");
            }
            else {
                struct exe_ne_header_entry_table_fixed_segment_entry *fent =
                    (struct exe_ne_header_entry_table_fixed_segment_entry*)rawd;

                if (!(fent->flags & 1)) printf(" private");
            }
            printf("\n");
        }
    }
}

void name_entry_table_sort_by_user_options(struct exe_ne_header_name_entry_table * const t) {
    ne_name_entry_sort_by_table = t;
    if (t->raw == NULL || t->length <= 1)
        return;

    /* NTS: Do not sort the module name in entry 0 */
    qsort(t->table+1,t->length-1,sizeof(*(t->table)),ne_name_entry_sort_by_ordinal);
}

void dump_ne_res_BITMAPINFOHEADER(const struct exe_ne_header_BITMAPINFOHEADER *bmphdr) {
    const unsigned char *fence = (const unsigned char*)bmphdr + bmphdr->biSize;

    if (((const unsigned char*)bmphdr + sizeof(struct exe_ne_header_BITMAPCOREHEADER)) > fence)
        return;

    printf("                        biSize:             %lu",
        (unsigned long)bmphdr->biSize);
    if (bmphdr->biSize == sizeof(struct exe_ne_header_BITMAPCOREHEADER))
        printf(" BITMAPCOREHEADER");
    if (bmphdr->biSize == sizeof(struct exe_ne_header_BITMAPINFOHEADER))
        printf(" BITMAPINFOHEADER");
    printf("\n");

    printf("                        biWidth:            %ld\n",
        (signed long)bmphdr->biWidth);
    printf("                        biHeight:           %ld\n",
        (signed long)bmphdr->biHeight);
    printf("                        biPlanes:           %u\n",
        (unsigned int)bmphdr->biPlanes);
    printf("                        biBitCount:         %u\n",
        (unsigned int)bmphdr->biBitCount);

    if (((const unsigned char*)bmphdr + sizeof(struct exe_ne_header_BITMAPINFOHEADER)) > fence)
        return;

    printf("                        biCompression:      0x%08lx",
        (unsigned long)bmphdr->biCompression);
    if (bmphdr->biCompression == exe_ne_header_BI_RGB)
        printf(" BI_RGB");
    if (bmphdr->biCompression == exe_ne_header_BI_RLE8)
        printf(" BI_RLE8");
    if (bmphdr->biCompression == exe_ne_header_BI_RLE4)
        printf(" BI_RLE4");
    if (bmphdr->biCompression == exe_ne_header_BI_BITFIELDS)
        printf(" BI_BITFIELDS");
    if (bmphdr->biCompression == exe_ne_header_BI_JPEG)
        printf(" BI_JPEG");
    if (bmphdr->biCompression == exe_ne_header_BI_PNG)
        printf(" BI_PNG");
    printf("\n");

    printf("                        biSizeImage:        %lu bytes\n",
        (unsigned long)bmphdr->biSizeImage);

    /* NTS: Despite Microsoft documentation stating that biSizeImage is used in cursors, icons, bitmaps etc.
     *      the resources seen in PBRUSH.EXE and other Windows 3.1 files says otherwise (biSizeImage == 0) */

    printf("                        biXPelsPerMeter:    %ld\n",
        (unsigned long)bmphdr->biXPelsPerMeter);
    printf("                        biYPelsPerMeter:    %ld\n",
        (unsigned long)bmphdr->biYPelsPerMeter);
    printf("                        biClrUsed:          %lu\n",
        (unsigned long)bmphdr->biClrUsed);
    printf("                        biClrImportant:     %lu\n",
        (unsigned long)bmphdr->biClrImportant);
}

size_t dump_ne_res_BITMAPINFO_PALETTE(const struct exe_ne_header_BITMAPINFOHEADER *bmphdr,const unsigned char *data,const unsigned char *fence) {
    unsigned int num_colors = exe_ne_header_BITMAPINFOHEADER_get_palette_count(bmphdr);
    const struct exe_ne_header_RGBQUAD *pal = (const struct exe_ne_header_RGBQUAD*)data;
    const unsigned char *p_data = data;
    unsigned int palent;

    printf("                      * Number of color palette entries: %u\n",num_colors);
    if (num_colors != 0 && (data+(sizeof(*pal) * num_colors) <= fence)) {
        for (palent=0;palent < num_colors;palent++) {
            if ((palent&3) == 0) {
                if (palent != 0) printf("\n");
                printf("                        ");
            }

            printf("RGBX(0x%02x,0x%02x,0x%02x,0x%02x) ",
                    pal[palent].rgbRed,
                    pal[palent].rgbGreen,
                    pal[palent].rgbBlue,
                    pal[palent].rgbReserved);
        }

        printf("\n");
        data += sizeof(*pal) * num_colors;
    }

    return (size_t)(data - p_data);
}

int main(int argc,char **argv) {
    struct exe_ne_header_entry_table_table ne_entry_table;
    struct exe_ne_header_name_entry_table ne_nonresname;
    struct exe_ne_header_name_entry_table ne_resname;
    struct exe_ne_header ne_header;
    uint32_t ne_header_offset;
    uint32_t file_size;
    char *a;
    int i;

    assert(sizeof(ne_header) == 0x40);
    memset(&exehdr,0,sizeof(exehdr));
    exe_ne_header_name_entry_table_init(&ne_resname);
    exe_ne_header_name_entry_table_init(&ne_nonresname);
    exe_ne_header_entry_table_table_init(&ne_entry_table);

    for (i=1;i < argc;) {
        a = argv[i++];

        if (*a == '-') {
            do { a++; } while (*a == '-');

            if (!strcmp(a,"h") || !strcmp(a,"help")) {
                help();
                return 1;
            }
            else if (!strcmp(a,"i")) {
                src_file = argv[i++];
                if (src_file == NULL) return 1;
            }
            else if (!strcmp(a,"n")) {
                src_file_real = argv[i++];
                if (src_file_real == NULL) return 1;
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
    if ((ne_header_offset+EXE_HEADER_NE_HEADER_SIZE) >= file_size) {
        fprintf(stderr,"NE header not present (offset out of range)\n");
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

    /* load nonresident name table */
    if (ne_header.nonresident_name_table_offset != 0 && ne_header.nonresident_name_table_length != 0 &&
        (unsigned long)lseek(src_fd,ne_header.nonresident_name_table_offset,SEEK_SET) == ne_header.nonresident_name_table_offset) {
        unsigned char *base;

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

        base = exe_ne_header_name_entry_table_alloc_raw(&ne_resname,raw_length);
        if (base != NULL) {
            if ((unsigned long)read(src_fd,base,ne_resname.raw_length) != ne_resname.raw_length)
                exe_ne_header_name_entry_table_free_raw(&ne_resname);
        }

        exe_ne_header_name_entry_table_parse_raw(&ne_resname);
        name_entry_table_sort_by_user_options(&ne_resname);
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

    /* show module name */
    {
        if (ne_resname.table != NULL && ne_resname.length != 0) {
            const struct exe_ne_header_name_entry *ent = ne_resname.table;
            uint16_t ordinal;
            char tmp[255+1];

            ordinal = ne_name_entry_get_ordinal(&ne_resname,ent);
            ne_name_entry_get_name(tmp,sizeof(tmp),&ne_resname,ent);

            if (ordinal == 0)
                printf("MODULE %s\n",tmp);
        }
    }

    /* show module description */
    {
        if (ne_nonresname.table != NULL && ne_nonresname.length != 0) {
            const struct exe_ne_header_name_entry *ent = ne_nonresname.table;
            uint16_t ordinal;
            char tmp[255+1];

            ordinal = ne_name_entry_get_ordinal(&ne_nonresname,ent);
            ne_name_entry_get_name(tmp,sizeof(tmp),&ne_nonresname,ent);

            if (ordinal == 0)
                printf("    DESCRIPTION=%s\n",tmp);
        }
    }

    /* file it came from */
    {
        char *nn = src_file_real ? src_file_real : src_file;
#if defined(TARGET_MSDOS) || defined(TARGET_WINDOWS)
        char *fname = strrchr(nn,'\\');
#else
        char *fname = strrchr(nn,'/');
#endif

        if (fname != NULL)
            fname++;
        else
            fname = nn;

        printf("    FILE.NAME=%s\n",fname);
    }

    /* file size */
    {
        unsigned long sz = lseek(src_fd,0,SEEK_END);

        printf("    FILE.SIZE=%lu\n",sz);
    }

    /* exports */
    print_entry_table(&ne_entry_table,&ne_nonresname,&ne_resname);

    exe_ne_header_entry_table_table_free(&ne_entry_table);
    exe_ne_header_name_entry_table_free(&ne_nonresname);
    exe_ne_header_name_entry_table_free(&ne_resname);
    close(src_fd);
    return 0;
}
