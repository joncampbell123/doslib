
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
    fprintf(stderr," -neoff <n> Assume NE header offset\n");
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

void name_entry_table_sort_by_user_options(struct exe_ne_header_name_entry_table * const t) {
    unsigned int first = 0;

    ne_name_entry_sort_by_table = t;
    if (t->raw == NULL || t->length <= 1)
        return;

    if (ne_name_entry_get_ordinal(t,&t->table[0]) == 0)
        first++;

    if (opt_sort_ordinal) {
        /* NTS: Do not sort the module name in entry 0 IF first entry is zero */
        qsort(t->table+first,t->length-first,sizeof(*(t->table)),ne_name_entry_sort_by_ordinal);
    }
    else if (opt_sort_names) {
        /* NTS: Do not sort the module name in entry 0 IF first entry is zero */
        qsort(t->table+first,t->length-first,sizeof(*(t->table)),ne_name_entry_sort_by_name);
    }
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

void dump_ne_res_RT_ICON(const unsigned char *data,const size_t len) {
    const unsigned char *fence = data + len;

    printf("                RT_ICON resource:\n");

    if (len < 4)
        return;

    if (exe_ne_header_is_WINOLDICON(data,len)) {
        const struct exe_ne_header_RTICONBITMAP *bmphdr =
            (const struct exe_ne_header_RTICONBITMAP*)data;

        // NTS: We wouldn't be here if rnType != 0x1, rnFlags anything else, rnZero != 0
        printf("                    BITMAP icHeader (Windows 1.x/2.x):\n");

        if ((data+sizeof(*bmphdr)) > fence)
            return;

        if (bmphdr->rnZero != 0)
            printf("                        rnZero:         0x%04X\n",bmphdr->rnZero);
        if (bmphdr->bmType != 0)
            printf("                        bmType:         0x%04X\n",bmphdr->bmType);

	// NTS: Despite providing structure members to specify the icon width and height,
	//      bits per pixel and bitplanes, the only thing Windows really pays attention
	//      to (kind of) is the bmWidthBytes field. Icons in Windows 1.x and 2.x are
	//      ALWAYS 1-bit monochrome 64x64 icons (Windows scales down to 32x32 for
	//      display). It doesn't appear to pay attention to bmWidthBytes for the mask,
	//      only the non-mask bits.
	//
	//      So if you were naive enough to think you could specify a 32x32 16-color icon
	//      in this format, think again, because Windows 1.0/2.0 will misrender them as
	//      monochrome anyway.

        printf("                        bmWidth:        %u\n",bmphdr->bmWidth);
        printf("                        bmHeight:       %u\n",bmphdr->bmHeight);
        printf("                        bmWidthBytes:   %u bytes\n",bmphdr->bmWidthBytes);
        printf("                        bmPlanes:       %u\n",bmphdr->bmPlanes);
        printf("                        bmBitsPixel:    %u\n",bmphdr->bmBitsPixel);
    }
    else {
        const struct exe_ne_header_BITMAPINFOHEADER *bmphdr =
            (const struct exe_ne_header_BITMAPINFOHEADER*)data;

        /* BITMAPINFOHEADER         icHeader;
         * RGBQUAD                  icColors[];
         * BYTE                     icXOR[];
         * BYTE                     icAND[]; */
        printf("                    BITMAPINFOHEADER icHeader: (biHeight reflects image + mask)\n");

        if ((data+bmphdr->biSize) > fence)
            return;

        dump_ne_res_BITMAPINFOHEADER(bmphdr);
        data += bmphdr->biSize;
        if (data >= fence)
            return;

        data += dump_ne_res_BITMAPINFO_PALETTE(bmphdr,data,fence);
    }

    assert(data <= fence);
}

void dump_ne_res_RT_GROUP_ICON(const unsigned char *data,const size_t len) {
    const struct exe_ne_header_resource_GRICONDIRENTRY *grent;
    const struct exe_ne_header_resource_ICONDIR *hdr;
    const unsigned char *fence = data + len;
    unsigned int count = 0;

    printf("                RT_GROUP_ICON resource:\n");

    /* struct exe_ne_header_resource_ICONDIR            hdr
     * truct exe_ne_header_resource_GRICONDIRENTRY      entries[idCount] */
    if ((data+sizeof(*hdr)) > fence) return;
    hdr = (const struct exe_ne_header_resource_ICONDIR *)data;
    data += sizeof(*hdr);

    printf("                    header:\n");
    printf("                        idType:         %u\n",hdr->idType);
    printf("                        idCount:        %u\n",hdr->idCount);

    while (count < hdr->idCount && (data+sizeof(*grent)) <= fence) {
        grent = (const struct exe_ne_header_resource_GRICONDIRENTRY *)data;

        printf("                    entry:\n");
        printf("                        bWidth:         %u\n",grent->bWidth);
        printf("                        bHeight:        %u\n",grent->bHeight);
        printf("                        bColorCount:    %u\n",grent->bColorCount);
        printf("                        bReserved:      %u\n",grent->bReserved);
        printf("                        wPlanes:        %u\n",grent->wPlanes);
        printf("                        wBitCount:      %u\n",grent->wBitCount);
        printf("                        dwBytesInRes:   %lu\n",(unsigned long)grent->dwBytesInRes);
        printf("                        nID:            %u\n",grent->nID);

        /* NTS: The Windows 3.1 docs I have mis-document it as "resource table index of RT_ICON resource", which is wrong.
         *      Even more devious, is that this mis-documentation would happen to work regardless because RT_ICON resources
         *      are compiled with sequential IDs anyway starting from 1 and almost every file in Windows 3.1 is compiled
         *      this way.
         *
         *      MSDN docs on their site correctly document it as the ID of the RT_ICON resource. */

        data += sizeof(*grent);
        count++;
    }

    assert(data <= fence);
}

void dump_ne_res_RT_CURSOR(const unsigned char *data,const size_t len,unsigned int winver) {
    const unsigned char *fence = data + len;

    if (len < (4+4))
        return;

    /* There's no way to auto detect from values because the first two are the cursor hotspot.
     * A cursor hotspot of 3,3 could be mistaken as an old Windows 2.0 icon. We have to use
     * the NE header's version number */
    if (winver < 0x300) {
        const struct exe_ne_header_RTCURSORBITMAP *bmphdr =
            (const struct exe_ne_header_RTCURSORBITMAP*)data;

        // NTS: We wouldn't be here if rnType != 0x1, rnFlags anything else, rnZero != 0
        printf("                    BITMAP icHeader (Windows 1.x/2.x):\n");

        if ((data+sizeof(*bmphdr)) > fence)
            return;


	// NTS: Despite providing structure members to specify the icon width and height,
	//      bits per pixel and bitplanes, Windows COMPLETELY IGNORES the members of
	//      this structure. Cursors are always 32x32 monochrome 1bpp, no matter WHAT
	//      this structure says (so WHY have a structure???).

        printf("                        hotspot:        %u, %u\n",bmphdr->hotspotX,bmphdr->hotspotY);
        printf("                        bmWidth:        %u\n",bmphdr->bmWidth);
        printf("                        bmHeight:       %u\n",bmphdr->bmHeight);
        printf("                        bmWidthBytes:   %u bytes\n",bmphdr->bmWidthBytes);
        printf("                        bmPlanes:       %u\n",bmphdr->bmPlanes);
        printf("                        bmBitsPixel:    %u\n",bmphdr->bmBitsPixel);
    }
    else {
        const struct exe_ne_header_BITMAPINFOHEADER *bmphdr;

        printf("                RT_CURSOR resource:\n");
        printf("                    wXHotspot:          %d\n",*((int16_t*)(data+0)));
        printf("                    wYHotspot:          %d\n",*((int16_t*)(data+2)));
        data += 4;

        bmphdr = (const struct exe_ne_header_BITMAPINFOHEADER*)data;
        /* BITMAPINFOHEADER         icHeader;
         * RGBQUAD                  icColors[];
         * BYTE                     icXOR[];
         * BYTE                     icAND[]; */
        printf("                    BITMAPINFOHEADER icHeader: (biHeight reflects image + mask)\n");

        if ((data+bmphdr->biSize) > fence)
            return;

        dump_ne_res_BITMAPINFOHEADER(bmphdr);
        data += bmphdr->biSize;
        if (data >= fence)
            return;

        data += dump_ne_res_BITMAPINFO_PALETTE(bmphdr,data,fence);
    }

    assert(data <= fence);
}

void dump_ne_res_RT_GROUP_CURSOR(const unsigned char *data,const size_t len) {
    const struct exe_ne_header_resource_GRCURSORDIRENTRY *grent;
    const struct exe_ne_header_resource_CURSORDIR *hdr;
    const unsigned char *fence = data + len;
    unsigned int count = 0;

    printf("                RT_GROUP_CURSOR resource:\n");

    /* struct exe_ne_header_resource_CURSORDIR            hdr
     * truct exe_ne_header_resource_GRCURSORDIRENTRY      entries[idCount] */
    if ((data+sizeof(*hdr)) > fence) return;
    hdr = (const struct exe_ne_header_resource_CURSORDIR *)data;
    data += sizeof(*hdr);

    printf("                    header:\n");
    printf("                        cdType:         %u\n",hdr->cdType);
    printf("                        cdCount:        %u\n",hdr->cdCount);

    while (count < hdr->cdCount && (data+sizeof(*grent)) <= fence) {
        grent = (const struct exe_ne_header_resource_GRCURSORDIRENTRY *)data;

        printf("                    entry:\n");
        printf("                        wWidth:         %u\n",grent->wWidth);
        printf("                        wHeight:        %u\n",grent->wHeight);
        printf("                        wPlanes:        %u\n",grent->wPlanes);
        printf("                        wBitCount:      %u\n",grent->wBitCount);
        printf("                        lBytesInRes:    %lu\n",(unsigned long)grent->lBytesInRes);
        printf("                        nID:            %u\n",grent->nID);

        /* NTS: The Windows 3.1 docs I have mis-document it as "resource table index of RT_CURSOR resource", which is wrong.
         *      Even more devious, is that this mis-documentation would happen to work regardless because RT_CURSOR resources
         *      are compiled with sequential IDs anyway starting from 1 and almost every file in Windows 3.1 is compiled
         *      this way. */

        data += sizeof(*grent);
        count++;
    }

    assert(data <= fence);
}

void dump_ne_res_RT_BITMAP(const unsigned char *data,const size_t len) {
    const unsigned char *fence = data + len;

    printf("                RT_BITMAP resource:\n");

    if (len < 4)
        return;

    /* if the first byte is 0x02, it's a Windows 1.x or 2.x BITMAP.
     * in resources dumped on my test environments, the first two bytes are usually 0x02 0x00
     * however a few have been seen with 0x02 0x81. I'm guessing the 0x02 means it's a bitmap
     * and the second byte is a flags field of some kind.
     *
     * these old formats COULD do color, however all that I can find are monochromatic 1bpp samples so far.
     *
     * note that in the Windows 1.04 SDK, the ICO files in the samples are essentially byte-for-byte
     * the same format as what is placed in the final EXE, rather than the Windows 3.x approach
     * where the file is given an ICON directory and each icon is stored as a separate RT_ICON resource. */
    if (exe_ne_header_is_WINOLDBITMAP(data,len)) {
        const struct exe_ne_header_RTBITMAP *bmphdr =
            (const struct exe_ne_header_RTBITMAP *)data;

        /* BYTE                     ???         = 0x02
         * BYTE                     ???         = 0x00 (or 0x80, or 0x81)
         * WORD                     bmType;
         * WORD                     bmWidth;
         * WORD                     bmHeight;
         * WORD                     bmWidthBytes;
         * BYTE                     bmPlanes;
         * BYTE                     bmBitsPixel;
         * DWORD                    ???         = 0x00000000     (TOTAL: 16 BYTES)
         *
         * BYTE                     bmBits[] */
        printf("                    BITMAP bmHeader (Windows 1.x/2.x):\n");

        if ((data+sizeof(*bmphdr)) > fence)
            return;

	/* NTS: Despite the presence of fields, and such fields are even filled in,
	 *      Windows 1.x/2.x does not pay attention to bmBitsPerPixel or bmPlanes,
	 *      nor does it pay attention to bmWidthBytes. All that matters is
	 *      bmWidth and bmHeight.
	 *
	 *      Which means color bitmaps in Windows 1.x/2.x are impossible. Noticeably,
	 *      all cursors, icons, and bitmaps in Windows are monochrome 1bpp only.*/

        /* NTS: bmType == 0, always, or else this branch would not have been taken */
        printf("                        bmWidth:        %u\n",bmphdr->bmWidth);
        printf("                        bmHeight:       %u\n",bmphdr->bmHeight);
        printf("                        bmWidthBytes:   %u bytes\n",bmphdr->bmWidthBytes);
        printf("                        bmPlanes:       %u\n",bmphdr->bmPlanes);
        printf("                        bmBitsPixel:    %u\n",bmphdr->bmBitsPixel);
        if (bmphdr->bmZero != 0)
            printf("                        bmZero:         0x%08lx\n",(unsigned long)bmphdr->bmZero);
    }
    else {
        const struct exe_ne_header_BITMAPINFOHEADER *bmphdr;

        bmphdr = (const struct exe_ne_header_BITMAPINFOHEADER*)data;
        /* BITMAPINFOHEADER         icHeader;
         * RGBQUAD                  icColors[];
         * BYTE                     icXOR[];
         * BYTE                     icAND[]; */
        printf("                    BITMAPINFOHEADER bmHeader:\n");

        if ((data+bmphdr->biSize) > fence)
            return;

        dump_ne_res_BITMAPINFOHEADER(bmphdr);
        data += bmphdr->biSize;
        if (data >= fence)
            return;

        data += dump_ne_res_BITMAPINFO_PALETTE(bmphdr,data,fence);
    }

    assert(data <= fence);
}

void dump_ne_res_RT_STRING(const unsigned char *data,const size_t len,const unsigned int rnID) {
    const unsigned char *fence = data + len;
    unsigned int count = 0;
    unsigned char length;
    char tmp[255+1];

    /* NTS: Here's something to trip you up:
     *
     *      Most Windows resources, when you locate them with the FindResource()
     *      API and an integer ID, will lookup the ID value you specified directly,
     *      however according to the Windows 3.1 SDK for RT_STRING:
     *
     *          "A string table consists of one or more separate resources containing
     *           exactly 16 strings."
     *
     *          ...
     *
     *          "Windows uses a 16-bit identifier to locate a string in a string-table
     *           resource. Bits 4 through 15 specify the block in which the string
     *           appears. Bits 0 through 3 specify the location of that string relaive
     *           to the beginning of the block".
     *
     *      "Location of the string" is misleading, they mean the string count relative
     *      to the first string in the block.
     *
     *      If PBRUSH.EXE in Windows 3.1 is any indication, this means that when Windows
     *      looks up a string, it does something like this:
     *
     *      WORD target_rnID = (wanted_ID >> 4)
     *      BYTE search_index = wanted_ID & 0xF;
     *
     *      So if your code looks up string ID 0x123, Windows will load RT_STRING resource
     *      with rnID = 0x12 and skip 3 strings before returning the next one.
     */

    printf("                RT_STRING resource:\n");

    while (data < fence) {
        length = *data++;
        if ((data+length) > fence) break;

        tmp[length] = 0; /* don't worry, this is why tmp[] is 255+1 bytes */
        if (length != 0) {
            memcpy(tmp,data,length);
            data += length;
        }

        /* RT_STRING resources are supposed to have 16 strings.
         * Be prepared to print more if it violates that rule.
         * Alignment and rounding of resource data offset + size means we could
         * potentially read enough zeros past the end to count more than 16, but
         * it's not something to show unless extra strings show up there. */
        if (count < 16 || length != 0)
            printf("                    0x%04x: ",((rnID << 4U) + count) & 0xFFFFU);

        if (length != 0) {
            /* string resources can be multiple-line! */
            char *s = tmp,*f = tmp + length,*n;
            unsigned int lines = 0;

            while (s < f && (*s != 0 && s == tmp)) {
                /* scan for newline.
                 * can be CR LF (Windows 3.1 and higher) or just LF (Windows 3.0) */
                n = s;
                while (*n && !(*n == '\r' || *n == '\n')) n++;
                if (*n == '\r') *n++ = 0;
                if (*n == '\n') *n++ = 0;

                if (lines != 0)
                    printf("\n                            ");

                printf("'%s'",s);

                s = n;
                lines++;
            }
            printf("\n");
        }
        else if (count < 16) {
            printf("''");
            printf("\n");
        }
        count++;
    }

    assert(data <= fence);
}

void dump_ne_res_RT_ACCELERATOR(const unsigned char *data,const size_t len) {
    const struct exe_ne_header_resource_ACCELERATOR *ent;
    const unsigned char *fence = data + len;

    printf("                RT_ACCELERATOR resource:\n");

    while ((data+sizeof(*ent)) <= fence) {
        ent = (const struct exe_ne_header_resource_ACCELERATOR*)data;

        printf("                    entry:\n");
        printf("                        fFlags:             0x%02x",ent->fFlags);

        if (ent->fFlags & exe_ne_header_ACCELERATOR_FLAGS_VIRTUAL_KEY)
            printf(" VKEY");
        else
            printf(" ASCII");

        if (ent->fFlags & exe_ne_header_ACCELERATOR_FLAGS_NOHILITE_TOPLEVEL)
            printf(" NOHILIGHT_TOPLEVEL");
        if (ent->fFlags & exe_ne_header_ACCELERATOR_FLAGS_SHIFT)
            printf(" SHIFT");
        if (ent->fFlags & exe_ne_header_ACCELERATOR_FLAGS_CONTROL)
            printf(" CTRL");
        if (ent->fFlags & exe_ne_header_ACCELERATOR_FLAGS_ALT)
            printf(" ALT");
        if (ent->fFlags & exe_ne_header_ACCELERATOR_FLAGS_LAST_ENTRY)
            printf(" LAST");
        printf("\n");

        printf("                        wEvent:             0x%04x (VK_KEY or ASCII)",ent->wEvent);
        // Ahahaha notice Microsoft never documented bit 0 which is actually very important to determine
        // if wEvent is ASCII or a VK scan code. Pfff...
        if (ent->fFlags & exe_ne_header_ACCELERATOR_FLAGS_VIRTUAL_KEY) {
            if ((ent->wEvent >= 'A' && ent->wEvent <= 'Z') || /* only 'A'-'Z' are defined as ASCII */
                (ent->wEvent >= '0' && ent->wEvent <= '9')) { /* only '0'-'9' are defined as ASCII */
                printf(" '%c'-key",toupper((char)ent->wEvent));
            }
            else if (ent->wEvent == 0x01)
                printf(" VK_LBUTTON");
            else if (ent->wEvent == 0x02)
                printf(" VK_RBUTTON");
            else if (ent->wEvent == 0x03)
                printf(" VK_CANCEL");
            else if (ent->wEvent == 0x04)
                printf(" VK_MBUTTON");
            else if (ent->wEvent == 0x05)
                printf(" VK_XBUTTON1");
            else if (ent->wEvent == 0x06)
                printf(" VK_XBUTTON2");
            else if (ent->wEvent == 0x08)
                printf(" VK_BACK");
            else if (ent->wEvent == 0x09)
                printf(" VK_TAB");
            else if (ent->wEvent == 0x0C)
                printf(" VK_CLEAR");
            else if (ent->wEvent == 0x0D)
                printf(" VK_RETURN");
            else if (ent->wEvent == 0x10)
                printf(" VK_SHIFT");
            else if (ent->wEvent == 0x11)
                printf(" VK_CONTROL");
            else if (ent->wEvent == 0x12)
                printf(" VK_MENU");
            else if (ent->wEvent == 0x13)
                printf(" VK_PAUSE");
            else if (ent->wEvent == 0x14)
                printf(" VK_CAPITAL");
            else if (ent->wEvent == 0x15)
                printf(" VK_KANA/VK_HANGEUL");
            else if (ent->wEvent == 0x16)
                printf(" VK_IME_ON");
            else if (ent->wEvent == 0x17)
                printf(" VK_JUNJA");
            else if (ent->wEvent == 0x18)
                printf(" VK_FINAL");
            else if (ent->wEvent == 0x19)
                printf(" VK_HANJA");
            else if (ent->wEvent == 0x1A)
                printf(" VK_KANJI");
            else if (ent->wEvent == 0x1B)
                printf(" VK_ESCAPE");
            else if (ent->wEvent == 0x20)
                printf(" VK_SPACE");
            else if (ent->wEvent == 0x2D)
                printf(" VK_INSERT");
            else if (ent->wEvent == 0x2E)
                printf(" VK_DELETE");
            else if (ent->wEvent == 0x2F)
                printf(" VK_HELP");
            else if (ent->wEvent >= 0x70 && ent->wEvent <= 0x87) /* F1-F24 */
                printf(" VK_F%u",(unsigned int)(ent->wEvent + 1 - 0x70));
        }
        else {
            if (ent->wEvent >= 0x01 && ent->wEvent <= 0x1A)
                printf(" CTRL+%c",(char)(ent->wEvent+'A'-1));
            else if (ent->wEvent >= 0x20 && ent->wEvent <= 0x7F)
                printf(" '%c'",(char)ent->wEvent);
        }
        printf("\n");

        printf("                        wId:                0x%04x\n",ent->wId);

        if (ent->fFlags & exe_ne_header_ACCELERATOR_FLAGS_LAST_ENTRY)
            break;

        data += sizeof(*ent);
    }
}

void dump_ne_res_RT_NAME_TABLE(const unsigned char *data,const size_t len) {
    const struct exe_ne_header_resource_NAMETABLE *ent;
    const unsigned char *fence = data + len;
    const char *p;

    printf("                RT_NAME_TABLE resource:\n");

    while ((data+sizeof(*ent)) < fence) {
        ent = (const struct exe_ne_header_resource_NAMETABLE*)data;
        if (ent->wBytesInEntry < sizeof(*ent)) break;
        if ((data+ent->wBytesInEntry) > fence) break;

        printf("                    entry:\n");
        printf("                        wBytesInEntry:          %u\n",ent->wBytesInEntry);
        printf("                        wTypeOrdinal:           0x%04x\n",ent->wTypeOrdinal);
        printf("                        wIDOrdinal:             0x%04x\n",ent->wIDOrdinal);

        // szType and szID are null-terminated strings.
        // as a safety precaution, we do not dump strings if the last byte of the entry is nonzero.
        //
        // If the high order bit of wTypeOrdinal is set, then szType is just \0
        // If the high order bit of wIDOrdinal is set, then szID is just \0
        //
        // I noticed in every Windows 3.0 installed file under \WINDOWS that all programs use this
        // name table resource scheme to name their resources, and the resource name part of the
        // resource segment contains nothing. Only Windows 3.0 uses this mapping scheme to associate
        // names with resources, Windows 3.1 and later, as well as Windows 2.x and earlier, use the
        // rscResourceNames portion of the resource segment.
        if (ent->wBytesInEntry > sizeof(*ent) && data[ent->wBytesInEntry-1] == 0) {
            p = (const char*)(data + sizeof(*ent));

            printf("                        szType:                 '%s'\n",p);
            p += strlen(p)+1;
            assert((const char*)p <= (const char*)(data+ent->wBytesInEntry));

            if ((const char*)p < (const char*)(data+ent->wBytesInEntry)) {
                printf("                        szID:                   '%s'\n",p);
                p += strlen(p)+1;
                assert((const char*)p <= (const char*)(data+ent->wBytesInEntry));
            }
        }

        data += ent->wBytesInEntry;
    }

    assert(data <= fence);
}

void dump_ne_res_RT_MENU(const unsigned char *data,const size_t len) {
    const struct exe_ne_header_resource_MENU_HEADER *hdr;
    const unsigned char *fence = data + len;

    printf("                RT_MENU resource:\n");

    if (len < sizeof(struct exe_ne_header_resource_MENU_HEADER))
        return;

    printf("                    MenuHeader:\n");
    hdr = (const struct exe_ne_header_resource_MENU_HEADER*)data;

    if (hdr->wVersion == 0) { /* Windows 3.x menu format */
#define MAX_DEPTH 16
        const struct exe_ne_header_resource_MENU_NORMAL_ITEM *stack[MAX_DEPTH] = {NULL};
        const char *menu_text;
        int stacksp = -1;
        unsigned int i;

        printf("                        wVersion:       0x%04x\n",hdr->wVersion);
        printf("                        wReserved:      0x%04x\n",hdr->wReserved);
        data += sizeof(*hdr);

        /* apparently the menu structure is something with a common struct, except one field
         * is missing if MF_POPUP is in the flags field. Each popup menu enters into a deeper level,
         * until MF_END to leave the popup and resume the popup menu above it.
         *
         * POPUP ITEM
         *   NORMAL ITEM
         *   NORMAL ITEM
         *   NORMAL ITEM
         *   POPUP ITEM
         *     NORMAL ITEM
         *     NORMAL ITEM MF_END
         *   NORMAL ITEM
         *   POPUP ITEM
         *     NORMAL ITEM MF_END
         *   NORMAL ITEM MF_END
         * (end of menu)
         *
         * FIXME: So does that mean the last item has MF_END regardless whether a popup or normal item? MSDN docs don't say.
         *        The examples given only cover when a normal item is last. */
        printf("                    Windows 3.x menu:\n");

        while ((data+2+1) <= fence) { /* enough for menu item + NUL char */
            /* NTS: Remember that the popup item and normal item are ALMOST the same structure.
             *      A popup item is missing a field. However, the first 16-bit word (flags)
             *      sits at the same location. */
            const struct exe_ne_header_resource_MENU_NORMAL_ITEM *mitem =
                (const struct exe_ne_header_resource_MENU_NORMAL_ITEM*)data;

            /* FIXME: How is a menu formatted if the last item of a popup is a popup? */
            if (mitem->fItemFlags & exe_ne_header_MF_POPUP) {
                /* a popup item is the same as normal item minus the menu ID field */
                if ((data+2+1) >= fence) break; /* must be enough room for at least a NUL */
                data += 2;
                menu_text = (const char*)data;
            }
            else {
                /* NTS: Entries with fItemFlags == 0 and wMenuID == 0 at topmost level are a
                 *      subtle hint that we've overrun past the end of the menu into the
                 *      undefined extra data region caused by resource data+len alignment. */
                if (stacksp < 0 && mitem->fItemFlags == 0 && mitem->wMenuID == 0)
                    break;

                /* proceed */
                if ((data+2+2+1) >= fence) break; /* must be enough room for at least a NUL */
                data += 2+2; /* flags + menu ID */
                menu_text = (const char*)data;
            }

            /* scan past string + NUL or end of buffer.
             * if we hit end of buffer without a NUL, then break out, because this code requires the ending NUL to show it. */
            while (data < fence && *data != 0) data++;
            if (data >= fence) break;
            if (*data == 0) data++;

            printf("L%u ",stacksp + 1);
            for (i=0;i < (unsigned int)(stacksp + 6 + 1);i++) printf("    ");
            printf("\"%s\" flags=0x%04x ",menu_text,mitem->fItemFlags);
            if (!(mitem->fItemFlags & exe_ne_header_MF_POPUP))
                printf("wMenuID=0x%04x ",mitem->wMenuID);

            if (mitem->fItemFlags & exe_ne_header_MF_GRAYED)
                printf("MF_GRAYED ");
            if (mitem->fItemFlags & exe_ne_header_MF_BITMAP)
                printf("MF_BITMAP ");
            if (mitem->fItemFlags & exe_ne_header_MF_CHECKED)
                printf("MF_CHECKED ");
            if (mitem->fItemFlags & exe_ne_header_MF_POPUP)
                printf("MF_POPUP ");
            if (mitem->fItemFlags & exe_ne_header_MF_MENUBARBREAK)
                printf("MF_MENUBARBREAK ");
            if (mitem->fItemFlags & exe_ne_header_MF_MENUBREAK)
                printf("MF_MENUBREAK ");
            if (mitem->fItemFlags & exe_ne_header_MF_END)
                printf("MF_END ");
            if (mitem->fItemFlags & exe_ne_header_MF_OWNERDRAW)
                printf("MF_OWNERDRAW ");

            /* NTS: I like how Microsoft App Studio (MSVC 1.52c) and SDK documentation implies
             *      that there's a flag you set to make a menu break, yet none of the binaries
             *      in the WINDOWS directory use it. They make a menu break instead with a
             *      NUL string, no menu ID, and no flags. */
            if (*menu_text == 0/*NUL string*/ && mitem->fItemFlags == 0 && mitem->wMenuID == 0)
                printf("[implicit MF_MENUBREAK]");

            printf("\n");

            if (mitem->fItemFlags & exe_ne_header_MF_POPUP) {
                if ((++stacksp) >= MAX_DEPTH) {
                    /* TOO DEEP! */
                    printf("! Menu is too deep!\n");
                    break;
                }

                assert(stacksp >= 0);
                stack[stacksp] = mitem;
            }
            else if (mitem->fItemFlags & exe_ne_header_MF_END) {
                /* Guess: If the popup entry that started us also has MF_END, then go up another level.
                 *        And if that has MF_END, go up. And so on, and so forth. I don't know if this
                 *        is correct behavior. If I can whip up a dummy NE executable with menus that
                 *        have popup menus on the last item of a popup, I can verify this. The other
                 *        possibility is that Microsoft somehow prohibits this in their resource
                 *        compiler.
                 * Update: A-ha! Microsoft Visual C++ 1.52's main executable MSVC.EXE has such a popup menu!
                 *         It's the "Project" menu where the last item "Save Workspace" is a popup menu.
                 *         Let's make sure it displays correctly */
                while (stacksp >= 0 && (stack[stacksp]->fItemFlags & exe_ne_header_MF_END))
                    stacksp--;
                if (stacksp >= 0)
                    stacksp--;
            }
        }

        if (stacksp >= 0)
            printf("! Menu does not seem to end properly at level=%d\n",stacksp);
#undef MAX_DEPTH
    }
    else {
#define MAX_DEPTH 16
        const struct exe_ne_header_resource_WIN2X_MENU_NORMAL_ITEM *stack[MAX_DEPTH] = {NULL};
        const char *menu_text;
        int stacksp = -1;
        unsigned int i;

        /* Same as Windows 3.x but some fields are shorter, and submenus are not supported.
	 *
	 * So you can have:
	 *
	 * POPUP ITEM
	 *   NORMAL ITEM
	 *   etc
	 * (end of menu)
	 * POPUP ITEM
	 *   NORMAL ITEM
	 *   etc
	 * (end of menu)
	 * etc
	 *
	 * But not:
	 *
	 * POPUP ITEM
	 *   NORMAL ITEM
	 *   POPUP ITEM
	 *     NORMAL ITEM
	 *   (end of menu)
	 * (end of menu)
	 * POPUP ITEM
	 *   NORMAL ITEM
	 *   POPUP ITEM
	 *     NORMAL ITEM
	 *   (end of menu)
	 * (end of menu)
         *
         * This restriction is also enforced at runtime using CreateMenu/ChangeMenu/etc */
        printf("                    Windows 1.x/2.x menu:\n");

        while ((data+2+1) <= fence) { /* enough for menu item + NUL char */
            /* NTS: Remember that the popup item and normal item are ALMOST the same structure.
             *      A popup item is missing a field. However, the first 16-bit word (flags)
             *      sits at the same location. */
            const struct exe_ne_header_resource_WIN2X_MENU_NORMAL_ITEM *mitem =
                (const struct exe_ne_header_resource_WIN2X_MENU_NORMAL_ITEM*)data;

            /* FIXME: How is a menu formatted if the last item of a popup is a popup? */
            if (mitem->fItemFlags & exe_ne_header_MF_POPUP) {
                /* a popup item is the same as normal item minus the menu ID field */
                if ((data+1+1) >= fence) break; /* must be enough room for at least a NUL */
                data += 1;
                menu_text = (const char*)data;
            }
            else {
                /* NTS: Entries with fItemFlags == 0 and wMenuID == 0 at topmost level are a
                 *      subtle hint that we've overrun past the end of the menu into the
                 *      undefined extra data region caused by resource data+len alignment. */
                if (stacksp < 0 && mitem->fItemFlags == 0 && mitem->wMenuID == 0)
                    break;

                /* proceed */
                if ((data+1+2+1) >= fence) break; /* must be enough room for at least a NUL */
                data += 1+2; /* flags + menu ID */
                menu_text = (const char*)data;
            }

            /* scan past string + NUL or end of buffer.
             * if we hit end of buffer without a NUL, then break out, because this code requires the ending NUL to show it. */
            while (data < fence && *data != 0) data++;
            if (data >= fence) break;
            if (*data == 0) data++;

            printf("L%u ",stacksp + 1);
            for (i=0;i < (unsigned int)(stacksp + 6 + 1);i++) printf("    ");
            printf("\"%s\" flags=0x%02x ",menu_text,mitem->fItemFlags);
            if (!(mitem->fItemFlags & exe_ne_header_MF_POPUP))
                printf("wMenuID=0x%04x ",mitem->wMenuID);

            if (mitem->fItemFlags & exe_ne_header_MF_GRAYED)
                printf("MF_GRAYED ");
            if (mitem->fItemFlags & exe_ne_header_MF_BITMAP)
                printf("MF_BITMAP ");
            if (mitem->fItemFlags & exe_ne_header_MF_CHECKED)
                printf("MF_CHECKED ");
            if (mitem->fItemFlags & exe_ne_header_MF_POPUP)
                printf("MF_POPUP ");
            if (mitem->fItemFlags & exe_ne_header_MF_MENUBARBREAK)
                printf("MF_MENUBARBREAK ");
            if (mitem->fItemFlags & exe_ne_header_MF_MENUBREAK)
                printf("MF_MENUBREAK ");
            if (mitem->fItemFlags & exe_ne_header_MF_END)
                printf("MF_END ");
            if (mitem->fItemFlags & exe_ne_header_MF_OWNERDRAW)
                printf("MF_OWNERDRAW ");

            /* NTS: I like how Microsoft App Studio (MSVC 1.52c) and SDK documentation implies
             *      that there's a flag you set to make a menu break, yet none of the binaries
             *      in the WINDOWS directory use it. They make a menu break instead with a
             *      NUL string, no menu ID, and no flags. */
            if (*menu_text == 0/*NUL string*/ && mitem->fItemFlags == 0 && mitem->wMenuID == 0)
                printf("[implicit MF_MENUBREAK]");

            printf("\n");

            if (mitem->fItemFlags & exe_ne_header_MF_POPUP) {
                stacksp++;
                if (stacksp >= MAX_DEPTH) {
                    /* TOO DEEP! */
                    printf("! Menu is too deep!\n");
                    break;
                }
                else if (stacksp > 0) {
                    printf("! Warning: Sub-menus not supported before Windows 3.0\n");
                }

                assert(stacksp >= 0);
                stack[stacksp] = mitem;
            }
            else if (mitem->fItemFlags & exe_ne_header_MF_END) {
                /* Guess: If the popup entry that started us also has MF_END, then go up another level.
                 *        And if that has MF_END, go up. And so on, and so forth. I don't know if this
                 *        is correct behavior. If I can whip up a dummy NE executable with menus that
                 *        have popup menus on the last item of a popup, I can verify this. The other
                 *        possibility is that Microsoft somehow prohibits this in their resource
                 *        compiler.
                 * Update: A-ha! Microsoft Visual C++ 1.52's main executable MSVC.EXE has such a popup menu!
                 *         It's the "Project" menu where the last item "Save Workspace" is a popup menu.
                 *         Let's make sure it displays correctly */
                while (stacksp >= 0 && (stack[stacksp]->fItemFlags & exe_ne_header_MF_END))
                    stacksp--;
                if (stacksp >= 0)
                    stacksp--;
            }
        }

        if (stacksp >= 0)
            printf("! Menu does not seem to end properly at level=%d\n",stacksp);
#undef MAX_DEPTH
    }

    assert(data <= fence);
}

void dump_ne_res_WSTYLE(const uint32_t lstyle) {
    if (lstyle & exe_ne_header_DS_ABSALIGN)
        printf(" DS_ABSALIGN");
    if (lstyle & exe_ne_header_DS_SYSMODAL)
        printf(" DS_SYSMODAL");
    if (lstyle & exe_ne_header_DS_LOCALEDIT)
        printf(" DS_LOCALEDIT");
    if (lstyle & exe_ne_header_DS_SETFONT)
        printf(" DS_SETFONT");
    if (lstyle & exe_ne_header_DS_MODALFRAME)
        printf(" DS_MODALFRAME");
    if (lstyle & exe_ne_header_DS_NOIDLEMSG)
        printf(" DS_NOIDLEMSG");
    if (lstyle & exe_ne_header_WS_TABSTOP)
        printf(" WS_TABSTOP/WS_MAXIMIZEBOX");
    if (lstyle & exe_ne_header_WS_GROUP)
        printf(" WS_GROUP/WS_MINIMIZEBOX");
    if (lstyle & exe_ne_header_WS_THICKFRAME)
        printf(" WS_THICKFRAME");
    if (lstyle & exe_ne_header_WS_SYSMENU)
        printf(" WS_SYSMENU");
    if (lstyle & exe_ne_header_WS_HSCROLL)
        printf(" WS_HSCROLL");
    if (lstyle & exe_ne_header_WS_VSCROLL)
        printf(" WS_VSCROLL");
    if (lstyle & exe_ne_header_WS_DLGFRAME)
        printf(" WS_DLGFRAME");
    if (lstyle & exe_ne_header_WS_BORDER)
        printf(" WS_BORDER");
    if (lstyle & exe_ne_header_WS_CAPTION)
        printf(" WS_CAPTION");
    if (lstyle & exe_ne_header_WS_MAXIMIZE)
        printf(" WS_MAXIMIZE");
    if (lstyle & exe_ne_header_WS_CLIPCHILDREN)
        printf(" WS_CLIPCHILDREN");
    if (lstyle & exe_ne_header_WS_CLIPSIBLINGS)
        printf(" WS_CLIPSIBLINGS");
    if (lstyle & exe_ne_header_WS_DISABLED)
        printf(" WS_DISABLED");
    if (lstyle & exe_ne_header_WS_VISIBLE)
        printf(" WS_VISIBLE");
    if (lstyle & exe_ne_header_WS_MINIMIZE)
        printf(" WS_MINIMIZE");
    if (lstyle & exe_ne_header_WS_CHILD)
        printf(" WS_CHILD");
    if (lstyle & exe_ne_header_WS_POPUP)
        printf(" WS_POPUP");
}

void dump_ne_res_RT_DIALOG(const unsigned char *data,const size_t len) {
    const struct exe_ne_header_resource_DIALOG_HEADER *hdr;
    const unsigned char *fence = data + len;
    unsigned int ctli;

    printf("                RT_DIALOG resource:\n");

    if (len < sizeof(*hdr))
        return;

    /* NTS: A preliminary look at Windows 2.x binaries shows a same or similar dialog box structure */
    hdr = (const struct exe_ne_header_resource_DIALOG_HEADER*)data;
    data += sizeof(*hdr);
    printf("                    Dialog header:\n");
    printf("                        lStyle:             0x%08lx",(unsigned long)(hdr->lStyle));
    dump_ne_res_WSTYLE(hdr->lStyle);
    printf("\n");
    printf("                        bNumberOfItems:     %u\n",hdr->bNumberOfItems);
    printf("                        x:                  %u dialog units\n",hdr->x);
    printf("                        y:                  %u dialog units\n",hdr->y);
    printf("                        cx:                 %u dialog units\n",hdr->cx);
    printf("                        cy:                 %u dialog units\n",hdr->cy);
    if (data >= fence) return;

    /* szMenuName (null-terminated string) */
    /* NUL-terminated string: resource by name of RT_MENU resource */
    /* NUL: no menu */
    /* 0xFF, uint16_t: resource by ordinal of RT_MENU resource */
    printf("                        szMenuName:         ");
    {
        if (data[0] == 0) {
            printf("(none)");
            data++;
        }
        else if (data[0] == 0xFF) { // UNTESTED!
            if ((data+3) > fence) return;
            printf("RT_MENU ordinal #%d",*((uint16_t*)(data + 3)));
            data += 3;
        }
        else { // UNTESTED!
            const char *str = (const char*)data;
            while (data < fence && *data != 0) data++;
            if (data >= fence) return;
            if (*data == 0) data++;

            printf("RT_MENU name '%s'",str);
        }
    }
    printf("\n");
    if (data >= fence) return;

    /* szClassName (null-terminated string) */
    printf("                        szClassName:        ");
    {
        if (data[0] == 0) {
            printf("(default)");
            data++;
        }
        else { // UNTESTED!
            const char *str = (const char*)data;
            while (data < fence && *data != 0) data++;
            if (data >= fence) return;
            if (*data == 0) data++;

            printf("'%s'",str);
        }
    }
    printf("\n");
    if (data >= fence) return;

    /* szCaption (null-terminated string) */
    printf("                        szCaption:          ");
    {
        const char *str = (const char*)data;
        while (data < fence && *data != 0) data++;
        if (data >= fence) return;
        if (*data == 0) data++;

        printf("'%s'",str);
    }
    printf("\n");
    if (data >= fence) return;

    /* these two fields only exist if DS_SETFONT */
    if (hdr->lStyle & exe_ne_header_DS_SETFONT) {
        /* uint16_t wPointSize
         * char szFaceName[]; */
        if ((data+2+1) > fence) return;

        printf("                        wPointSize:         %u pt\n",*((uint16_t*)data));
        data += 2;
        {
            const char *str = (const char*)data;
            while (data < fence && *data != 0) data++;
            if (data >= fence) return;
            if (*data == 0) data++;

            printf("                        szFaceName:         '%s'\n",str);
        }
    }

    /* now list the controls */
    for (ctli=0;ctli < hdr->bNumberOfItems;ctli++) {
        const struct exe_ne_header_resource_DIALOG_CONTROL *ctl =
            (const struct exe_ne_header_resource_DIALOG_CONTROL *)data;
        data += sizeof(*ctl);

        printf("                    Dialog control:\n");
        printf("                        x:                  %u dialog units\n",ctl->x);
        printf("                        y:                  %u dialog units\n",ctl->y);
        printf("                        cx:                 %u dialog units\n",ctl->cx);
        printf("                        cy:                 %u dialog units\n",ctl->cy);
        printf("                        wID:                0x%04x\n",ctl->wID);
        printf("                        lStyle:             0x%08lx",(unsigned long)ctl->lStyle);
        dump_ne_res_WSTYLE(ctl->lStyle);
        printf("\n");

        /* ClassID.
         *
         * If (first byte & 0x80) then the class ID is a single byte.
         * else it's a NUL-terminated string. */
        if (data >= fence) return;
        if (*data & 0x80) {
            unsigned char clas = *data++;
            printf("                        ClassID:            0x%02x '%s'",clas,exe_ne_header_RT_DIALOG_ClassID_to_string(clas));
            printf("\n");
        }
        else { // UNTESTED
            const char *str = (const char*)data;
            while (data < fence && *data != 0) data++;
            if (data >= fence) return;
            if (*data == 0) data++;

            printf("                        ClassID:            '%s'\n",str);
        }

        /* char szText[] */
        if (data >= fence) return;
        {
            const char *str = (const char*)data;
            while (data < fence && *data != 0) data++;
            if (data >= fence) return;
            if (*data == 0) data++;

            printf("                        szText:             '%s'\n",str);
        }

        /* NTS: This is not mentioned anywhere in the Windows 3.1 SDK, but there is an extra NUL byte here */
        if (data >= fence) return;
        if (*data == 0) data++;
    }
 
    assert(data <= fence);
}

void dump_ne_res_RT_VERSION_list(const unsigned char *data,const size_t len,const struct exe_ne_header_resource_VERSION_BLOCK *parent_blk,const char *parent_szKey,const struct exe_ne_header_resource_VERSION_BLOCK *parent_parent_blk,const char *parent_parent_szKey,const unsigned int level) {
    const struct exe_ne_header_resource_VERSION_BLOCK *blk;
    const unsigned char *fence = data + len;
    const unsigned char *base = data;
    const unsigned char *abData;
    const unsigned char *next;
    char isString = 0;
    const char *szKey;
    unsigned int i;

    while (data < fence) {
        /* Microsoft's SDK says that abData is DWORD-aligned, they don't say anything about
         * the blocks themselves being DWORD-aligned *sigh* and yet you'll lose track of
         * blocks easily if you don't do this. */
        while (data < fence && (((size_t)(data - base)) & 3/*DWORD*/) != 0) data++;
        if ((data+sizeof(*blk)) > fence) break;

        /* cbBlock, cbValue */
        blk = (const struct exe_ne_header_resource_VERSION_BLOCK*)data;
        if (blk->cbBlock <= sizeof(*blk)) break;
        if ((data+blk->cbBlock) > fence) break;
        next = data + blk->cbBlock;
        data += sizeof(*blk);
        if (data >= fence) break;

        /* szKey field */
        szKey = (const char*)data;
        while (data < fence && *data != 0) data++;
        if (data >= fence) break;
        if (*data == 0) data++;

        /* start announcing */
        for (i=0;i < (level + 5);i++) printf("    ");
        printf("Block: '%s' cbBlock=%u cbValue=%u\n",
            szKey,blk->cbBlock,blk->cbValue);

        /* "additional zero bytes are appended to the string to align the last byte on a 32-bit boundary"
           ...or, they could have simply said that additional zero bytes are appended to align abData on
           a 32-bit boundary, it would make more sense to say that. at least dumping actual RT_VERSION
           resources from Windows binaries shows that behavior. */
        while (data < fence && (((size_t)(data - base)) & 3/*DWORD*/) != 0) data++;

        /* abValue field */
        if ((data+blk->cbValue) > fence) break;
        abData = (const unsigned char*)data;
        data += blk->cbValue;

        /* we can assume as string if the parent-parent is StringFileInfo */
        isString = 0;
        if (!strcmp(szKey,"VS_VERSION_INFO")) {
            /* FIXME: What other versions of this structure exist??
             *        Windows 3.0 binaries generally don't use this, or if they do, they appear to use the same format.
             *        We don't worry about Windows 1.x and 2.x because they don't appear to have RT_VERSION */
            /* Fun facts:
             *  - Windows 3.1 binaries DO use this structure, and they set the File and Product major versions to 0x0003000A (3.10).
             *  - You can differentiate Windows 3.1 from Windows 3.11 by whether or not KRNL386.EXE has a RT_VERSION resource.
             *  - The Windows 3.11 KRNL386.EXE RT_VERSION resource lists the File and Product major versions as 0x0003000B (3.11),
             *    where most of the other binaries still have version 0x0003000A (3.10) */
                const struct exe_ne_header_resource_VS_FIXEDFILEINFO_WIN31 *fix =
                    (const struct exe_ne_header_resource_VS_FIXEDFILEINFO_WIN31*)abData;

            if (blk->cbValue >= 4) {
                for (i=0;i < (level + 5 + 1);i++) printf("    ");
                printf("dwSignature:            0x%08lX\n",(unsigned long)fix->dwSignature);
            }
            if (blk->cbValue >= 8 && fix->dwSignature == 0xFEEF04BDUL) {
                for (i=0;i < (level + 5 + 1);i++) printf("    ");
                printf("dwStrucVersion:         0x%08lX (%u.%u)\n",(unsigned long)fix->dwStrucVersion,
                    (unsigned int)(fix->dwStrucVersion >> 16UL),
                    (unsigned int)(fix->dwStrucVersion & 0xFFFFUL));
            }

            if (blk->cbValue >= sizeof(struct exe_ne_header_resource_VS_FIXEDFILEINFO_WIN31) && fix->dwSignature == 0xFEEF04BDUL && fix->dwStrucVersion >= 0x29UL) {
                uint32_t fflags;

                for (i=0;i < (level + 5 + 1);i++) printf("    ");
                printf("dwFileVersion:          MS 0x%08lX LS 0x%08lX\n",
                    (unsigned long)fix->dwFileVersionMS,
                    (unsigned long)fix->dwFileVersionLS);

                for (i=0;i < (level + 5 + 1);i++) printf("    ");
                printf("dwProductVersion:       MS 0x%08lX LS 0x%08lX\n",
                    (unsigned long)fix->dwProductVersionMS,
                    (unsigned long)fix->dwProductVersionLS);

                fflags = fix->dwFileFlags & fix->dwFileFlagsMask;
                for (i=0;i < (level + 5 + 1);i++) printf("    ");
                printf("dwFileFlags:            0x%08lX (VALUE) AND 0x%08lX (MASK)\n",
                    (unsigned long)fix->dwFileFlags,
                    (unsigned long)fix->dwFileFlagsMask);
                if (fflags != 0UL) {
                    for (i=0;i < (level + 5 + 1 + 1);i++) printf("    ");
                    if (fflags & exe_ne_header_VS_FF_DEBUG)
                        printf("VS_FF_DEBUG ");
                    if (fflags & exe_ne_header_VS_FF_INFOINFERRED)
                        printf("VS_FF_INFOINFERRED ");
                    if (fflags & exe_ne_header_VS_FF_PATCHED)
                        printf("VS_FF_PATCHED ");
                    if (fflags & exe_ne_header_VS_FF_PRERELEASE)
                        printf("VS_FF_PRERELEASE ");
                    if (fflags & exe_ne_header_VS_FF_PRIVATEBUILD)
                        printf("VS_FF_PRIVATEBUILD ");
                    if (fflags & exe_ne_header_VS_FF_SPECIALBUILD)
                        printf("VS_FF_SPECIALBUILD ");
                    printf("\n");
                }

                for (i=0;i < (level + 5 + 1);i++) printf("    ");
                printf("dwFileOS:               0x%08lX ",
                    (unsigned long)fix->dwFileOS);
                /* Groan... if only their SDK would make it clear the value is two 16-bit word values combined
                 * instead of just rattling off constant values. */
                switch (fix->dwFileOS & 0xFFFF0000UL) {
                    case exe_ne_header_VOS_UNKNOWN:     printf("VOS_UNKNOWN"); break;
                    case exe_ne_header_VOS_DOS:         printf("VOS_DOS"); break;
                    case exe_ne_header_VOS_OS216:       printf("VOS_OS216"); break;
                    case exe_ne_header_VOS_OS232:       printf("VOS_OS232"); break;
                    case exe_ne_header_VOS_NT:          printf("VOS_NT"); break;
                };
                printf(" + ");
                switch (fix->dwFileOS & 0x0000FFFFUL) {
                    case exe_ne_header_VOS__BASE:       printf("VOS__BASE"); break;
                    case exe_ne_header_VOS__WINDOWS16:  printf("VOS__WINDOWS16"); break;
                    case exe_ne_header_VOS__PM16:       printf("VOS__PM16"); break;
                    case exe_ne_header_VOS__PM32:       printf("VOS__PM32"); break;
                    case exe_ne_header_VOS__WINDOWS32:  printf("VOS__WINDOWS32"); break;
                };
                printf(" = ");
                switch (fix->dwFileOS) {
                    case exe_ne_header_VOS_DOS_WINDOWS16:   printf("VOS_DOS_WINDOWS16"); break;
                    case exe_ne_header_VOS_DOS_WINDOWS32:   printf("VOS_DOS_WINDOWS32"); break;
                    case exe_ne_header_VOS_OS216_PM16:      printf("VOS_OS216_PM16"); break;
                    case exe_ne_header_VOS_OS232_PM32:      printf("VOS_OS232_PM32"); break;
                    case exe_ne_header_VOS_NT_WINDOWS32:    printf("VOS_NT_WINDOWS32"); break;
                };
                printf("\n");

                for (i=0;i < (level + 5 + 1);i++) printf("    ");
                printf("dwFileType:             0x%08lX ",
                    (unsigned long)fix->dwFileType);
                switch (fix->dwFileType) {
                    case exe_ne_header_VFT_UNKNOWN:         printf("VFT_UNKNOWN"); break;
                    case exe_ne_header_VFT_APP:             printf("VFT_APP"); break;
                    case exe_ne_header_VFT_DLL:             printf("VFT_DLL"); break;
                    case exe_ne_header_VFT_DRV:             printf("VFT_DRV"); break;
                    case exe_ne_header_VFT_FONT:            printf("VFT_FONT"); break;
                    case exe_ne_header_VFT_VXD:             printf("VFT_VXD"); break;
                    case exe_ne_header_VFT_STATIC_LIB:      printf("VFT_STATIC_LIB"); break;
                };
                printf("\n");

                for (i=0;i < (level + 5 + 1);i++) printf("    ");
                printf("dwFileSubtype:          0x%08lX\n",
                    (unsigned long)fix->dwFileSubtype);//TODO enumeration

                // TODO: This is the 64-bit NT timestamp format, something like 100-nanosecond units since Jan 1st, 1601, right?
                //       Of course, Windows 3.1 binaries have this set to zero.
                for (i=0;i < (level + 5 + 1);i++) printf("    ");
                printf("dwFileDate:             0x%08lX%08lX\n",
                    (unsigned long)fix->dwFileDateMS,
                    (unsigned long)fix->dwFileDateLS);
            }
            else {
                printf("! Unknown variant of struct\n");
            }
        }
        else if (blk->cbValue != 0 && parent_parent_blk != NULL && parent_parent_szKey != NULL && !strcmp(parent_parent_szKey,"StringFileInfo"))
            isString = 1;
        else if (blk->cbValue >= 4 && !strcmp(szKey,"Translation") &&
            parent_blk != NULL && parent_szKey != NULL && !strcmp(parent_szKey,"VarFileInfo")) {
            /* two 16-bit values.
             *
             * uint16_t     language identifier
             * uint16_t     character-set identifier */
            for (i=0;i < (level + 5 + 1);i++) printf("    ");
            printf("Language ID:    0x%04X\n",*((uint16_t*)(abData + 0)));
            for (i=0;i < (level + 5 + 1);i++) printf("    ");
            printf("Charset ID:     0x%04X\n",*((uint16_t*)(abData + 2)));
        }

        if (isString) {
            const char *szString = (const char*)abData;
            const char *scan = szString;

            while (scan < (const char*)next && *scan != 0) scan++;
            if (scan < (const char*)next && *scan == 0) {
                for (i=0;i < (level + 5 + 1);i++) printf("    ");
                printf("\"%s\"\n",szString);
            }
        }

        /* possible nested block */
        assert(data <= next);
        if ((data+sizeof(*blk)) < next)
            dump_ne_res_RT_VERSION_list(data,(size_t)(next - data),blk,szKey,parent_blk,parent_szKey,level+1U);

        /* next! */
        data = next;
    }
}

void dump_ne_res_RT_VERSION(const unsigned char *data,const size_t len) {

    printf("                RT_VERSION resource:\n");
    dump_ne_res_RT_VERSION_list(data,len,NULL,NULL,NULL,NULL,0);
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
    uint32_t spec_ne=0;
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
            else if (!strcmp(a,"neoff")) {
                spec_ne = (uint32_t)strtoul(argv[i++],NULL,0);
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

    if (spec_ne != 0) {
        ne_header_offset = spec_ne;
    }
    else {
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
        printf("            GLOBAL_INIT\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_PROTECTED_MODE_ONLY)
        printf("            PROTECTED_MODE_ONLY\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_8086)
        printf("            Has 8086 instructions\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_80286)
        printf("            Has 80286 instructions\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_80386)
        printf("            Has 80386 instructions\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_80x87)
        printf("            Has 80x87 (FPU) instructions\n");

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
        printf("            FIRST_SEGMENT_CODE_APP_LOAD / OS2_FAMILY_APP\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_LINK_ERRORS)
        printf("            Link errors\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_NON_CONFORMING)
        printf("            Non-conforming\n");
    if (ne_header.flags & EXE_NE_HEADER_FLAGS_DLL)
        printf("            DLL or driver\n");

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

            /* NTS: if bit 15 of rtTypeID is set (rtTypeID & 0x8000), rtTypeID is an integer identifier.
             *      otherwise, rtTypeID is an offset to the length + string combo containing the
             *      identifier as a string. offset is relative to the resource table.
             *
             *      there are some standard integer identifiers defined by Windows, such as
             *      RT_ICON, RT_CURSOR, RT_DIALOG, etc. that define standard resources. */
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
                printf("                rnLength:           %u sectors << %u = %lu bytes\n",
                    ninfo->rnLength,
                    exe_ne_header_resource_table_get_shift(&ne_resources),
                    (unsigned long)ninfo->rnLength << (unsigned long)exe_ne_header_resource_table_get_shift(&ne_resources));

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

                /* NTS: if bit 15 of rnID is set (rnID & 0x8000), rnID is an integer identifier.
                 *      otherwise, rnID is an offset to the length + string combo containing the
                 *      identifier as a string. offset is relative to the resource table. */
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

                if (ninfo->rnLength != 0) {
                    unsigned long res_ofs = (unsigned long)ninfo->rnOffset << (unsigned long)exe_ne_header_resource_table_get_shift(&ne_resources);
                    unsigned long res_len = (unsigned long)ninfo->rnLength << (unsigned long)exe_ne_header_resource_table_get_shift(&ne_resources);
                    unsigned char *res_raw = NULL;

                    // impose limits on resource data reading.
                    // for most formats we only care about the header anyway.
#if TARGET_MSDOS == 16
                    // refuse to handle more than 16KB if 16-bit DOS
                    if (res_len > 0x4000UL) res_len = 0x4000UL;
#else
                    // refuse to handle more than 4MB for anything else. nobody's resources are that large anyway.
                    if (res_len > 0x400000UL) res_len = 0x400000UL;
#endif

                    res_raw = malloc(res_len);
                    if (res_raw != NULL) {
                        if ((unsigned long)lseek(src_fd,res_ofs,SEEK_SET) == res_ofs &&
                            (unsigned long)read(src_fd,res_raw,res_len) == res_len) {

                            /* FIXME: Running this code against Windows 2.x executables, it seems
                             *        that the ICON, CURSOR, and BITMAP resources used an entirely
                             *        different format inside the NE resource. */
                            if (tinfo->rtTypeID == exe_ne_header_RT_ICON)
                                dump_ne_res_RT_ICON(res_raw,(size_t)res_len);
                            else if (tinfo->rtTypeID == exe_ne_header_RT_GROUP_ICON)
                                dump_ne_res_RT_GROUP_ICON(res_raw,(size_t)res_len);
                            else if (tinfo->rtTypeID == exe_ne_header_RT_CURSOR)
                                dump_ne_res_RT_CURSOR(res_raw,(size_t)res_len,ne_header.minimum_windows_version);
                            else if (tinfo->rtTypeID == exe_ne_header_RT_GROUP_CURSOR)
                                dump_ne_res_RT_GROUP_CURSOR(res_raw,(size_t)res_len);
                            else if (tinfo->rtTypeID == exe_ne_header_RT_STRING)
                                dump_ne_res_RT_STRING(res_raw,(size_t)res_len,ninfo->rnID);
                            else if (tinfo->rtTypeID == exe_ne_header_RT_NAME_TABLE)
                                dump_ne_res_RT_NAME_TABLE(res_raw,(size_t)res_len);
                            else if (tinfo->rtTypeID == exe_ne_header_RT_ACCELERATOR)
                                dump_ne_res_RT_ACCELERATOR(res_raw,(size_t)res_len);
                            else if (tinfo->rtTypeID == exe_ne_header_RT_BITMAP)
                                dump_ne_res_RT_BITMAP(res_raw,(size_t)res_len);
                            else if (tinfo->rtTypeID == exe_ne_header_RT_MENU)
                                dump_ne_res_RT_MENU(res_raw,(size_t)res_len);
                            else if (tinfo->rtTypeID == exe_ne_header_RT_DIALOG)
                                dump_ne_res_RT_DIALOG(res_raw,(size_t)res_len);
                            else if (tinfo->rtTypeID == exe_ne_header_RT_VERSION)
                                dump_ne_res_RT_VERSION(res_raw,(size_t)res_len);
                        }

                        free(res_raw);
                    }
                }
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
