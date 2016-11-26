
#include <fmt/omf/osegdefs.h>
#include <fmt/omf/osegdeft.h>

const char *omf_segdefs_alignment_to_str(const unsigned char a) {
    switch (a) {
        case OMF_SEGDEF_ALIGN_ABSOLUTE:     return "ABSOLUTE";
        case OMF_SEGDEF_RELOC_BYTE:         return "BYTE";      // 1
        case OMF_SEGDEF_RELOC_WORD:         return "WORD";      // 2
        case OMF_SEGDEF_RELOC_PARA:         return "PARA";      // 16
        case OMF_SEGDEF_RELOC_PAGE:         return "PAGE";      // 4096
        case OMF_SEGDEF_RELOC_DWORD:        return "DWORD";     // 4
    };

    return "?";
}

const char *omf_segdefs_combination_to_str(const unsigned char c) {
    switch (c) {
        case OMF_SEGDEF_COMBINE_PRIVATE:    return "PRIVATE";
        case OMF_SEGDEF_COMBINE_PUBLIC:     return "PUBLIC";
        case OMF_SEGDEF_COMBINE_PUBLIC4:    return "PUBLIC"; // SAME AS PUBLIC
        case OMF_SEGDEF_COMBINE_STACK:      return "STACK";
        case OMF_SEGDEF_COMBINE_COMMON:     return "COMMON";
        case OMF_SEGDEF_COMBINE_PUBLIC7:    return "PUBLIC"; // SAME AS PUBLIC
    };

    return "?";
}

