
#include "oextdefs.h"

const char *omf_extdef_type_to_string(const unsigned char t) {
    switch (t) {
        case OMF_EXTDEF_TYPE_GLOBAL:    return "GLOBAL";
        case OMF_EXTDEF_TYPE_LOCAL:     return "LOCAL";
    };

    return "?";
}

