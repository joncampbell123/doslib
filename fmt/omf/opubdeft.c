
#include <fmt/omf/opubdefs.h>

const char *omf_pubdef_type_to_string(const unsigned char t) {
    switch (t) {
        case OMF_PUBDEF_TYPE_GLOBAL:    return "GLOBAL";
        case OMF_PUBDEF_TYPE_LOCAL:     return "LOCAL";
    };

    return "?";
}

