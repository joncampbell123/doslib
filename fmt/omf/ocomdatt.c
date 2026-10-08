
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

const char *omf_comdat_selection_to_str(const unsigned char s) {
    switch (s) {
        case OMF_COMDAT_SELECT_NO_MATCH:    return "NO-MATCH";
        case OMF_COMDAT_SELECT_PICK_ANY:    return "PICK-ANY";
        case OMF_COMDAT_SELECT_SAME_SIZE:   return "SAME-SIZE";
        case OMF_COMDAT_SELECT_EXACT_MATCH: return "EXACT-MATCH";
    };

    return "?";
}

const char *omf_comdat_allocation_to_str(const unsigned char a) {
    switch (a) {
        case OMF_COMDAT_ALLOC_EXPLICIT:     return "EXPLICIT";
        case OMF_COMDAT_ALLOC_FAR_CODE:     return "FAR-CODE";
        case OMF_COMDAT_ALLOC_FAR_DATA:     return "FAR-DATA";
        case OMF_COMDAT_ALLOC_CODE32:       return "CODE32";
        case OMF_COMDAT_ALLOC_DATA32:       return "DATA32";
    };

    return "?";
}

