/* NTS:
 *
 * I think I need to explain why I would waste my time writing this.
 *
 * InfoZIP.
 *
 * As useful as the tool is, it has the flaw that it makes ZIP archives
 * for use with Unix/Linux, not MS-DOS. The other reason is that it makes
 * ZIP archives that generally work with PKUNZIP.EXE, except that the
 * spanning mode doesn't work with PKUNZIP.EXE.
 *
 * As a bonus, this code uses GNU libiconv to allow you to host your
 * files in UTF-8 but convert filenames to a codepage that DOS supports.
 * The most common cases given in the help text is converting the file
 * names to US MS-DOS CP437, or Japanese PC-98 CP932/Shift-JIS, or any
 * other MS-DOS code page of your choice.
 *
 * If it's not clear, this ZIP archiver is focused on generating ZIP
 * archives that work with PKUNZIP v2.x and MS-DOS. */

#include <sys/types.h>
#include <sys/stat.h>

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

#include "zipcrc.h"

#ifndef O_BINARY
#define O_BINARY (0)
#endif

#pragma pack(push,1)
# define PKZIP_LOCAL_FILE_HEADER_SIG        (0x04034B50UL)

struct pkzip_local_file_header_main { /* PKZIP APPNOTE 2.0: General Format of a ZIP file section A */
    uint32_t                sig;                            /* 4 bytes  +0x00 0x04034B50 = 'PK\x03\x04' */
    uint16_t                version_needed_to_extract;      /* 2 bytes  +0x04 version needed to extract */
    uint16_t                general_purpose_bit_flag;       /* 2 bytes  +0x06 general purpose bit flag */
    uint16_t                compression_method;             /* 2 bytes  +0x08 compression method */
    uint16_t                last_mod_file_time;             /* 2 bytes  +0x0A */
    uint16_t                last_mod_file_date;             /* 2 bytes  +0x0C */
    uint32_t                crc32;                          /* 4 bytes  +0x0E */
    uint32_t                compressed_size;                /* 4 bytes  +0x12 */
    uint32_t                uncompressed_size;              /* 4 bytes  +0x16 */
    uint16_t                filename_length;                /* 2 bytes  +0x1A */
    uint16_t                extra_field_length;             /* 2 bytes  +0x1C */
};                                                          /*          =0x1E */
/* filename and extra field follow, then file data */
#pragma pack(pop)

#pragma pack(push,1)
# define PKZIP_CENTRAL_DIRECTORY_HEADER_SIG (0x02014B50UL)

struct pkzip_central_directory_header_main { /* PKZIP APPNOTE 2.0: General Format of a ZIP file section C */
    uint32_t                sig;                            /* 4 bytes  +0x00 0x02014B50 = 'PK\x01\x02' */
    uint16_t                version_made_by;                /* 2 bytes  +0x04 version made by */
    uint16_t                version_needed_to_extract;      /* 2 bytes  +0x06 version needed to extract */
    uint16_t                general_purpose_bit_flag;       /* 2 bytes  +0x08 general purpose bit flag */
    uint16_t                compression_method;             /* 2 bytes  +0x0A compression method */
    uint16_t                last_mod_file_time;             /* 2 bytes  +0x0C */
    uint16_t                last_mod_file_date;             /* 2 bytes  +0x0E */
    uint32_t                crc32;                          /* 4 bytes  +0x10 */
    uint32_t                compressed_size;                /* 4 bytes  +0x14 */
    uint32_t                uncompressed_size;              /* 4 bytes  +0x18 */
    uint16_t                filename_length;                /* 2 bytes  +0x1C */
    uint16_t                extra_field_length;             /* 2 bytes  +0x1E */
    uint16_t                file_comment_length;            /* 2 bytes  +0x20 */
    uint16_t                disk_number_start;              /* 2 bytes  +0x22 */
    uint16_t                internal_file_attributes;       /* 2 bytes  +0x24 */
    uint32_t                external_file_attributes;       /* 4 bytes  +0x26 */
    uint32_t                relative_offset_of_local_header;/* 4 bytes  +0x2A */
};                                                          /*          =0x2E */
/* filename and extra field follow, then file data */
#pragma pack(pop)

#pragma pack(push,1)
# define PKZIP_CENTRAL_DIRECTORY_END_SIG    (0x06054B50UL)

struct pkzip_central_directory_header_end { /* PKZIP APPNOTE 2.0: General Format of a ZIP file section C */
    uint32_t                sig;                            /* 4 bytes  +0x00 0x06054B50 = 'PK\x05\x06' */
    uint16_t                number_of_this_disk;            /* 2 bytes  +0x04 */
    uint16_t                number_of_disk_with_start_of_central_directory;
                                                            /* 2 bytes  +0x06 */
    uint16_t                total_number_of_entries_of_central_dir_on_this_disk;
                                                            /* 2 bytes  +0x08 */
    uint16_t                total_number_of_entries_of_central_dir;
                                                            /* 2 bytes  +0x0A */
    uint32_t                size_of_central_directory;      /* 4 bytes  +0x0C */
    uint32_t                offset_of_central_directory_from_start_disk;
                                                            /* 4 bytes  +0x10 */
    uint16_t                zipfile_comment_length;         /* 2 bytes  +0x14 */
};                                                          /*          =0x16 */
/* filename and extra field follow, then file data */
#pragma pack(pop)

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

int                     zip_fd = -1;
unsigned char           zip_cdir_start_disk = 0;

#define DISK_MAX        99

struct disk_info {
    unsigned long       byte_count;     /* byte offset of overall ZIP archive, at start of disk */
} disk_info;

struct disk_info        disk[DISK_MAX];
int                     disk_count = 0;

int disk_current_number(void) {
    return disk_count - 1;
}

struct disk_info *disk_get(int n) {
    if (n < 0 || n >= DISK_MAX)
        abort(); /* should not happen */

    return &disk[n];
}

struct disk_info *disk_current(void) {
    if (disk_count <= 0 || disk_count > DISK_MAX)
        abort(); /* should not happen */

    return &disk[disk_count-1];
}

struct disk_info *disk_new(void) {
    struct disk_info *d;

    if (disk_count >= DISK_MAX)
        return NULL;

    d = &disk[disk_count++];
    memset(d,0,sizeof(*d));
    return d;
}

#define ATTR_DOS_DIR    0x10            /* MS-DOS directory */

struct in_file {
    char*               in_path;
    char*               zip_name;
    unsigned char       attr;           /* MS-DOS attributes */
    unsigned char       disk_number;    /* disk number the file starts on */
    unsigned long       file_size;
    unsigned long       abs_offset;     /* absolute offset (from start of ZIP archive) of local file header */
    unsigned long       disk_offset;    /* offset relative to starting disk of local file header */
    unsigned long       compressed_size;
    unsigned short      msdos_time,msdos_date;
    uint32_t            crc32;
    struct in_file*     next;

    _Bool               data_descriptor;/* write data descriptor after file */
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
    memset(f,0,sizeof(*f));
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

unsigned long zip_out_pos(void) {
    if (zip_fd >= 0)
        return (unsigned long)lseek(zip_fd,0,SEEK_CUR);

    return 0UL;
}

int zip_out_open(void) {
    if (zip_fd < 0)
        zip_fd = open(zip_path,O_RDWR|O_CREAT|O_TRUNC|O_BINARY,0644);

    return (zip_fd >= 0);
}

void zip_out_close(void) {
    if (zip_fd >= 0) {
        close(zip_fd);
        zip_fd = -1;
    }
}

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

int zip_store(struct pkzip_local_file_header_main *lfh,struct in_file *list) {
    size_t buffer_sz = 16384; /* should be good */
    unsigned long total = 0;
    zipcrc_t crc32;
    char *buffer;
    int src_fd;
    int rd;

    crc32 = zipcrc_init();

    assert(list->in_path != NULL);

    lfh->uncompressed_size = list->file_size;
    if (list->file_size == 0)
        return 0; // nothing to do

    src_fd = open(list->in_path,O_RDONLY|O_BINARY);
    if (src_fd < 0) {
        fprintf(stderr,"Cannot open %s, %s\n",list->in_path,strerror(errno));
        return -1;
    }

    buffer = malloc(buffer_sz);
    if (buffer == NULL) {
        fprintf(stderr,"out of memory\n");
        close(src_fd);
        return -1;
    }

    while ((rd=read(src_fd,buffer,buffer_sz)) > 0) {
        assert(zip_fd >= 0);
        if (write(zip_fd,buffer,rd) != rd)
            return 1;

        crc32 = zipcrc_update(crc32,buffer,rd);
        total += (unsigned long)rd;
    }

    lfh->crc32 = list->crc32 = zipcrc_finalize(crc32);
    list->compressed_size = lfh->compressed_size = total;
    close(src_fd);
    free(buffer);
    return 0;
}

int zip_deflate(struct pkzip_local_file_header_main *lfh,struct in_file *list) {
    size_t inbuffer_sz = 32768,outbuffer_sz = 32768;
    char *inbuffer,*outbuffer;
    unsigned long total = 0;
    zipcrc_t crc32;
    z_stream z;
    int src_fd;
    size_t wd;
    int rd;

    crc32 = zipcrc_init();

    memset(&z,0,sizeof(z));
    assert(list->in_path != NULL);

    lfh->uncompressed_size = list->file_size;
    if (list->file_size == 0)
        return 0; // nothing to do

    src_fd = open(list->in_path,O_RDONLY|O_BINARY);
    if (src_fd < 0) {
        fprintf(stderr,"Cannot open %s, %s\n",list->in_path,strerror(errno));
        return -1;
    }

    outbuffer = malloc(outbuffer_sz);
    inbuffer = malloc(inbuffer_sz);
    if (inbuffer == NULL || outbuffer == NULL) {
        fprintf(stderr,"out of memory\n");
        close(src_fd);
        return -1;
    }

    if (deflateInit2(&z,deflate_mode,Z_DEFLATED,-15/*window, raw*/,8/*memlevel*/,Z_DEFAULT_STRATEGY) != Z_OK) {
        fprintf(stderr,"out of memory\n");
        close(src_fd);
        return -1;
    }

    while ((rd=read(src_fd,inbuffer,inbuffer_sz)) > 0) {
        crc32 = zipcrc_update(crc32,inbuffer,rd);

        z.avail_in = rd;
        z.next_in = (unsigned char*)inbuffer;
        while (z.avail_in > 0) {
            char *pin = (char*)z.next_in;

            z.next_out = (unsigned char*)outbuffer;
            z.avail_out = outbuffer_sz;

            if (deflate(&z,Z_NO_FLUSH) != Z_OK) {
                fprintf(stderr,"deflate() error\n");
                break;
            }

            assert((char*)z.next_out >= outbuffer);
            assert((char*)z.next_out <= (outbuffer+outbuffer_sz));
            wd = (size_t)((char*)z.next_out - (char*)outbuffer);
            if (wd > 0) {
                if ((size_t)write(zip_fd,outbuffer,wd) != wd) {
                    fprintf(stderr,"write error\n");
                    break;
                }

                total += wd;
            }
            else if ((char*)z.next_in == pin) {
                fprintf(stderr,"zlib locked up\n");
                break;
            }
        }
    }

    do {
        int x;

        z.avail_in = 0;
        z.next_in = (unsigned char*)inbuffer;
        z.avail_out = outbuffer_sz;
        z.next_out = (unsigned char*)outbuffer;

        x = deflate(&z,Z_FINISH);

        assert((char*)z.next_out >= outbuffer);
        assert((char*)z.next_out <= (outbuffer+outbuffer_sz));
        wd = (size_t)((char*)z.next_out - (char*)outbuffer);
        if (wd > 0) {
            if ((size_t)write(zip_fd,outbuffer,wd) != wd) {
                fprintf(stderr,"write error\n");
                break;
            }

            total += wd;
        }

        if (x == Z_STREAM_END)
            break;
        else if (x != Z_OK) {
            fprintf(stderr,"zlib Z_FINISH error\n");
            break;
        }
    } while (1);

    if (deflateEnd(&z) != Z_OK)
        fprintf(stderr,"deflateEnd() error\n");

    lfh->crc32 = list->crc32 = zipcrc_finalize(crc32);
    list->compressed_size = lfh->compressed_size = total;
    close(src_fd);
    free(inbuffer);
    free(outbuffer);
    return 0;
}

static int parse(int argc,char **argv) {
    unsigned int zip_cdir_total_count = 0;
    unsigned int zip_cdir_last_count = 0;
    unsigned long zip_cdir_byte_count = 0;
    unsigned long zip_cdir_offset = 0;
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
        struct disk_info *d = disk_new();
        if (d == NULL) return 1;
        d->byte_count = 0;
    }

    {
        struct pkzip_local_file_header_main lhdr;
        struct in_file *list;

        for (list=file_list_head;list;list=list->next) {
            assert(list->in_path != NULL);
            assert(list->zip_name != NULL);
            printf("%s: %s\n",
                deflate_mode==0?"Storing":"Deflating",list->in_path);

            memset(&lhdr,0,sizeof(lhdr));
            lhdr.sig = PKZIP_LOCAL_FILE_HEADER_SIG;
            lhdr.version_needed_to_extract = 20;        /* PKZIP 2.0 or higher */
            lhdr.general_purpose_bit_flag = (0 << 1);   /* just lie and say that "normal" deflate was used */

            if (deflate_mode > 0)
                lhdr.compression_method = 8; /* deflate */
            else
                lhdr.compression_method = 0; /* stored (no compression) */

            lhdr.last_mod_file_time = list->msdos_time;
            lhdr.last_mod_file_date = list->msdos_date;
            /* some fields we'll go back and write later */
            lhdr.filename_length = strlen(list->zip_name);

            /* data descriptors are valid ONLY for deflate */
            if (lhdr.compression_method != 8)
                list->data_descriptor = 0;

            if (!list->data_descriptor)
                lhdr.uncompressed_size = list->file_size;
            else {
                lhdr.general_purpose_bit_flag |= (1 << 3);
                lhdr.uncompressed_size = 0;
                lhdr.compressed_size = 0;
            }

            /* get writing! */
            if (!zip_out_open())
                return 1;

            /* write local file header */
            assert(zip_fd >= 0);
            list->disk_number = disk_current_number();
            list->disk_offset = zip_out_pos();
            list->abs_offset = disk_current()->byte_count + list->disk_offset;
            assert(sizeof(lhdr) == 30);
            if (write(zip_fd,&lhdr,sizeof(lhdr)) != sizeof(lhdr))
                return 1;

            if (lhdr.filename_length != 0) {
                if (write(zip_fd,list->zip_name,lhdr.filename_length) != lhdr.filename_length)
                    return 1;
            }

            /* store, if a file */
            if (!(list->attr & ATTR_DOS_DIR)) {
                if (lhdr.compression_method == 8) {
                    if (zip_deflate(&lhdr,list))
                        return 1;
                }
                else if (lhdr.compression_method == 0) {
                    if (zip_store(&lhdr,list))
                        return 1;
                }
                else {
                    abort();
                }

                if (list->data_descriptor) {
                    uint32_t x;

                    /* write a data descriptor */
                    assert((lhdr.general_purpose_bit_flag & (1 << 3)) != 0);

                    /* Um.... question: Why have a CRC-32 field if apparently PKZip and InfoZip require this to be zero?? */
                    lhdr.crc32 = list->crc32 = x = 0;
                    if (write(zip_fd,&x,4) != 4)
                        return 1;

                    x = lhdr.compressed_size;
                    if (write(zip_fd,&x,4) != 4)
                        return 1;

                    x = lhdr.uncompressed_size;
                    if (write(zip_fd,&x,4) != 4)
                        return 1;
                }
                else {
                    /* go back and write the lhdr again */
                    if ((unsigned long)lseek(zip_fd,list->disk_offset,SEEK_SET) != list->disk_offset)
                        return 1;
                    if (write(zip_fd,&lhdr,sizeof(lhdr)) != sizeof(lhdr))
                        return 1;

                    lseek(zip_fd,0,SEEK_END);
                }
            }
        }
    }

    /* write central directory */
    {
        struct pkzip_central_directory_header_main chdr;
        struct in_file *list;

        for (list=file_list_head;list;list=list->next) {
            assert(list->in_path != NULL);
            assert(list->zip_name != NULL);

            memset(&chdr,0,sizeof(chdr));
            chdr.sig = PKZIP_CENTRAL_DIRECTORY_HEADER_SIG;
            chdr.version_made_by = 0;                   /* MS-DOS */
            chdr.version_needed_to_extract = 20;        /* PKZIP 2.0 or higher */
            chdr.general_purpose_bit_flag = (0 << 1);   /* just lie and say that "normal" deflate was used */

            if (list->data_descriptor)
                chdr.general_purpose_bit_flag |= (1 << 3);

            if (deflate_mode > 0)
                chdr.compression_method = 8; /* deflate */
            else
                chdr.compression_method = 0; /* stored (no compression) */

            chdr.last_mod_file_time = list->msdos_time;
            chdr.last_mod_file_date = list->msdos_date;
            chdr.compressed_size = list->compressed_size;
            chdr.uncompressed_size = list->file_size;
            chdr.filename_length = strlen(list->zip_name);
            chdr.disk_number_start = list->disk_number;
            chdr.internal_file_attributes = 0;
            chdr.external_file_attributes = list->attr;
            chdr.relative_offset_of_local_header = list->disk_offset;
            chdr.crc32 = list->crc32;

            if (list->data_descriptor)
                chdr.general_purpose_bit_flag |= (1 << 3);

            /* get writing! */
            if (!zip_out_open())
                return 1;

            if (list == file_list_head) {
                zip_cdir_start_disk = disk_current_number();
                zip_cdir_offset = disk_current()->byte_count + zip_out_pos();
            }

            /* write file header */
            assert(zip_fd >= 0);
 
            assert(sizeof(chdr) == 46);
            zip_cdir_byte_count += sizeof(chdr);
            if (write(zip_fd,&chdr,sizeof(chdr)) != sizeof(chdr))
                return 1;

            if (chdr.filename_length != 0) {
                zip_cdir_byte_count += chdr.filename_length;
                if (write(zip_fd,list->zip_name,chdr.filename_length) != chdr.filename_length)
                    return 1;
            }

            zip_cdir_last_count++;
            zip_cdir_total_count++;
        }
    }

    {
        struct pkzip_central_directory_header_end ehdr;

        memset(&ehdr,0,sizeof(ehdr));
        ehdr.sig = PKZIP_CENTRAL_DIRECTORY_END_SIG;
        ehdr.number_of_disk_with_start_of_central_directory = zip_cdir_start_disk;
        ehdr.number_of_this_disk = disk_current_number();
        ehdr.total_number_of_entries_of_central_dir_on_this_disk = zip_cdir_last_count;
        ehdr.total_number_of_entries_of_central_dir = zip_cdir_total_count;
        ehdr.size_of_central_directory = zip_cdir_byte_count;
        ehdr.offset_of_central_directory_from_start_disk = zip_cdir_offset - disk_get(zip_cdir_start_disk)->byte_count;

        /* write file header */
        assert(zip_fd >= 0);

        assert(sizeof(ehdr) == 22);
        if (write(zip_fd,&ehdr,sizeof(ehdr)) != sizeof(ehdr))
            return 1;
    }

    zip_out_close();
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

