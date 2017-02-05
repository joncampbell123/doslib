
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
#include <hw/dos/exelehdr.h>
#include <hw/dos/exelepar.h>

int le_parser_apply_fixup(unsigned char * const data,const size_t datlen,const uint16_t object,const uint32_t data_object_offset,struct le_header_parseinfo *le_parser) {
    struct exe_le_header_object_table_entry *objent;
    struct le_header_fixup_record_table *frtable;
    unsigned int srcoff_count,srcoff_i;
    uint32_t page_first,page_last;
    unsigned char flags,src;
    uint32_t srclinoff;
    unsigned char *raw;
    uint32_t soffset;
    uint16_t srcoff;
    unsigned int ti;
    uint32_t page;
    int count = 0;

    if (datlen == 0)
        return count;
    if (le_parser->le_fixup_records.table == NULL || le_parser->le_fixup_records.length == 0 ||
        le_parser->le_object_table == NULL || object == 0 || object > le_parser->le_header.object_table_entries)
        return count;

    objent = le_parser->le_object_table + object - 1;
    page_first = (data_object_offset / le_parser->le_header.memory_page_size) + (uint32_t)objent->page_map_index;
    page_last = ((data_object_offset + datlen - 1) / le_parser->le_header.memory_page_size) + (uint32_t)objent->page_map_index;

    for (page=page_first;page <= page_last;page++) { // <- in case the DDB struct spans two pages
        if (page > le_parser->le_fixup_records.length)
            break;
        if (page == 0)
            continue;

        frtable = le_parser->le_fixup_records.table + page - 1; // <- page numbers are 1-based
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
                uint32_t trglinoff;
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

                // for this computation, we need to convert target object:offset to linear address
                if (tobject != 0 && tobject <= le_parser->le_header.object_table_entries) {
                    struct exe_le_header_object_table_entry *tobjent = le_parser->le_object_table + tobject - 1;

                    if (tobjent->page_map_index != 0)
                        trglinoff = (((uint32_t)tobjent->page_map_index - 1) * le_parser->le_header.memory_page_size) + trgoff;
                    else
                        trglinoff = 0;
                }
                else {
                    trglinoff = 0;
                }

                if (src & 0x20) {
                    for (srcoff_i=0;srcoff_i < srcoff_count;srcoff_i++) {
                        srcoff = *((int16_t*)raw); raw += 2;

                        if ((src&0xF) == 0x7) { // must be 32-bit offset fixup
                            // what is the relocation relative to the struct we just read?
                            srclinoff = ((page - 1) * le_parser->le_header.memory_page_size) + (uint32_t)srcoff;
                            soffset = ((uint32_t)srclinoff +
                                    (((uint32_t)objent->page_map_index - 1) * le_parser->le_header.memory_page_size)) - data_object_offset;

                            // if it's within range, and in the same object, patch
                            if ((soffset+4UL) <= datlen) {
                                *((uint32_t*)(data+soffset)) = trglinoff;
                                count++;
                            }
                        }
                    }
                }
                else {
                    if ((src&0xF) == 0x7) { // must be 32-bit offset fixup
                        // what is the relocation relative to the struct we just read?
                        srclinoff = ((page - 1) * le_parser->le_header.memory_page_size) + (uint32_t)srcoff;
                        soffset = ((uint32_t)srclinoff +
                                (((uint32_t)objent->page_map_index - 1) * le_parser->le_header.memory_page_size)) - data_object_offset;

                        // if it's within range, and in the same object, patch
                        if ((soffset+4UL) <= datlen) {
                            *((uint32_t*)(data+soffset)) = trglinoff;
                            count++;
                        }
                    }
                }
            }
            else {
                continue;
            }
        }
    }

    return count;
}

