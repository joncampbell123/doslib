/* NTS:
 *
 * Spanning support does not work. I'm not sure how to make a valid spanning archive that PKUNZIP.EXE likes.
 * Also what InfoZip considers spanning is different from the floppy-by-floppy kind of spanning that PKZIP.EXE
 * does, as each floppy contains the same ZIP file name. I'm out of patience with this project. At this point,
 * I'm happy to have made a ZIP archiver that can auto-convert the names to DOS code pages. 
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <time.h>
#include <stdint.h>
#include <endian.h>
#include <dirent.h>
#include <strings.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>

#if defined(TARGET_MSDOS)
#include <dos.h>
#define WILDCARD_MATCH
#endif

#if defined(TARGET_MSDOS)
#include <ext/zlib/zlib.h>
#include <conio.h>
#else
#include <zlib.h>
#define USE_ICONV
#include "iconv.h"
#endif

#include "zipcrc.h"

#ifndef O_BINARY
#define O_BINARY (0)
#endif

extern unsigned char msdos_floppy_nonboot[512];

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
    fprintf(stderr,"  -s <size>                Generate spanning ZIP archives (see options)\n");
    fprintf(stderr,"  -ic <charset>            File names on host use this charset\n");
    fprintf(stderr,"  -oc <charset>            File names for target use this charset\n");
    fprintf(stderr,"  -t+                      Add trailing data descriptor\n");
    fprintf(stderr,"  -t-                      Don't write trailing descriptor\n");
    fprintf(stderr,"\n");
#if defined(TARGET_MSDOS)
    fprintf(stderr,"Press any key to continue..."); fflush(stderr);
    getch();
    fprintf(stderr,"\x0D                                         \x0D"); fflush(stderr);
#endif
    fprintf(stderr,"Spanning size can be specified in bytes, or with K, M, G, suffix.\n");
    fprintf(stderr,"With spanning, the zip file must have .zip suffix, which will be changed\n");
    fprintf(stderr,"per fragment to d01, d02, etc.\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"The spanning format generated is that of floppy disk images, which can be\n");
    fprintf(stderr,"written to floppy disks and then fed to PKUNZIP.EXE disk-by-disk during\n");
    fprintf(stderr,"extraction. The .d01, etc. images are NOT compatible with other ZIP archiving\n");
    fprintf(stderr,"software!\n");
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

char *strdup_plus_slash(const char *s) {
    size_t l = strlen(s);
    char *r;

    r = malloc(l/*string*/ + 1/*slash*/ + 1/*NUL*/);
    if (r != NULL) {
        if (l != 0) memcpy(r,s,l);
        r[l+0] = '/';
        r[l+1] = 0;
    }

    return r;
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

    unsigned char       data_descriptor;/* write data descriptor after file */
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
struct in_file *file_list_tail = NULL; /* for ref only to append quicker */

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
unsigned char recurse = 0;
char *codepage_in = NULL;
char *codepage_out = NULL;
int trailing_data_descriptor = -1;

unsigned int fat_start = 0;
unsigned int data_start = 0;
unsigned int total_clusters = 0;
unsigned int total_log_sectors = 0;
unsigned int total_data_sectors = 0;
unsigned int reserved_sectors = 1;
unsigned int root_dir_entries = 16;
unsigned int sectors_per_cluster = 1;
unsigned int archive_zip_offset = 0;
unsigned int root_dir_sectors = 1;
unsigned int number_of_fats = 1;
unsigned int fat_sectors = 0;
unsigned int media_desc = 0;

unsigned long zip_out_pos_abs(void) {
    if (zip_fd >= 0)
        return (unsigned long)lseek(zip_fd,0,SEEK_CUR);

    return 0UL;
}

unsigned long zip_out_pos(void) {
    if (zip_fd >= 0) {
        unsigned long p = zip_out_pos_abs();
        if (p >= data_start) p -= data_start;
        else p = 0;
        return p;
    }

    return 0UL;
}

int zip_out_open(void) {
    if (zip_fd < 0)
        zip_fd = open(zip_path,O_RDWR|O_CREAT|O_TRUNC|O_BINARY,0644);

    return (zip_fd >= 0);
}

void zip_out_header_finish(void) {
    if (data_start != 0) {
        unsigned int i,m,u,it;
        unsigned long fsz;
        char tmp[512];

        lseek(zip_fd,0,SEEK_END);
        fsz = zip_out_pos_abs() - data_start;
        fprintf(stderr,"Finishing up file %lu bytes\n",fsz);

        assert(zip_fd >= 0);

        /* go back and write in the size of the "file" in the root dir */
        lseek(zip_fd,archive_zip_offset,SEEK_SET);
        read(zip_fd,tmp,32);

        /* starting cluster already filled in. update file size. */
        *((uint32_t*)(tmp+0x1C)) = fsz;

        lseek(zip_fd,archive_zip_offset,SEEK_SET);
        write(zip_fd,tmp,32);

        /* now make a FAT chain */
        {
            unsigned char *FAT = malloc(512 * fat_sectors);

            if (FAT == NULL) return;
            lseek(zip_fd,fat_start,SEEK_SET);
            read(zip_fd,FAT,512 * fat_sectors);

            /* how many clusters? */
            u = 512 * sectors_per_cluster;
            m = ((fsz + u - 1U) / u) + 2;
            for (i=2;i < m;i++) {
                unsigned int o = ((i >> 1U) * 3U) + (i & 1U);
                unsigned int os = (i & 1U) * 4U;
                unsigned int om = 0xFFFU << os;
                uint16_t ent;

                if ((i+1U) == m)
                    ent = 0xFF8;    /* last cluster */
                else
                    ent = i + 1;    /* next cluster */

                *((uint16_t*)(FAT+o)) &= ~om;
                *((uint16_t*)(FAT+o)) |= ent << os;
            }

            lseek(zip_fd,fat_start,SEEK_SET);
            for (it=0;it < number_of_fats;it++) {
                write(zip_fd,FAT,512 * fat_sectors);
            }
            free(FAT);
        }

        /* fill to end */
        memset(tmp,0,sizeof(tmp));
        lseek(zip_fd,0,SEEK_END);
        while (zip_out_pos_abs() < spanning_size) {
            unsigned long tor = spanning_size - zip_out_pos_abs();
            if (tor > 512) tor = 512;
            write(zip_fd,tmp,tor);
        }
        if (zip_out_pos_abs() > spanning_size) {
            fprintf(stderr,"Spanning ended up going over by %lu bytes!\n",(unsigned long)zip_out_pos_abs() - spanning_size);
            abort();
        }

        fat_start = 0;
        data_start = 0;
    }
}

int zip_out_header(void) {
    /* explanation: our spanning mode uses the PKZIP floppy method.
     * that includes a segmented ZIP and volume labels to match.
     * so the "header" is really just the first dozen sectors of an MS-DOS formatted floppy
     * up to the point that the ZIP archive begins. */
    if (spanning_size > 0 && zip_out_pos_abs() == 0) {
        char tmp[512];
        unsigned int C,H,S;
        unsigned int it;

        number_of_fats = 2;
        reserved_sectors = 1;
        root_dir_sectors = 14;
        sectors_per_cluster = 1;

        if (spanning_size >= (1440UL*1024UL)) {
            /* 1.44MB */
            C = 80;
            H = 2;
            S = 18;
            media_desc = 0xF0;
        }
        else if (spanning_size >= (1200UL*1024UL)) {
            /* 1.2MB */
            C = 80;
            H = 2;
            S = 15;
            media_desc = 0xF9;
        }
        else if (spanning_size >= (720UL*1024UL)) {
            /* 720KB */
            C = 80;
            H = 2;
            S = 9;
            media_desc = 0xF9;
        }
        else {
            abort();
        }

        root_dir_entries = (root_dir_sectors * 512UL) / 32UL;                   /* one sector / (bytes per root dir entry) */
        total_log_sectors = C * H * S;
        total_data_sectors = total_log_sectors - reserved_sectors - root_dir_sectors;
        total_clusters = total_data_sectors / sectors_per_cluster;
        for (it=0;it < 3;it++) {
            unsigned long fatsz = (((unsigned long)total_clusters + 1UL) / 2UL) * 3UL; /* FAT12 */
            fat_sectors = (fatsz + 511UL) / 512UL;
            total_data_sectors = total_log_sectors - reserved_sectors - root_dir_sectors;
            total_data_sectors -= fat_sectors * number_of_fats;
            total_clusters = total_data_sectors / sectors_per_cluster;
        }

        data_start = reserved_sectors + (fat_sectors * number_of_fats) + root_dir_sectors;
        data_start *= 512UL;

        /* copy boot sector, we're going to mod it */
        memcpy(tmp,msdos_floppy_nonboot,512);

        /* mod the BPB */
        *((uint16_t*)(tmp+0x00B)) = 512;                            /* bytes per logical sector */
        *((uint8_t*) (tmp+0x00D)) = sectors_per_cluster;            /* sectors per cluster */
        *((uint16_t*)(tmp+0x00E)) = reserved_sectors;               /* reserved sectors */
        *((uint8_t*) (tmp+0x010)) = number_of_fats;                 /* number of file alloc. tables */
        *((uint16_t*)(tmp+0x011)) = root_dir_entries;               /* number of root dir entries */
        *((uint16_t*)(tmp+0x013)) = total_log_sectors;              /* total logical sectors (of the entire disk) */
        *((uint8_t*) (tmp+0x015)) = media_desc;                     /* media descriptor */
        *((uint16_t*)(tmp+0x016)) = fat_sectors;                    /* sectors per FAT */
        *((uint16_t*)(tmp+0x018)) = S;
        *((uint8_t*) (tmp+0x01A)) = H;
        *((uint32_t*)(tmp+0x01C)) = 0;
        *((uint32_t*)(tmp+0x020)) = 0;

        assert(zip_fd >= 0);
        write(zip_fd,tmp,512);

        fat_start = zip_out_pos_abs();
        {
            unsigned char *FAT = malloc(512 * fat_sectors);

            if (FAT == NULL) return 1;
            memset(FAT,0,512 * fat_sectors);

            /* media byte + filler */
            FAT[0] = media_desc;
            FAT[1] = 0xFF;
            FAT[2] = 0xFF;

            /* zip_out_close() will write a FAT chain for how much was actually written */

            for (it=0;it < number_of_fats;it++) {
                write(zip_fd,FAT,512 * fat_sectors);
            }

            free(FAT);
        }

        /* root directory (last bit). ZIP file and volume label */
        memset(tmp,0,32*2);
        {
            unsigned char *ent = (unsigned char*)tmp + 0;

            /*          <---8+3---> */
            memcpy(ent,"ARCHIVE ZIP",11);
            ent[11] = 0x00; /* normal file */
            *((uint16_t*)(tmp+26)) = 2; /* starting cluster */
            /* zip_out_close() will update size later */
        }

        {
            unsigned char *ent = (unsigned char*)tmp + 32;

            /* volume label 11 char (based on PKZIP) */
            sprintf((char*)ent,"PKBACK# %03u",disk_current_number()+1);
            ent[11] = 0x08; /* volume label */
        }

        archive_zip_offset = zip_out_pos_abs();
        write(zip_fd,tmp,32+32);

        /* fill to data start */
        memset(tmp,0,sizeof(tmp));
        while (zip_out_pos_abs() < data_start) {
            unsigned long tor = data_start - zip_out_pos_abs();
            if (tor > 512) tor = 512;
            write(zip_fd,tmp,tor);
        }

        if (zip_out_pos_abs() > data_start) {
            fprintf(stderr,"WARNING: header too large\n");
            abort();
        }
    }

    return 1;
}

void zip_out_close(void) {
    if (zip_fd >= 0) {
        if (spanning_size > 0)
            zip_out_header_finish();

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
    if (*s != 0) {
        char c = tolower(*s++);

        if (*s != 0)
            return 0;

        if (c == 'k')
            *out <<= 10ULL;
        else if (c == 'm')
            *out <<= 20ULL;
        else if (c == 'g')
            *out <<= 30ULL;
        else
            return 0;
    }

    return 1;
}

char *zip_out_get_span_name(int disk_number) {
    char *a,*x;

    assert(zip_path != NULL);
    if (disk_number < 0 || disk_number >= 99) return NULL;

    a = strdup(zip_path);
    if (a == NULL) return 0;

    /* replace .zip with .d01, .d02, etc. */
    /* we'll change to doing .z01, .z02, etc. when we support split archives properly,
     * until then we only support generating disk images. */
    x = strrchr(a,'.');
    if (x == NULL) {
        free(a);
        return NULL;
    }

    if (!strcasecmp(x,".zip")) {
/*                     0123 */
        x[1] = (char)'d';
        x[2] = (char)((disk_number + 1) / 10) + '0';
        x[3] = (char)((disk_number + 1) % 10) + '0';
    }
    else {
        free(a);
        return NULL;
    }

    return a;
}

int zip_out_rename_to_span(int disk_number) {
    char *a = zip_out_get_span_name(disk_number);

    fprintf(stderr,"Renaming: '%s' to '%s'\n",zip_path,a);
    if (rename(zip_path,a)) {
        fprintf(stderr,"Failed to rename '%s' to '%s', %s\n",zip_path,a,strerror(errno));
        free(a);
        return 0;
    }

    free(a);
    return 1;
}

ssize_t zip_write_and_span(int fd,const void *buf,size_t count) {
    unsigned long t,pos = zip_out_pos_abs();
    size_t towrite = 0;
    ssize_t wd = 0;
    ssize_t w;

    while (count > 0) {
        if (spanning_size > 0) {
            t = spanning_size;
            if (t >= pos)
                t -= pos;
            else
                t = 0;

            if (t > count)
                t = count;

            towrite = (size_t)t;
        }
        else {
            towrite = count;
        }

        if (towrite != 0) {
            assert(fd >= 0);
            w = write(fd,buf,towrite);
            if (w <= 0) break;
        }
        else {
            w = 0;
        }

        wd += w;
        count -= (size_t)w;
        buf = (const void*)((const char*)buf + w);

        if (spanning_size > 0) {
            pos = zip_out_pos_abs();
            if (pos >= spanning_size) {
                unsigned long disk_pos = zip_out_pos_abs();
                unsigned long disk_abs = disk_current()->byte_count;

                zip_out_close();
                if (!zip_out_rename_to_span(disk_current_number()))
                    return -1;

                {
                    struct disk_info *d = disk_new();
                    if (d == NULL) return -1;
                    d->byte_count = disk_pos + disk_abs;
                }

                if (!zip_out_open())
                    return -1;

                if (spanning_size > 0 && zip_out_pos_abs() == 0) {
                    if (!zip_out_header())
                        return 1;
                }
            }
        }
    }

    return wd;
}

int zip_store(struct pkzip_local_file_header_main *lfh,struct in_file *list) {
#if TARGET_MSDOS == 16
    size_t buffer_sz = 512; /* should be good */
#else
    size_t buffer_sz = 16384; /* should be good */
#endif
    unsigned long total = 0;
    zipcrc_t crc32;
    char *buffer;
    int src_fd;
    int rd;

    crc32 = zipcrc_init();

    assert(list->in_path != NULL);

    lfh->uncompressed_size = list->file_size;
    if (list->file_size == 0)
        return 0; /* nothing to do */

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
        if (zip_write_and_span(zip_fd,buffer,rd) != rd)
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
#if TARGET_MSDOS == 16
    size_t inbuffer_sz = 512,outbuffer_sz = 512;
#else
    size_t inbuffer_sz = 32768,outbuffer_sz = 32768;
#endif
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
                if ((size_t)zip_write_and_span(zip_fd,outbuffer,wd) != wd) {
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
            if ((size_t)zip_write_and_span(zip_fd,outbuffer,wd) != wd) {
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

uint16_t stat2msdostime(struct stat *st) {
    struct tm *tm = localtime(&st->st_mtime);
    assert(tm != NULL);

    return
        (tm->tm_hour << 11U) +
        (tm->tm_min  <<  5U) +
        (tm->tm_sec  >>  1U);
}

uint16_t stat2msdosdate(struct stat *st) {
    struct tm *tm = localtime(&st->st_mtime);
    assert(tm != NULL);

    tm->tm_year += 1900;
    if (tm->tm_year < 1980)
        tm->tm_year = 1980;
    else if (tm->tm_year > (1980+127))
        tm->tm_year = 1980+127;

    tm->tm_mon++;

    return
        (((tm->tm_year - 1980U) & 0x7FU) << 9U) +
          (tm->tm_mon                    << 5U) +
           tm->tm_mday;
}

#ifdef USE_ICONV
static int add_file(char *a,struct stat st,iconv_t ic) {
#else
static int add_file(char *a,struct stat st) {
#endif
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
    /* PKZIP undocumented behavior (or not mentioned in APPNOTE) is that directories are stored with '/' at the end of the name */
    if (f->attr & ATTR_DOS_DIR)
        t = strdup_plus_slash(a);
    else
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

#ifdef USE_ICONV
    if (ic != (iconv_t)-1) {
        size_t inleft = strlen(ft);
        size_t outleft = sizeof(ic_tmp)-1;
        char *out = ic_tmp,*in = ft;
        int ret;

        ret = iconv(ic,&in,&inleft,&out,&outleft);
        if (ret == -1 || inleft != (size_t)0 || outleft == (size_t)0) {
            fprintf(stderr,"file name conversion error from '%s'. ret=%d inleft=%lu outleft=%lu\n",ft,ret,(unsigned long)inleft,(unsigned long)outleft);
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
#else
    if (0) {
    }
#endif
    else {
        if (in_file_set_zip_name(f,ft) == NULL) {
            fprintf(stderr,"out of memory\n");
            return 1;
        }
    }

    f->msdos_time = stat2msdostime(&st);
    f->msdos_date = stat2msdosdate(&st);

    free(t);
    file_list_append(f);
    return 0;
}

static int skip_file(char *a,struct stat *st) {
    if (lstat(a,st)) {
        fprintf(stderr,"Cannot stat %s, %s\n",a,strerror(errno));
        return 1;
    }
    if (!(S_ISREG(st->st_mode) || S_ISDIR(st->st_mode))) {
        fprintf(stderr,"Skipping non-file non-directory %s\n",a);
        return 1;
    }
    if (st->st_size >= (off_t)((1UL << 31UL) - (1UL << 28UL))) { /* 2GB - 256MB */
        fprintf(stderr,"Skipping file %s, too large (%llu bytes)\n",a,(unsigned long long)st->st_size);
        return 1;
    }

    return 0;
}

#ifdef WILDCARD_MATCH
static int is_wildcard(const char *s) {
    while (*s) {
        if (*s == '*' || *s == '?')
            return 1;
        else
            s++;
    }

    return 0;
}
#endif

static int parse(int argc,char **argv) {
    unsigned int zip_cdir_total_count = 0;
    unsigned int zip_cdir_last_count = 0;
    unsigned long zip_cdir_byte_count = 0;
    unsigned long zip_cdir_offset = 0;
#ifdef USE_ICONV
    iconv_t ic = (iconv_t)-1;
#endif
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
#ifdef USE_ICONV
                if (ic != (iconv_t)-1) {
                    iconv_close(ic);
                    ic = (iconv_t)-1;
                }
#endif
            }
            else if (!strcmp(a,"oc")) {
                a = argv[i++];
                if (a == NULL) return 1;
                set_string(&codepage_out,a);
#ifdef USE_ICONV
                if (ic != (iconv_t)-1) {
                    iconv_close(ic);
                    ic = (iconv_t)-1;
                }
#endif
            }
            else if (!strcmp(a,"t+")) {
                trailing_data_descriptor = 1;
            }
            else if (!strcmp(a,"t-")) {
                trailing_data_descriptor = 0;
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
#ifdef USE_ICONV
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
#endif

#ifdef WILDCARD_MATCH
            if (is_wildcard(a)) {
#if defined(TARGET_MSDOS) /* MS-DOS 8.3 */
                struct find_t fi;

                char *tmp = malloc(PATH_MAX),*tf = tmp+PATH_MAX,*tmpappend = tmp;
                if (!tmp) return 1;
                *tmp = 0;

                if (_dos_findfirst(a,_A_NORMAL|_A_RDONLY|_A_SUBDIR|_A_ARCH,&fi) == 0) {
                    {
                        char *s = a,*e = strrchr(a,'\\');
                        if (e) {
                            while (s <= e && tmpappend < tf) *tmpappend++ = *s++;
                            if (tmpappend >= tf) continue;
                            *tmpappend = 0;
                        }
                    }

                    do {
                        if (fi.name[0] == '.')
                            continue;

                        {
                            char *s = fi.name,*d = tmpappend;
                            while (*s && d < tf) *d++ = *s++;
                            if (d >= tf) continue;
                            *d = 0;
                        }

                        if (skip_file(tmp,&st))
                            continue;

#ifdef USE_ICONV
# error unexpected
#else
                        if (add_file(tmp,st))
                                return 1;
#endif
                    } while (_dos_findnext(&fi) == 0);
                }

                free(tmp);
#endif
                continue;
            }
#endif /*WILDCARD_MATCH*/

            if (skip_file(a,&st))
                continue;

#ifdef USE_ICONV
            if (add_file(a,st,ic))
                return 1;
#else
            if (add_file(a,st))
                return 1;
#endif
        }
    }

    if (spanning_size > 0)
        spanning_size -= spanning_size & 0x1FFU;

    if (spanning_size > 0) {
        /* when writing as disk images, the spanning size must match that of a floppy disk */
        if (spanning_size > (512UL*80UL*2UL*18UL)) {
            fprintf(stderr,"For a floppy, spanning size is too big. Try 1440k.\n");
            return 1;
        }
        else if (spanning_size > (512UL*80UL*2UL*15UL)) { /* 1.44MB, larger than 1.2MB */
            spanning_size = (512UL*80UL*2UL*18UL);
        }
        else if (spanning_size > (512UL*80UL*1UL*18UL)) { /* 1.2MB, larger than 720KB */
            spanning_size = (512UL*80UL*2UL*15UL);
        }
        else if (spanning_size > (512UL*80UL*1UL*16UL)) { /* 720KB, larger than 640KB */
            spanning_size = (512UL*80UL*1UL*18UL);
        }
        else {
            fprintf(stderr,"For a floppy, spanning size is too small. Try 1440k.\n");
        }
    }

    if (spanning_size > 0)
        fprintf(stderr,"Spanning size: %luKB\n",spanning_size / 1024UL);

#ifdef USE_ICONV
    if (ic != (iconv_t)-1) {
        iconv_close(ic);
        ic = (iconv_t)-1;
    }
#endif

    if (zip_path == NULL) {
        fprintf(stderr,"Must specify ZIP file: --zip\n");
        return 1;
    }
    if (file_list_head == NULL) {
        fprintf(stderr,"Nothing to add\n");
        return 1;
    }

    /* default, off, UNLESS spanning ZIP archives.
     * The reason we default ON for spanning ZIP archives is that
     * PKUNZIP.EXE 2.5 gets confused by ZIP archives spanning floppies IF
     * the trailing data descriptor is not there. I guess the assumption is
     * that, if the ZIP was written across floppies in a spanning mode, then
     * it has to use the trailing data descriptor since it can't go back a floppy
     * and rewrite the local header.
     *
     * We can avoid a lot of "CRC error" and "inconsistent header" errors from PKUNZIP.EXE
     * by making the trailing data descriptor default. */
    if (trailing_data_descriptor < 0) {
        if (spanning_size > 0)
            trailing_data_descriptor = 1;
        else
            trailing_data_descriptor = 0;
    }

    if (recurse) {
        struct in_file *list;
        struct dirent *d;
        size_t sl;
        DIR *dir;

#ifdef USE_ICONV
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
#endif

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
                    if (st.st_size >= (off_t)((1UL << 31UL) - (1UL << 28UL))) { /* 2GB - 256MB */
                        fprintf(stderr,"Skipping file %s, too large (%llu bytes)\n",ic_tmp,(unsigned long long)st.st_size);
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
                        /* PKZIP undocumented behavior (or not mentioned in APPNOTE) is that directories are stored with '/' at the end of the name */
                        if (f->attr & ATTR_DOS_DIR)
                            t = strdup_plus_slash(ic_tmp);
                        else
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

#ifdef USE_ICONV
                        if (ic != (iconv_t)-1) {
                            size_t inleft = strlen(ft);
                            size_t outleft = sizeof(ic_tmp)-1;
                            char *out = ic_tmp,*in = ft;
                            int ret;

                            ret = iconv(ic,&in,&inleft,&out,&outleft);
                            if (ret == -1 || inleft != (size_t)0 || outleft == (size_t)0) {
                                fprintf(stderr,"file name conversion error from '%s'. ret=%d inleft=%lu outleft=%lu\n",ft,ret,(unsigned long)inleft,(unsigned long)outleft);
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
#else
                        if (0) {
                        }
#endif
                        else {
                            if (in_file_set_zip_name(f,ft) == NULL) {
                                fprintf(stderr,"out of memory\n");
                                return 1;
                            }
                        }

                        f->msdos_time = stat2msdostime(&st);
                        f->msdos_date = stat2msdosdate(&st);

                        free(t);
                        file_list_append(f);
                    }
                }

                closedir(dir);
            }
        }
    }

#ifdef USE_ICONV
    if (ic != (iconv_t)-1) {
        iconv_close(ic);
        ic = (iconv_t)-1;
    }
#endif

    {
        struct disk_info *d = disk_new();
        if (d == NULL) return 1;
        d->byte_count = 0;
    }

    if (spanning_size > 0) {
        /* get writing! */
        if (!zip_out_open())
            return 1;

        if (spanning_size > 0 && zip_out_pos_abs() == 0) {
            if (!zip_out_header())
                return 1;
        }

        {
            /* the first segment of spanned ZIP archives have a special signature at the start */
            uint32_t x = 0x08074B50UL; /* PK\x07\x08 */

            write(zip_fd,&x,4);
        }
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

            if (deflate_mode > 0 && !(list->attr & ATTR_DOS_DIR))
                lhdr.compression_method = 8; /* deflate */
            else
                lhdr.compression_method = 0; /* stored (no compression) */

            lhdr.last_mod_file_time = list->msdos_time;
            lhdr.last_mod_file_date = list->msdos_date;
            /* some fields we'll go back and write later */
            lhdr.filename_length = strlen(list->zip_name);

            /* data descriptors are valid ONLY for deflate */
            if (lhdr.compression_method == 8)
                list->data_descriptor = (trailing_data_descriptor > 0);
            else
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

            if (spanning_size > 0 && zip_out_pos_abs() == 0) {
                if (!zip_out_header())
                    return 1;
            }

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

                    x = lhdr.crc32;
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
                    int write_fd=zip_fd,alt_fd=-1;

                    /* access disk if necessary */
                    if (list->disk_number != disk_current_number()) {
                        char *nn = zip_out_get_span_name(list->disk_number);
                        if (nn == NULL) return 1;
                        write_fd = alt_fd = open(nn,O_RDWR|O_BINARY);
                        if (alt_fd < 0) return 1;
                        free(nn);
                    }

                    /* go back and write the lhdr again */
                    /* WARNING: If spanning floppies we assume the same MS-DOS fat format and data_start */
                    if ((unsigned long)lseek(write_fd,list->disk_offset + data_start,SEEK_SET) != (list->disk_offset + data_start))
                        return 1;
                    if (write(write_fd,&lhdr,sizeof(lhdr)) != sizeof(lhdr))
                        return 1;

                    if (alt_fd >= 0) {
                        close(alt_fd);
                        alt_fd = -1;
                    }

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

            if (spanning_size > 0) {
                if (zip_out_pos_abs() >= spanning_size) {
                    unsigned long disk_pos = zip_out_pos();
                    unsigned long disk_abs = disk_current()->byte_count;

                    zip_out_close();
                    if (!zip_out_rename_to_span(disk_current_number()))
                        return -1;

                    {
                        struct disk_info *d = disk_new();
                        if (d == NULL) return 1;
                        d->byte_count = disk_pos + disk_abs;
                    }

                    if (!zip_out_open())
                        return 1;

                    if (spanning_size > 0 && zip_out_pos_abs() == 0) {
                        if (!zip_out_header())
                            return 1;
                    }
                }
            }

            memset(&chdr,0,sizeof(chdr));
            chdr.sig = PKZIP_CENTRAL_DIRECTORY_HEADER_SIG;
            chdr.version_made_by = (0 << 8) + 25;       /* MS-DOS + PKZIP 2.5 */
            chdr.version_needed_to_extract = 20;        /* PKZIP 2.0 or higher */
            chdr.general_purpose_bit_flag = (0 << 1);   /* just lie and say that "normal" deflate was used */

            if (list->data_descriptor)
                chdr.general_purpose_bit_flag |= (1 << 3);

            if (deflate_mode > 0 && !(list->attr & ATTR_DOS_DIR))
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

            if (spanning_size > 0 && zip_out_pos_abs() == 0) {
                if (!zip_out_header())
                    return 1;
            }

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

    if (disk_current_number() > 0) {
        zip_out_close();
        if (!zip_out_rename_to_span(disk_current_number()))
            return -1;
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

