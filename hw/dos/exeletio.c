
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

int le_segofs_to_trackio(struct le_vmap_trackio * const io,const uint16_t object,const uint32_t offset,const struct le_header_parseinfo * const lep) {
    const struct exe_le_header_parseinfo_object_page_table_entry *pageent;
    const struct exe_le_header_object_table_entry *objent;

    if (object > lep->le_header.object_table_entries) return 0;

    if (object != 0) {
        io->object = object;
        io->offset = offset;
    }
    else {
        /* flat 32-bit tracking. object == 0 and offset == linear address in memory */
        io->object = 0;
        io->offset = offset;

        /* chosen linear addresses must exist past this point */
        if (lep->le_object_table_loaded_linear == NULL)
            return 0;

        /* given the linear address, what object does it correspond to? */
        io->object = 1;
        while (io->object < lep->le_header.object_table_entries && offset >= lep->le_object_table_loaded_linear[io->object])
            io->object++;

        if (io->object > lep->le_header.object_table_entries)
            return 0; /* didn't find it */

        /* what page does that correspond to? convert "offset" from linear to offset relative to object. */
        io->offset -= lep->le_object_table_loaded_linear[io->object - 1];
    }

    /* Object number is 1-based, our array is zero based */
    /* NTS: If pages in a 32-bit flat map are discontinuous, we will NOT support reading into the gaps */
    objent = (const struct exe_le_header_object_table_entry*)(lep->le_object_table + io->object - 1);
    io->page_number = objent->page_map_index + (io->offset / lep->le_header.memory_page_size);
    if (io->page_number >= (objent->page_map_index + objent->page_map_entries)) return 0;
    if (io->page_number == 0) return 0;
    io->page_ofs = io->offset % lep->le_header.memory_page_size;
    io->page_size = lep->le_header.memory_page_size;

    /* page numbers are 1-based, our array is zero based */
    if (io->page_number == 0) return 0;
    if (io->page_number > lep->le_header.number_of_memory_pages) return 0;
    pageent = (const struct exe_le_header_parseinfo_object_page_table_entry*)(lep->le_object_page_map_table + io->page_number - 1);

    io->file_ofs = pageent->page_data_offset;
    return 1;
}

int le_trackio_read(unsigned char *buf,int len,const int fd,struct le_vmap_trackio * const io,const struct le_header_parseinfo * const lep) {
    const struct exe_le_header_parseinfo_object_page_table_entry *pageent;
    unsigned long ofs;
    int rd = 0;
    int canrd;
    int gotrd;

    while (len > 0) {
        if (io->object == 0 || io->page_number == 0) break;

        if (io->page_ofs < io->page_size) {
            canrd = (int)(io->page_size - io->page_ofs);
            if (canrd > len) canrd = len;

            ofs = io->file_ofs + io->page_ofs;
            if ((unsigned long)lseek(fd,ofs,SEEK_SET) != ofs) break;

            gotrd = read(fd,buf,canrd);
            if (gotrd <= 0) break;

            io->page_ofs += gotrd;
            io->offset += gotrd;
            buf += gotrd;
            len -= gotrd;
            rd += gotrd;
        }

        assert(io->page_ofs <= io->page_size);
        if (io->page_ofs >= io->page_size) {
            /* next page */
            if (io->page_number >= lep->le_header.number_of_memory_pages) break; /* at or past last page */
            io->page_number++;
            io->page_ofs = 0;

            /* page numbers are 1-based, our array is zero based */
            pageent = (const struct exe_le_header_parseinfo_object_page_table_entry*)(lep->le_object_page_map_table + io->page_number - 1);
            io->file_ofs = pageent->page_data_offset;
        }
    }

    return rd;
}

