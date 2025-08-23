
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
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

int le_parser_is_windows_vxd(struct le_header_parseinfo * const p,uint16_t * const object,uint32_t * const offset) {
    if ((p->le_header.module_type_flags & LE_HEADER_MODULE_TYPE_FLAGS_IS_DLL) &&
        (p->le_header.module_type_flags & LE_HEADER_MODULE_TYPE_FLAGS_PM_WINDOWING_MASK) == LE_HEADER_MODULE_TYPE_FLAGS_PM_WINDOWING_UNKNOWN &&
        p->le_header.target_operating_system == 0x04/*Windows 386*/ && p->le_entry_table.table != NULL && p->le_entry_table.length >= 1) {
        struct le_header_entry_table_entry *ent = p->le_entry_table.table;//ordinal 1
        unsigned char *raw;
        char tmp[255+1];
        unsigned int i;

        tmp[0] = 0;
        if (tmp[0] == 0 && p->le_resident_names.table != NULL) {
            for (i=0;i < p->le_resident_names.length;i++) {
                struct exe_ne_header_name_entry *ent = p->le_resident_names.table + i;

                if (ne_name_entry_get_ordinal(&p->le_resident_names,ent) == 1) {
                    ne_name_entry_get_name(tmp,sizeof(tmp),&p->le_resident_names,ent);
                    break;
                }
            }
        }

        if (tmp[0] == 0 && p->le_nonresident_names.table != NULL) {
            for (i=0;i < p->le_nonresident_names.length;i++) {
                struct exe_ne_header_name_entry *ent = p->le_nonresident_names.table + i;

                if (ne_name_entry_get_ordinal(&p->le_nonresident_names,ent) == 1) {
                    ne_name_entry_get_name(tmp,sizeof(tmp),&p->le_nonresident_names,ent);
                    break;
                }
            }
        }

        /* HACK: Microsoft DINPUT.VXD put the DDB export in entry #1 but nonresident name ordinal == 0 */
        if (tmp[0] == 0 && p->le_nonresident_names.table != NULL) {
            for (i=0;i < p->le_nonresident_names.length;i++) {
                struct exe_ne_header_name_entry *ent = p->le_nonresident_names.table + i;

                if (ne_name_entry_get_ordinal(&p->le_nonresident_names,ent) == 0) {
                    ne_name_entry_get_name(tmp,sizeof(tmp),&p->le_nonresident_names,ent);

                    {
                        char *s = tmp + strlen(tmp) - 4;

                        if (s > tmp && !strcasecmp(s,"_DDB"))
                            break;
                        else
                            tmp[0] = 0;
                    }
                }
            }
        }

        raw = le_header_entry_table_get_raw_entry(&p->le_entry_table,0/*ordinal 1*/); /* parser makes sure there is sufficient space for struct given type */
        if (raw != NULL && (ent->type == 3)/*32-bit*/) {
            uint32_t ent_offset;
            uint8_t flags;

            flags = *raw++;
            ent_offset = *((uint32_t*)raw); raw += 4;
            assert(raw <= (p->le_entry_table.raw+p->le_entry_table.raw_length));

            if (flags & 1/*exported*/) {
                /* is named, ends in _DDB */
                char *s = tmp + strlen(tmp) - 4;
                if (s > tmp && !strcasecmp(s,"_DDB")) {
                    *object = ent->object;
                    *offset = ent_offset;
                    return 1;
                }
            }
        }
    }

    return 0;
}

