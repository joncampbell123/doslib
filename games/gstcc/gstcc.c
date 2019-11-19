
#include <sys/types.h>
#include <sys/stat.h>

#include <time.h>
#include <stdint.h>
#include <endian.h>
#include <dirent.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>

#include "zlib.h"
#include "iconv.h"

#ifndef O_BINARY
#define O_BINARY (0)
#endif

FILE*           in_fp = NULL;
char            in_line[4096];
char*           in_line_r = NULL;

char*           in_file = NULL;
char*           out_file = NULL;
char*           out_codepage = NULL;
int32_t         out_codepage_num = -1;

iconv_t         iconv_context = (iconv_t)-1;

#define MAX_STRINGS     8192

typedef struct out_str_t {
    unsigned char       *data;
    unsigned int        data_len;
    unsigned int        data_ofs;
} out_str_t;

unsigned int    out_string_base = 1;
unsigned int    out_string_count = 0;
out_str_t       out_string[MAX_STRINGS];

char            out_str_tmp[4096];
char*           out_str_tmp_w = 0;

int in_gl(void) {
    if (in_line_r == in_line)
        return 1;

    if (in_fp != NULL) {
        memset(in_line,0,16); // keep the first 16 bytes NULL

        if (feof(in_fp))
            return 0;
        if (ferror(in_fp))
            return -1;
        if (fgets(in_line,sizeof(in_line),in_fp) == NULL)
            return 0;

        /* eat the newline */
        {
            char *s = in_line + strlen(in_line) - 1;
            while (s >= in_line && (*s == '\r' || *s == '\n')) *s-- = 0;
        }

        in_line_r = in_line;
        return 1;
    }

    return 0;
}

void in_gl_discard(void) {
    in_line_r = in_line + 1;
}

int in_gl_p(void) {
    int r;

    while ((r=in_gl()) == 1) {
        if (in_line[0] == ';') { /* skip comments */
            in_gl_discard();
            continue;
        }

        break;
    }

    return r;
}

static void help(void) {
    fprintf(stderr,"DOSLIB game string table compiler\n");
    fprintf(stderr,"gstcc -i <input> -o <output> [-e codepage]\n");
    fprintf(stderr,"To get a complete list of code pages see iconv --list.\n");
    fprintf(stderr,"Input file is assumed to be UTF-8 and will be encoded to the target\n");
    fprintf(stderr,"code page on compile.\n");
}

void clear_string(char **a) {
    if (*a != NULL) {
        free(*a);
        *a = NULL;
    }
}

char *set_string(char **a,const char *s) {
    clear_string(a);

    if (s != NULL) {
        assert(*a == NULL);
        *a = strdup(s);
    }

    return *a;
}

int32_t codepage_to_num(const char *s) {
    if (!strncmp(s,"CP",2) && isdigit(s[2])) {
        // CP437, CP932, etc.
        return (int32_t)atol(s+2);
    }
    if (!strcmp(s,"UTF-8")) {
        return (int32_t)65001;//microsoft
    }

    return -1;
}

static int parse(int argc,char **argv) {
    struct stat st;
    char *a;
    int i;

    if (argc <= 1) {
        help();
        return 1;
    }

    for (i=1;i < argc;) {
        a = argv[i++];

        if (*a == '-') {
            do { a++; } while (*a == '-');

            if (!strcmp(a,"h") || !strcmp(a,"help")) {
                help();
                return 1;
            }
            else if (!strcmp(a,"i")) {
                a = argv[i++];
                if (a == NULL) return 1;
                set_string(&in_file,a);
            }
            else if (!strcmp(a,"o")) {
                a = argv[i++];
                if (a == NULL) return 1;
                set_string(&out_file,a);
            }
            else if (!strcmp(a,"e")) {
                a = argv[i++];
                if (a == NULL) return 1;
                set_string(&out_codepage,a);
            }
            else {
                fprintf(stderr,"Unknown switch %s\n",a);
                return 1;
            }
        }
        else {
            fprintf(stderr,"Unexpected arg %s\n",a);
            return 1;
        }
    }

    if (in_file == NULL) {
        fprintf(stderr,"Input file -i required\n");
        return 1;
    }
    if (out_file == NULL) {
        fprintf(stderr,"Output file -o required\n");
        return 1;
    }

    if (stat(in_file,&st)) {
        fprintf(stderr,"Cannot stat input file %s, %s\n",in_file,strerror(errno));
        return 1;
    }
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr,"%s is not a file\n",in_file);
        return 1;
    }

    return 0;
}

int prep_body(void) {
    if (out_codepage == NULL)
        set_string(&out_codepage,"CP437");

    assert(out_codepage != NULL);

    out_codepage_num = codepage_to_num(out_codepage);
    if (out_codepage_num < 0) {
        fprintf(stderr,"Unknown codepage %s\n",out_codepage);
        return -1;
    }

    fprintf(stderr,"Encoding: %s (code page %d)\n",out_codepage,out_codepage_num);
    return 0;
}

void split_name_value(char **name,char **value,char *s) {
    *name = s;
    *value = strchr(s,'=');
    if (*value != NULL)
        *((*value)++) = 0;
    else
        *value = s + strlen(s);
}

int read_header(void) {
    char *name,*value;
    int r;

    while ((r=in_gl_p()) == 1) {
        if (in_line[0] != '!')
            break;

        split_name_value(&name,&value,in_line+1/*skip!*/);
        in_gl_discard();

        if (!strcmp(name,"codepage"))
            set_string(&out_codepage,value);
    }

    assert(iconv_context == (iconv_t)-1);
    iconv_context = iconv_open(out_codepage,"UTF-8");
    if (iconv_context == (iconv_t)-1) {
        fprintf(stderr,"Unable to open iconv context\n");
        return -1;
    }

    return 0;
}

int out_string_alloc_slot(unsigned int idx) {
    while (out_string_count <= idx) {
        out_string[out_string_count].data = NULL;
        out_string[out_string_count].data_len = 0;
        out_string_count++;
    }

    return 0;
}

int encode_string_to_slot(unsigned int idx,char *s) {
    size_t ol = sizeof(out_str_tmp) - 1;
    size_t sl = strlen(s);
    int ret;

    if (idx > out_string_count)
        return -1;

    out_str_tmp_w = out_str_tmp;

    ret = iconv(iconv_context,&s,&sl,&out_str_tmp_w,&ol);
    if (ret < 0 || sl != 0 || ol == 0) {
        fprintf(stderr,"Iconv conversion error\n");
        return -1;
    }
    *out_str_tmp_w = 0;

    if (out_string[idx].data) {
        free(out_string[idx].data);
        out_string[idx].data = NULL;
        out_string[idx].data_len = 0;
    }

    assert(out_str_tmp <= out_str_tmp_w);

    out_string[idx].data_len = (unsigned int)(out_str_tmp_w - out_str_tmp);
    if (out_string[idx].data_len != 0) {
        out_string[idx].data = malloc(out_string[idx].data_len);
        if (out_string[idx].data == NULL) {
            fprintf(stderr,"Cannot allocate string memory\n");
            return -1;
        }
        memcpy(out_string[idx].data,out_str_tmp,out_string[idx].data_len);
    }
    else {
        out_string[idx].data = NULL;
    }

    return 0;
}

int read_body(void) {
    char *name,*value;
    int r;

    while ((r=in_gl_p()) == 1) {
        split_name_value(&name,&value,in_line);
        in_gl_discard();

        if (isdigit(*name)) {
            unsigned long idx = strtoul(name,&name,0);
            if (idx == 0) {
                fprintf(stderr,"Index 0 is reserved\n");
                return -1;
            }
            idx--;
            if (idx >= (unsigned long)MAX_STRINGS) {
                fprintf(stderr,"Index too large\n");
                return -1;
            }

            if (*name != 0) {
                fprintf(stderr,"Extra junk after number\n");
                return -1;
            }

            if (out_string_alloc_slot((unsigned int)idx) < 0) {
                fprintf(stderr,"Failed to alloc string slot\n");
                return -1;
            }

            if (encode_string_to_slot((unsigned int)idx,value) < 0) {
                fprintf(stderr,"String encoding failure for string #%lu\n",idx);
                return -1;
            }
        }
    }

    return 0;
}

void close_in(void) {
    if (iconv_context != (iconv_t)-1) {
        iconv_close(iconv_context);
        iconv_context = (iconv_t)-1;
    }

    fclose(in_fp);
    in_fp = NULL;
}

/* header:
 * char         "ST0U"
 * DWORD        code page
 * WORD         string first index
 * WORD         string count
 */

int proc_strings(void) {
    unsigned long ofs = 12;
    unsigned int i;

    for (i=0;i < out_string_count;i++) {
        if (out_string[i].data_len != 0)
            break;
    }
    out_string_base = i;

    ofs += (out_string_count - out_string_base) * 2; // length of string 16-bit

    for (i=out_string_base;i < out_string_count;i++) {
        out_string[i].data_ofs = ofs;
        ofs += out_string[i].data_len;
    }

    fprintf(stderr,"String table defines strings %u <= x <= %u\n",out_string_base+1,out_string_count);
    fprintf(stderr,"String table size: %lu bytes\n",ofs);

    if (ofs >= 0xFFF0) { // This is for MS-DOS 16-bit so it's gotta fit into a 64KB segment
        fprintf(stderr,"String table to big.\n");
        return -1;
    }

    return 0;
}

void write16le(unsigned char *d,uint16_t v) {
    d[0] = (unsigned char)( v        & 0xFFu);
    d[1] = (unsigned char)((v >> 8u) & 0xFFu);
}

void write32le(unsigned char *d,uint32_t v) {
    d[0] = (unsigned char)( v          & 0xFFul);
    d[1] = (unsigned char)((v >>  8ul) & 0xFFul);
    d[2] = (unsigned char)((v >> 16ul) & 0xFFul);
    d[3] = (unsigned char)((v >> 24ul) & 0xFFul);
}

int write_out(void) {
    unsigned char tmp[12];
    unsigned int i;
    FILE *fp;

    fp = fopen(out_file,"wb");
    if (fp == NULL) {
        fprintf(stderr,"Cannot open output file %s\n",out_file);
        return -1;
    }

    // header
    memcpy(tmp,"ST0U",4);
    write32le(tmp+4,out_codepage_num);
    write16le(tmp+8,out_string_base);
    write16le(tmp+10,out_string_count - out_string_base);
    fwrite(tmp,12,1,fp);

    // list
    for (i=out_string_base;i < out_string_count;i++) {
        write16le(tmp,out_string[i].data_len);
        fwrite(tmp,2,1,fp);
    }

    // strings
    for (i=out_string_base;i < out_string_count;i++) {
        if (out_string[i].data_len != 0)
            fwrite(out_string[i].data,out_string[i].data_len,1,fp);
    }

    fclose(fp);
    return 0;
}

int main(int argc,char **argv) {
    if (parse(argc,argv))
        return 1;

    // begin reading
    in_fp = fopen(in_file,"rb");
    if (in_fp == NULL) {
        fprintf(stderr,"Unable to open input file %s\n",in_file);
        return 1;
    }

    // read header
    if (read_header() < 0)
        return 1;

    // prepare to read body
    if (prep_body() < 0)
        return 1;

    // read body
    if (read_body() < 0)
        return 1;

    // process strings
    if (proc_strings() < 0)
        return 1;

    // done loading
    close_in();

    // write output
    if (write_out() < 0)
        return 1;

    return 0;
}

