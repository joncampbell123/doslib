
#include <sys/types.h>
#include <sys/stat.h>

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

static char ic_tmp[PATH_MAX];

static void help(void) {
    fprintf(stderr,"zip4dos [options] <files to archive>\n");
    fprintf(stderr,"A very simple ZIP archiver with MS-DOS/PKUNZIP.EXE in mind.\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"  --zip <path>             ZIP archive to write.\n");
    fprintf(stderr,"  -0                       Store only\n");
    fprintf(stderr,"  -1                       Fast compression\n");
    fprintf(stderr,"  -9                       Better compression\n");
    fprintf(stderr,"  -r                       Recurse into directories\n");
    fprintf(stderr,"  -s <size>                Generate spanning ZIP archives\n");
    fprintf(stderr,"  -ic <charset>            File names on host use this charset\n");
    fprintf(stderr,"  -oc <charset>            File names for target use this charset\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"Spanning size can be specified in bytes, or with K, M, G, suffix.\n");
    fprintf(stderr,"With spanning, the zip file must have .zip suffix, which will be changed\n");
    fprintf(stderr,"per fragment to z01, z02, etc.\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"No changes are made to the filename if neither -ic or -oc are specified.\n");
    fprintf(stderr,"If only -oc is specified, -ic is assumed from your locale.\n");
    fprintf(stderr,"If targeting US MS-DOS systems, you can use -oc CP437.\n");
    fprintf(stderr,"If targeting Japanese PC-98, use -oc CP932 or -oc SHIFT-JIS.\n");
    fprintf(stderr,"You will need to specify -ic and/or -oc before listing files to archive.\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"For simplistic reasons, this code only supports Deflate compression\n");
    fprintf(stderr,"with no password protection.\n");
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

#define ATTR_DOS_DIR    0x10            /* MS-DOS directory */

struct in_file {
    char*               in_path;
    char*               zip_name;
    unsigned char       attr;           /* MS-DOS attributes */
    unsigned long       file_size;
    struct in_file*     next;
} in_file;

struct in_file *in_file_alloc(void) {
    return (struct in_file*)calloc(1,sizeof(struct in_file));
}

char *in_file_set_in_path(struct in_file *f,char *s) {
    return set_string(&f->in_path,s);
}

char *in_file_set_zip_name(struct in_file *f,char *s) {
    return set_string(&f->zip_name,s);
}

void in_file_free(struct in_file *f) {
    clear_string(&f->in_path);
    clear_string(&f->zip_name);
    f->file_size = 0;
    f->attr = 0;
    f->next = NULL;
}

struct in_file *file_list_head = NULL;
struct in_file *file_list_tail = NULL; // for ref only to append quicker

void file_list_free(void) {
    while (file_list_head != NULL) {
        struct in_file *next = file_list_head->next;

        file_list_head->next = NULL;
        in_file_free(file_list_head);
        free(file_list_head);

        file_list_head = next;
    }

    file_list_head = file_list_tail = NULL;
}

void file_list_append(struct in_file *f) {
    assert(f != NULL);

    if (file_list_head != NULL) {
        assert(file_list_tail != NULL);
        assert(f->next == NULL);

        file_list_tail->next = f;
        file_list_tail = f;
    }
    else {
        file_list_head = file_list_tail = f;
    }
}

char *zip_path = NULL;
unsigned long spanning_size = 0;
int deflate_mode = 5; /* default deflate mode */
_Bool recurse = 0;
char *codepage_in = NULL;
char *codepage_out = NULL;

int parse_unit_amount(unsigned long *out,const char *s) {
    if (!isdigit(*s))
        return 0;

    /* value */
    *out = strtoull(s,(char**)(&s),10);

    /* suffix, if any */
    if (*out != 0) {
        char c = tolower(*out++);

        if (*out != 0)
            return 0;

        if (c == 'k')
            *out <<= 10ULL;
        else if (c == 'm')
            *out <<= 20ULL;
        else if (c == 'g')
            *out <<= 30ULL;
        return 0;
    }

    return 1;
}

static int parse(int argc,char **argv) {
    iconv_t ic = (iconv_t)-1;
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
            else if (!strcmp(a,"zip")) {
                if (zip_path != NULL) return 1;
                zip_path = strdup(argv[i++]);
                if (zip_path == NULL) return 1;
            }
            else if (!strcmp(a,"r")) {
                recurse = 1;
            }
            else if (!strcmp(a,"s")) {
                a = argv[i++];
                if (a == NULL) return 1;
                if (!parse_unit_amount(&spanning_size,a)) return 1;
            }
            else if (!strcmp(a,"ic")) {
                a = argv[i++];
                if (a == NULL) return 1;
                set_string(&codepage_in,a);

                if (ic != (iconv_t)-1) {
                    iconv_close(ic);
                    ic = (iconv_t)-1;
                }
            }
            else if (!strcmp(a,"oc")) {
                a = argv[i++];
                if (a == NULL) return 1;
                set_string(&codepage_out,a);

                if (ic != (iconv_t)-1) {
                    iconv_close(ic);
                    ic = (iconv_t)-1;
                }
            }
            else if (isdigit(*a)) {
                deflate_mode = (int)strtol(a,(char**)(&a),10);
                if (deflate_mode < 0 || deflate_mode > 9) return 1;
                if (*a != 0) return 1;
            }
            else {
                fprintf(stderr,"Unknown switch %s\n",a);
                return 1;
            }
        }
        else {
            /* file to archive */
            if (ic == (iconv_t)-1 && (codepage_out != NULL || codepage_in != NULL)) {
                if (codepage_out != NULL && codepage_in == NULL) {
                    codepage_in = strdup("UTF-8");
                    if (codepage_in == NULL) return 1;
                }

                ic = iconv_open(codepage_out,codepage_in);
                if (ic == (iconv_t)-1) {
                    fprintf(stderr,"Unable to open character encoding conversion from '%s' to '%s', '%s'\n",codepage_in,codepage_out,strerror(errno));
                    return 1;
                }
            }

            if (lstat(a,&st)) {
                fprintf(stderr,"Cannot stat %s, %s\n",a,strerror(errno));
                return 1;
            }
            if (!(S_ISREG(st.st_mode) || S_ISDIR(st.st_mode))) {
                fprintf(stderr,"Skipping non-file non-directory %s\n",a);
                continue;
            }
            if (st.st_size >= (off_t)((2UL << 31UL) - (1UL << 28UL))) { /* 2GB - 256MB */
                fprintf(stderr,"Skipping file %s, too large\n",a);
                continue;
            }

            {
                char *t,*ft;
                struct in_file *f = in_file_alloc();

                if (f == NULL) {
                    fprintf(stderr,"Out of memory\n");
                    return 1;
                }

                if (S_ISDIR(st.st_mode)) {
                    f->attr = ATTR_DOS_DIR;
                }
                else {
                    f->file_size = (unsigned long)st.st_size;
                    f->attr = 0;
                }

                if (in_file_set_in_path(f,a) == NULL) {
                    fprintf(stderr,"out of memory\n");
                    return 1;
                }

                /* now pick the ZIP name */
                t = strdup(a);
                if (t == NULL) return 1;

                /* (in case of future porting to MS-DOS) convert backwards slashes to forward slashes */
                {
                    char *ss;
                    for (ss=t;*ss!=0;ss++) {
                        if (*ss == '\\')
                            *ss = '/';
                    }
                }

                /* prevent absolute paths */
                ft = t;
                while (*ft == '/') ft++;

                /* NO single or double dots! */
                {
                    char *ss = ft,*n;
                    size_t chk;

                    while (*ss != 0) {
                        n = strchr(ss,'/');
                        if (n)
                            chk = (size_t)(n-ss);
                        else
                            chk = strlen(ss);

                        if ((chk == 1 && !strncmp(ss,".",  chk)) ||
                            (chk == 2 && !strncmp(ss,"..", chk))) {
                            fprintf(stderr,". or .. not allowed in the path\n");
                            return 1;
                        }

                        ss += chk;
                        if (*ss == '/') ss++;
                    }
                }

                if (ic != (iconv_t)-1) {
                    size_t inleft = strlen(ft);
                    size_t outleft = sizeof(ic_tmp)-1;
                    char *out = ic_tmp,*in = ft;
                    int ret;

                    ret = iconv(ic,&in,&inleft,&out,&outleft);
                    if (ret == -1 || inleft != (size_t)0 || outleft == (size_t)0) {
                        fprintf(stderr,"file name conversion error from '%s'. ret=%d inleft=%zu outleft=%zu\n",ft,ret,inleft,outleft);
                        return 1;
                    }
                    assert(out >= ic_tmp);
                    assert(out < (ic_tmp+sizeof(ic_tmp)));
                    *out = 0;

                    if (in_file_set_zip_name(f,ic_tmp) == NULL) {
                        fprintf(stderr,"out of memory\n");
                        return 1;
                    }
                }
                else {
                    if (in_file_set_zip_name(f,ft) == NULL) {
                        fprintf(stderr,"out of memory\n");
                        return 1;
                    }
                }

                free(t);
                file_list_append(f);
            }
        }
    }

    if (ic != (iconv_t)-1) {
        iconv_close(ic);
        ic = (iconv_t)-1;
    }

    if (zip_path == NULL) {
        fprintf(stderr,"Must specify ZIP file: --zip\n");
        return 1;
    }
    if (file_list_head == NULL) {
        fprintf(stderr,"Nothing to add\n");
        return 1;
    }

    if (recurse) {
        struct in_file *list;
        struct dirent *d;
        size_t sl;
        DIR *dir;

        if (ic == (iconv_t)-1 && (codepage_out != NULL || codepage_in != NULL)) {
            if (codepage_out != NULL && codepage_in == NULL) {
                codepage_in = strdup("UTF-8");
                if (codepage_in == NULL) return 1;
            }

            ic = iconv_open(codepage_out,codepage_in);
            if (ic == (iconv_t)-1) {
                fprintf(stderr,"Unable to open character encoding conversion from '%s' to '%s', '%s'\n",codepage_in,codepage_out,strerror(errno));
                return 1;
            }
        }

        for (list=file_list_head;list;list=list->next) {
            if (list->attr & ATTR_DOS_DIR) {
                printf("Scanning: %s\n",list->in_path);

                dir = opendir(list->in_path);
                if (dir == NULL) {
                    fprintf(stderr,"Unable to open dir %s\n",list->in_path);
                    return 1;
                }

                while ((d=readdir(dir)) != NULL) {
                    if (d->d_name[0] == '.') continue;

                    sl = snprintf(ic_tmp,sizeof(ic_tmp),"%s/%s",list->in_path,d->d_name);
                    if (sl >= sizeof(ic_tmp)) {
                        fprintf(stderr,"Cannot recurse, new path too long\n");
                        return 1;
                    }

                    if (lstat(ic_tmp,&st)) {
                        fprintf(stderr,"Cannot stat %s, %s\n",ic_tmp,strerror(errno));
                        return 1;
                    }
                    if (!(S_ISREG(st.st_mode) || S_ISDIR(st.st_mode))) {
                        fprintf(stderr,"Skipping non-file non-directory %s\n",ic_tmp);
                        continue;
                    }
                    if (st.st_size >= (off_t)((2UL << 31UL) - (1UL << 28UL))) { /* 2GB - 256MB */
                        fprintf(stderr,"Skipping file %s, too large\n",ic_tmp);
                        continue;
                    }

                    {
                        char *t,*ft;
                        struct in_file *f = in_file_alloc();

                        if (f == NULL) {
                            fprintf(stderr,"Out of memory\n");
                            return 1;
                        }

                        if (S_ISDIR(st.st_mode)) {
                            f->attr = ATTR_DOS_DIR;
                        }
                        else {
                            f->file_size = (unsigned long)st.st_size;
                            f->attr = 0;
                        }

                        if (in_file_set_in_path(f,ic_tmp) == NULL) {
                            fprintf(stderr,"out of memory\n");
                            return 1;
                        }

                        /* now pick the ZIP name */
                        t = strdup(ic_tmp);
                        if (t == NULL) return 1;

                        /* (in case of future porting to MS-DOS) convert backwards slashes to forward slashes */
                        {
                            char *ss;
                            for (ss=t;*ss!=0;ss++) {
                                if (*ss == '\\')
                                    *ss = '/';
                            }
                        }

                        /* prevent absolute paths */
                        ft = t;
                        while (*ft == '/') ft++;

                        /* NO single or double dots! */
                        {
                            char *ss = ft,*n;
                            size_t chk;

                            while (*ss != 0) {
                                n = strchr(ss,'/');
                                if (n)
                                    chk = (size_t)(n-ss);
                                else
                                    chk = strlen(ss);

                                if ((chk == 1 && !strncmp(ss,".",  chk)) ||
                                    (chk == 2 && !strncmp(ss,"..", chk))) {
                                    fprintf(stderr,". or .. not allowed in the path\n");
                                    return 1;
                                }

                                ss += chk;
                                if (*ss == '/') ss++;
                            }
                        }

                        if (ic != (iconv_t)-1) {
                            size_t inleft = strlen(ft);
                            size_t outleft = sizeof(ic_tmp)-1;
                            char *out = ic_tmp,*in = ft;
                            int ret;

                            ret = iconv(ic,&in,&inleft,&out,&outleft);
                            if (ret == -1 || inleft != (size_t)0 || outleft == (size_t)0) {
                                fprintf(stderr,"file name conversion error from '%s'. ret=%d inleft=%zu outleft=%zu\n",ft,ret,inleft,outleft);
                                return 1;
                            }
                            assert(out >= ic_tmp);
                            assert(out < (ic_tmp+sizeof(ic_tmp)));
                            *out = 0;

                            if (in_file_set_zip_name(f,ic_tmp) == NULL) {
                                fprintf(stderr,"out of memory\n");
                                return 1;
                            }
                        }
                        else {
                            if (in_file_set_zip_name(f,ft) == NULL) {
                                fprintf(stderr,"out of memory\n");
                                return 1;
                            }
                        }

                        free(t);
                        file_list_append(f);
                    }
                }

                closedir(dir);
            }
        }
    }

    if (ic != (iconv_t)-1) {
        iconv_close(ic);
        ic = (iconv_t)-1;
    }

    {
        struct in_file *list;

        for (list=file_list_head;list;list=list->next) {
            assert(list->in_path != NULL);
            assert(list->zip_name != NULL);
            printf("%s: %s\n",
                deflate_mode==0?"Storing":"Deflating",list->in_path);
        }
    }

    clear_string(&codepage_out);
    clear_string(&codepage_in);
    clear_string(&zip_path);
    file_list_free();
    return 0;
}

int main(int argc,char **argv) {
    if (parse(argc,argv))
        return 1;

    return 0;
}

