
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

const char *exe_ne_header_resource_table_typeinfo_TYPEID_INTEGER_name_str(const uint16_t typeID) {
    switch (typeID) {
        case exe_ne_header_RT_CURSOR:               return "RT_CURSOR";
        case exe_ne_header_RT_BITMAP:               return "RT_BITMAP";
        case exe_ne_header_RT_ICON:                 return "RT_ICON";
        case exe_ne_header_RT_MENU:                 return "RT_MENU";
        case exe_ne_header_RT_DIALOG:               return "RT_DIALOG";
        case exe_ne_header_RT_STRING:               return "RT_STRING";
        case exe_ne_header_RT_FONTDIR:              return "RT_FONTDIR";
        case exe_ne_header_RT_FONT:                 return "RT_FONT";
        case exe_ne_header_RT_ACCELERATOR:          return "RT_ACCELERATOR";
        case exe_ne_header_RT_RCDATA:               return "RT_RCDATA";
        case exe_ne_header_RT_MESSAGETABLE:         return "RT_MESSAGETABLE";
        case exe_ne_header_RT_GROUP_CURSOR:         return "RT_GROUP_CURSOR";
        case exe_ne_header_RT_GROUP_ICON:           return "RT_GROUP_ICON";
        case exe_ne_header_RT_NAME_TABLE:           return "RT_NAME_TABLE";
        case exe_ne_header_RT_VERSION:              return "RT_VERSION";
        default:                                    break;
    }

    return NULL;
}

