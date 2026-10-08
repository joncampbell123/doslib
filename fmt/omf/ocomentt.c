
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

const char *omf_coment_class_to_str(const unsigned char c) {
    switch (c) {
        case OMF_COMENT_TRANSLATOR:         return "Translator";
        case OMF_COMENT_COPYRIGHT:          return "Copyright";
        case OMF_COMENT_LIBRARY_SPEC:       return "Library specifier (obsolete)";
        case OMF_COMENT_WATCOM_PROC_MODEL:  return "Watcom processor and memory model";
        case OMF_COMENT_DOS_VERSION:        return "MS-DOS version (obsolete)";
        case OMF_COMENT_MS_PROC_MODEL:      return "Microsoft processor and memory model";
        case OMF_COMENT_DOSSEG:             return "DOSSEG";
        case OMF_COMENT_DEFAULT_LIBRARY:    return "Default library";
        case OMF_COMENT_OMF_EXTENSION:      return "OMF extension";
        case OMF_COMENT_NEW_OMF:            return "New OMF extension";
        case OMF_COMENT_LINK_PASS:          return "Link pass separator";
        case OMF_COMENT_LIBMOD:             return "LIBMOD";
        case OMF_COMENT_EXESTR:             return "EXESTR";
        case OMF_COMENT_INCERR:             return "INCERR";
        case OMF_COMENT_NOPAD:              return "NOPAD";
        case OMF_COMENT_WKEXT:              return "WKEXT (weak extern)";
        case OMF_COMENT_LZEXT:              return "LZEXT (lazy extern)";
        case OMF_COMENT_EASY_OMF:           return "Easy OMF";
        case OMF_COMENT_DEPENDENCY:         return "Dependency file";
        case OMF_COMENT_DISASM_DIRECTIVE:   return "Watcom disassembler directive";
        case OMF_COMENT_LINKER_DIRECTIVE:   return "Watcom linker directive";
        case OMF_COMENT_COMMAND_LINE:       return "Command line or source file name";
    };

    return "?";
}

const char *omf_coment_omfext_to_str(const unsigned char s) {
    switch (s) {
        case OMF_COMENT_OMFEXT_IMPDEF:      return "IMPDEF";
        case OMF_COMENT_OMFEXT_EXPDEF:      return "EXPDEF";
        case OMF_COMENT_OMFEXT_INCDEF:      return "INCDEF";
        case OMF_COMENT_OMFEXT_PROTLIB:     return "Protected Memory Library";
        case OMF_COMENT_OMFEXT_LNKDIR:      return "LNKDIR";
        case OMF_COMENT_OMFEXT_BIG_ENDIAN:  return "Big-endian";
        case OMF_COMENT_OMFEXT_PRECOMP:     return "PRECOMP";
    };

    return "?";
}

