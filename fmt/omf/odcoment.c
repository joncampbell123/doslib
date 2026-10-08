
#include <fmt/omf/omf.h>

// print the rest of the record after the label: as a quoted string if it is all printable, else as hex bytes
static void dump_COMENT_rest(FILE *fp,const char * const label,struct omf_record_t * const rec) {
    const size_t len = omf_record_data_available(rec);
    const unsigned char *p;
    size_t i;

    if (len == 0)
        return;

    p = rec->data + rec->recpos;
    i = 0;
    while (i < len && p[i] >= 0x20 && p[i] < 0x7F)
        i++;

    fprintf(fp,"    %-18s",label);
    if (i == len) {
        fprintf(fp,"\"%.*s\"",(int)len,(const char*)p);
    }
    else {
        for (i=0;i < len;i++)
            fprintf(fp,"%s%02X",i != 0 ? " " : "",p[i]);
    }
    fprintf(fp,"\n");

    omf_record_lseek(rec,rec->reclen);
}

// print a text comment. It is usually just the text, but NASM puts a length byte first, like the strings
// in other records. Take it that way only if that first byte is not printable, so that text that happens
// to start with a character that matches its length is still printed whole.
static void dump_COMENT_text(FILE *fp,const char * const label,struct omf_record_t * const rec) {
    const size_t len = omf_record_data_available(rec);
    const unsigned char *p;
    size_t i;

    if (len >= 2) {
        p = rec->data + rec->recpos;
        if ((size_t)p[0] == (len - 1u) && (p[0] < 0x20 || p[0] >= 0x7F)) {
            i = 1;
            while (i < len && p[i] >= 0x20 && p[i] < 0x7F)
                i++;

            if (i == len) {
                fprintf(fp,"    %-18s\"%.*s\"\n",label,(int)(len - 1u),(const char*)(p + 1));
                omf_record_lseek(rec,rec->reclen);
                return;
            }
        }
    }

    dump_COMENT_rest(fp,label,rec);
}

// Watcom and Microsoft processor and memory model, a string of characters:
//   processor ('0' = 8086, '1' = 80186, '2' = 80286, '3' = 80386 ...), memory model, 'O' if optimized, floating point
static void dump_COMENT_proc_model(FILE *fp,struct omf_record_t * const rec) {
    const char *s;
    unsigned char c;

    if (omf_record_eof(rec)) return;
    c = omf_record_get_byte(rec);
    if (c == '0')
        fprintf(fp,"    %-18s8086\n","Processor:");
    else if (c > '0' && c <= '9')
        fprintf(fp,"    %-18s80%c86\n","Processor:",(char)c);
    else
        fprintf(fp,"    %-18s?(0x%02X)\n","Processor:",c);

    if (omf_record_eof(rec)) return;
    c = omf_record_get_byte(rec);
    switch (c | 0x20/*lower case*/) {
        case 's':   s = "Small"; break;
        case 'm':   s = "Medium"; break;
        case 'c':   s = "Compact"; break;
        case 'l':   s = "Large"; break;
        case 'h':   s = "Huge"; break;
        case 'f':   s = "Flat"; break;
        default:    s = NULL; break;
    }
    if (s != NULL)
        fprintf(fp,"    %-18s%s\n","Memory model:",s);
    else
        fprintf(fp,"    %-18s?(0x%02X)\n","Memory model:",c);

    if (omf_record_eof(rec)) return;
    c = omf_record_get_byte(rec);
    fprintf(fp,"    %-18s%s\n","Optimized:",c == 'O' ? "yes" : "no");

    if (omf_record_eof(rec)) return;
    c = omf_record_get_byte(rec);
    switch (c) {
        case 'e':   s = "inline emulation"; break;
        case 'c':   s = "emulator calls"; break;
        case 'p':   s = "inline 80x87"; break;
        default:    s = NULL; break;
    }
    if (s != NULL)
        fprintf(fp,"    %-18s%s\n","Floating point:",s);
    else
        fprintf(fp,"    %-18s?(0x%02X)\n","Floating point:",c);
}

// IMPDEF: ordinal flag (byte), internal name, module name, then the ordinal (word) if the flag is set,
//         else the imported name (empty if it is the same as the internal name)
static void dump_COMENT_impdef(FILE *fp,struct omf_record_t * const rec) {
    unsigned char by_ordinal;
    int len;

    if (omf_record_eof(rec)) return;
    by_ordinal = omf_record_get_byte(rec);

    if (omf_record_get_lenstr(omf_temp_str,sizeof(omf_temp_str),rec) < 0) return;
    fprintf(fp,"    %-18sinternal=\"%s\"","Import:",omf_temp_str);

    if (omf_record_get_lenstr(omf_temp_str,sizeof(omf_temp_str),rec) >= 0) {
        fprintf(fp," module=\"%s\"",omf_temp_str);

        if (by_ordinal) {
            if (omf_record_data_available(rec) >= 2)
                fprintf(fp," ordinal=%u",omf_record_get_word(rec));
        }
        else if ((len=omf_record_get_lenstr(omf_temp_str,sizeof(omf_temp_str),rec)) > 0) {
            fprintf(fp," name=\"%s\"",omf_temp_str);
        }
        else if (len == 0) {
            fprintf(fp," name=(internal)");
        }
    }

    fprintf(fp,"\n");
}

// EXPDEF: exported flag (byte) [7] by ordinal [6] resident name [5] no data [4:0] parameter count,
//         exported name, internal name (empty if it is the same), then the ordinal (word) if by ordinal
static void dump_COMENT_expdef(FILE *fp,struct omf_record_t * const rec) {
    unsigned char flag;

    if (omf_record_eof(rec)) return;
    flag = omf_record_get_byte(rec);

    if (omf_record_get_lenstr(omf_temp_str,sizeof(omf_temp_str),rec) < 0) return;
    fprintf(fp,"    %-18sname=\"%s\"","Export:",omf_temp_str);

    if (omf_record_get_lenstr(omf_temp_str,sizeof(omf_temp_str),rec) > 0)
        fprintf(fp," internal=\"%s\"",omf_temp_str);

    if ((flag & 0x80) && omf_record_data_available(rec) >= 2)
        fprintf(fp," ordinal=%u",omf_record_get_word(rec));
    if (flag & 0x40)
        fprintf(fp," RESIDENT");
    if (flag & 0x20)
        fprintf(fp," NODATA");
    if (flag & 0x1F)
        fprintf(fp," parameters=%u",flag & 0x1F);

    fprintf(fp,"\n");
}

// New OMF extension: version (byte) and debug information style (2 characters). Empty means CodeView.
static void dump_COMENT_new_omf(FILE *fp,struct omf_record_t * const rec) {
    const unsigned char *p;
    const char *s = NULL;

    if (omf_record_eof(rec)) {
        fprintf(fp,"    %-18sCodeView\n","Style:");
        return;
    }

    fprintf(fp,"    %-18s%u\n","Version:",omf_record_get_byte(rec));

    p = rec->data + rec->recpos;
    if (omf_record_data_available(rec) == 2) {
        if (!memcmp(p,"CV",2))
            s = "CodeView";
        else if (!memcmp(p,"HL",2))
            s = "IBM HLL";
    }

    if (s != NULL) {
        fprintf(fp,"    %-18s\"%.2s\" %s\n","Style:",(const char*)p,s);
        omf_record_lseek(rec,rec->reclen);
    }
    else {
        dump_COMENT_rest(fp,"Style:",rec);
    }
}

// WKEXT and LZEXT: pairs of EXTDEF indexes, the weak or lazy extern and the default to resolve it to
static void dump_COMENT_wkext(FILE *fp,const struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    unsigned int ext,def;

    while (!omf_record_eof(rec)) {
        ext = omf_record_get_index(rec);
        def = omf_record_get_index(rec);
        fprintf(fp,"    %-18s\"%s\"(%u) default=\"%s\"(%u)\n","Extern:",
            omf_context_get_extdef_name_safe(ctx,ext),ext,
            omf_context_get_extdef_name_safe(ctx,def),def);
    }
}

// Borland dependency: DOS time (word), DOS date (word), file name. An empty one ends the list.
static void dump_COMENT_dependency(FILE *fp,struct omf_record_t * const rec) {
    unsigned int t,d;

    if (omf_record_eof(rec)) {
        fprintf(fp,"    End of dependency list\n");
        return;
    }

    if (omf_record_data_available(rec) < 5) return;
    t = omf_record_get_word(rec);
    d = omf_record_get_word(rec);
    if (omf_record_get_lenstr(omf_temp_str,sizeof(omf_temp_str),rec) < 0) return;

    fprintf(fp,"    %-18s\"%s\" %04u-%02u-%02u %02u:%02u:%02u\n","File:",omf_temp_str,
        1980u + (d >> 9u),(d >> 5u) & 0xFu,d & 0x1Fu,
        t >> 11u,(t >> 5u) & 0x3Fu,(t & 0x1Fu) * 2u);
}

// print the directive character of a Watcom linker or disassembler directive
static void dump_COMENT_directive(FILE *fp,const unsigned char c,const char * const what) {
    if (c >= 0x20 && c < 0x7F)
        fprintf(fp,"    %-18s'%c' %s\n","Directive:",(char)c,what);
    else
        fprintf(fp,"    %-18s0x%02X %s\n","Directive:",c,what);
}

// Watcom linker directive: a directive character, then what it needs
static void dump_COMENT_linker_directive(FILE *fp,const struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    unsigned int a,b;
    unsigned char c;

    if (omf_record_eof(rec)) return;
    c = omf_record_get_byte(rec);
    switch (c) {
        case 'D': // debug information version (major, minor bytes) and source language
            dump_COMENT_directive(fp,c,"debug information version and source language");
            if (omf_record_data_available(rec) < 2) break;
            a = omf_record_get_byte(rec);
            b = omf_record_get_byte(rec);
            fprintf(fp,"    %-18s%u.%u\n","Version:",a,b);
            dump_COMENT_rest(fp,"Language:",rec);
            break;
        case 'L': // default library: priority (byte) and name
            dump_COMENT_directive(fp,c,"default library");
            if (omf_record_eof(rec)) break;
            fprintf(fp,"    %-18s%u\n","Priority:",omf_record_get_byte(rec));
            dump_COMENT_rest(fp,"Library:",rec);
            break;
        case 'O': // optimize far calls in a segment (SEGDEF index)
            dump_COMENT_directive(fp,c,"optimize far calls");
            if (omf_record_eof(rec)) break;
            a = omf_record_get_index(rec);
            fprintf(fp,"    %-18s\"%s\"(%u)\n","Segment:",omf_context_get_segdef_name_safe(ctx,a),a);
            break;
        case 'U': // far call optimization is unsafe for the last FIXUPP
            dump_COMENT_directive(fp,c,"far call optimization unsafe for the last FIXUPP");
            break;
        case 'V': // virtual function table, and pure virtual function table:
        case 'P': //   EXTDEF index, default EXTDEF index, then the LNAMES indexes it is conditional on
            dump_COMENT_directive(fp,c,c == 'V' ? "virtual function table" : "pure virtual function table");
            if (omf_record_eof(rec)) break;
            a = omf_record_get_index(rec);
            b = omf_record_get_index(rec);
            fprintf(fp,"    %-18s\"%s\"(%u) default=\"%s\"(%u)\n","Extern:",
                omf_context_get_extdef_name_safe(ctx,a),a,
                omf_context_get_extdef_name_safe(ctx,b),b);
            while (!omf_record_eof(rec)) {
                a = omf_record_get_index(rec);
                fprintf(fp,"    %-18s\"%s\"(%u)\n","Conditional on:",omf_lnames_context_get_name_safe(&ctx->LNAMEs,a),a);
            }
            break;
        case 'R': // virtual function reference: EXTDEF index, then SEGDEF index, or 0 and the COMDAT's LNAMES index
            dump_COMENT_directive(fp,c,"virtual function reference");
            if (omf_record_eof(rec)) break;
            a = omf_record_get_index(rec);
            fprintf(fp,"    %-18s\"%s\"(%u)\n","Extern:",omf_context_get_extdef_name_safe(ctx,a),a);
            if (omf_record_eof(rec)) break;
            a = omf_record_get_index(rec);
            if (a != 0) {
                fprintf(fp,"    %-18s\"%s\"(%u)\n","Segment:",omf_context_get_segdef_name_safe(ctx,a),a);
            }
            else if (!omf_record_eof(rec)) {
                a = omf_record_get_index(rec);
                fprintf(fp,"    %-18s\"%s\"(%u)\n","COMDAT:",omf_lnames_context_get_name_safe(&ctx->LNAMEs,a),a);
            }
            break;
        case 'T': // timestamp (dword) of the object in a library
            dump_COMENT_directive(fp,c,"object timestamp");
            if (omf_record_data_available(rec) < 4) break;
            fprintf(fp,"    %-18s0x%08lX\n","Timestamp:",omf_record_get_dword(rec));
            break;
        case '7':
            dump_COMENT_directive(fp,c,"pack far data");
            break;
        case 'F':
            dump_COMENT_directive(fp,c,"debug addresses are flat");
            break;
        default:
            dump_COMENT_directive(fp,c,"?");
            break;
    }
}

// Watcom disassembler directive: a directive character, then what it needs
static void dump_COMENT_disasm_directive(FILE *fp,const struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    unsigned long start,end;
    unsigned int a;
    unsigned char c;

    if (omf_record_eof(rec)) return;
    c = omf_record_get_byte(rec);
    switch (c) {
        case 's': // scan table (data in code): SEGDEF index, or 0 and the COMDAT's LNAMES index,
        case 'S': //   then start and end+1 offsets (words for 's', dwords for 'S')
            dump_COMENT_directive(fp,c,"scan table");
            if (omf_record_eof(rec)) break;
            a = omf_record_get_index(rec);
            if (a != 0) {
                fprintf(fp,"    %-18s\"%s\"(%u)\n","Segment:",omf_context_get_segdef_name_safe(ctx,a),a);
            }
            else {
                if (omf_record_eof(rec)) break;
                a = omf_record_get_index(rec);
                fprintf(fp,"    %-18s\"%s\"(%u)\n","COMDAT:",omf_lnames_context_get_name_safe(&ctx->LNAMEs,a),a);
            }

            if (c == 'S') {
                if (omf_record_data_available(rec) < 8) break;
                start = omf_record_get_dword(rec);
                end = omf_record_get_dword(rec);
            }
            else {
                if (omf_record_data_available(rec) < 4) break;
                start = omf_record_get_word(rec);
                end = omf_record_get_word(rec);
            }

            fprintf(fp,"    %-18sstart=0x%lX end=0x%lX\n","Range:",start,end);
            break;
        default:
            dump_COMENT_directive(fp,c,"?");
            break;
    }
}

void dump_COMENT(FILE *fp,const struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    unsigned char comment_type,comment_class,subtype;

    fprintf(fp,"COMENT:\n");
    if (omf_record_data_available(rec) < 2) {
        fprintf(fp,"    [record too short]\n");
        return;
    }

    comment_type = omf_record_get_byte(rec);
    comment_class = omf_record_get_byte(rec);

    fprintf(fp,"    %-18s0x%02X","Comment Type:",comment_type);
    if (comment_type & OMF_COMENT_TYPE_NO_PURGE)
        fprintf(fp," NO-PURGE");
    if (comment_type & OMF_COMENT_TYPE_NO_LIST)
        fprintf(fp," NO-LIST");
    fprintf(fp,"\n");

    fprintf(fp,"    %-18s0x%02X %s\n","Comment Class:",comment_class,omf_coment_class_to_str(comment_class));

    switch (comment_class) {
        case OMF_COMENT_TRANSLATOR:
        case OMF_COMENT_COPYRIGHT:
        case OMF_COMENT_LIBRARY_SPEC:
        case OMF_COMENT_EXESTR:
        case OMF_COMENT_COMMAND_LINE:
            dump_COMENT_text(fp,"Text:",rec);
            break;
        case OMF_COMENT_DEFAULT_LIBRARY: // the name, without a length byte
            dump_COMENT_rest(fp,"Library:",rec);
            break;
        case OMF_COMENT_WATCOM_PROC_MODEL:
        case OMF_COMENT_MS_PROC_MODEL:
            dump_COMENT_proc_model(fp,rec);
            break;
        case OMF_COMENT_OMF_EXTENSION:
            if (omf_record_eof(rec)) break;
            subtype = omf_record_get_byte(rec);
            fprintf(fp,"    %-18s0x%02X %s\n","Subtype:",subtype,omf_coment_omfext_to_str(subtype));
            if (subtype == OMF_COMENT_OMFEXT_IMPDEF)
                dump_COMENT_impdef(fp,rec);
            else if (subtype == OMF_COMENT_OMFEXT_EXPDEF)
                dump_COMENT_expdef(fp,rec);
            break;
        case OMF_COMENT_NEW_OMF:
            dump_COMENT_new_omf(fp,rec);
            break;
        case OMF_COMENT_LINK_PASS:
            if (omf_record_eof(rec)) break;
            subtype = omf_record_get_byte(rec);
            fprintf(fp,"    %-18s0x%02X%s\n","Link pass:",subtype,subtype == 0x01 ? " (end of pass 1)" : "");
            break;
        case OMF_COMENT_WKEXT:
        case OMF_COMENT_LZEXT:
            dump_COMENT_wkext(fp,ctx,rec);
            break;
        case OMF_COMENT_EASY_OMF:
            dump_COMENT_rest(fp,"Signature:",rec);
            break;
        case OMF_COMENT_DEPENDENCY:
            dump_COMENT_dependency(fp,rec);
            break;
        case OMF_COMENT_DISASM_DIRECTIVE:
            dump_COMENT_disasm_directive(fp,ctx,rec);
            break;
        case OMF_COMENT_LINKER_DIRECTIVE:
            dump_COMENT_linker_directive(fp,ctx,rec);
            break;
    }

    // anything not decoded above
    dump_COMENT_rest(fp,"Data:",rec);
}

