
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#ifndef O_BINARY
#define O_BINARY (0)
#endif

const char *omf_rectype_to_str_long(unsigned char rt) {
    switch (rt) {
        case 0x80:  return "Translator Header Record";
        case 0x82:  return "Library Module Header Record";
        case 0x88:  return "Comment Record";
        case 0x8A:  return "Module End Record";
        case 0x8B:  return "Module End Record (32-bit)";
        case 0x8C:  return "External Names Definition Record";
        case 0x90:  return "Public Names Definition Record";
        case 0x91:  return "Public Names Definition Record (32-bit)";
        case 0x94:  return "Line Numbers Record";
        case 0x95:  return "Line Numbers Record (32-bit)";
        case 0x96:  return "List of Names Record";
        case 0x98:  return "Segment Definition Record";
        case 0x99:  return "Segment Definition Record (32-bit)";
        case 0x9A:  return "Group Definition Record";
        case 0x9C:  return "Fixup Record";
        case 0x9D:  return "Fixup Record (32-bit)";
        case 0xA0:  return "Logical Enumerated Data Record";
        case 0xA1:  return "Logical Enumerated Data Record (32-bit)";
        case 0xA2:  return "Logical Iterated Data Record";
        case 0xA3:  return "Logical Iterated Data Record (32-bit)";
        case 0xB0:  return "Communal Names Definition Record";
        case 0xB2:  return "Backpatch Record";
        case 0xB3:  return "Backpatch Record (32-bit)";
        case 0xB4:  return "Local External Names Definition Record";
        case 0xB6:  return "Local Public Names Definition Record";
        case 0xB7:  return "Local Public Names Definition Record (32-bit)";
        case 0xB8:  return "Local Communal Names Definition Record";
        case 0xBC:  return "COMDAT External Names Definition Record";
        case 0xC2:  return "Initialized Communal Data Record";
        case 0xC3:  return "Initialized Communal Data Record (32-bit)";
        case 0xC4:  return "Symbol Line Numbers Record";
        case 0xC5:  return "Symbol Line Numbers Record (32-bit)";
        case 0xC6:  return "Alias Definition Record";
        case 0xC8:  return "Named Backpatch Record";
        case 0xC9:  return "Named Backpatch Record (32-bit)";
        case 0xCA:  return "Local Logical Names Definition Record";
        case 0xCC:  return "OMF Version Number Record";
        case 0xCE:  return "Vendor-specific OMF Extension Record";
        case 0xF0:  return "Library Header Record";
        case 0xF1:  return "Library End Record";
        default:    break;
    }

    return "?";
}

const char *omf_rectype_to_str(unsigned char rt) {
    switch (rt) {
        case 0x80:  return "THEADR";
        case 0x82:  return "LHEADR";
        case 0x88:  return "COMENT";
        case 0x8A:  return "MODEND";
        case 0x8B:  return "MODEND32";
        case 0x8C:  return "EXTDEF";
        case 0x90:  return "PUBDEF";
        case 0x91:  return "PUBDEF32";
        case 0x94:  return "LINNUM";
        case 0x95:  return "LINNUM32";
        case 0x96:  return "LNAMES";
        case 0x98:  return "SEGDEF";
        case 0x99:  return "SEGDEF32";
        case 0x9A:  return "GRPDEF";
        case 0x9C:  return "FIXUPP";
        case 0x9D:  return "FIXUPP32";
        case 0xA0:  return "LEDATA";
        case 0xA1:  return "LEDATA32";
        case 0xA2:  return "LIDATA";
        case 0xA3:  return "LIDATA32";
        case 0xB0:  return "COMDEF";
        case 0xB2:  return "BAKPAT";
        case 0xB3:  return "BAKPAT32";
        case 0xB4:  return "LEXTDEF";
        case 0xB6:  return "LPUBDEF";
        case 0xB7:  return "LPUBDEF32";
        case 0xB8:  return "LCOMDEF";
        case 0xBC:  return "CEXTDEF";
        case 0xC2:  return "COMDAT";
        case 0xC3:  return "COMDAT32";
        case 0xC4:  return "LINSYM";
        case 0xC5:  return "LINSYM32";
        case 0xC6:  return "ALIAS";
        case 0xC8:  return "NBKPAT";
        case 0xC9:  return "NBKPAT32";
        case 0xCA:  return "LLNAMES";
        case 0xCC:  return "VERNUM";
        case 0xCE:  return "VENDEXT";
        case 0xF0:  return "LIBHEAD"; // I made up this name
        case 0xF1:  return "LIBEND"; // I also made up this name
        default:    break;
    }

    return "?";
}

static char                 tempstr[257];
static unsigned char        tempstr_len;

static unsigned char        omf_record[16384];
static unsigned char        omf_rectype = 0;
static unsigned long        omf_recoffs = 0;
static unsigned int         omf_reclen = 0; // NTS: Does NOT include leading checksum byte
static unsigned int         omf_recpos = 0; // where we are parsing

static inline unsigned int omfrec_eof(void) {
    return (omf_recpos >= omf_reclen);
}

static inline unsigned int omfrec_avail(void) {
    return (omf_reclen - omf_recpos);
}

void omfrec_end(void) {
    omf_recpos = omf_reclen;
}

static inline unsigned char omfrec_gb(void) {
    return omf_record[omf_recpos++];
}

void omfrec_read(char *dst,unsigned int len) {
    if (len > 0) {
        memcpy(dst,omf_record+omf_recpos,len);
        omf_recpos += len;
    }
}

void omfrec_checkrange(void) {
    if (omf_recpos > omf_reclen) {
        fprintf(stderr,"ERROR: Overread occurred\n");
        abort();
    }
}

static char*                in_file = NULL;   

static void help(void) {
    fprintf(stderr,"omfdump [options]\n");
    fprintf(stderr,"  -i <file>    OMF file to dump\n");
}

static int read_omf_record(int fd) {
    unsigned char sum = 0;
    unsigned int i;

    omf_recoffs = lseek(fd,0,SEEK_CUR);
    if (read(fd,omf_record,3) != 3)
        return 0;

    omf_rectype = omf_record[0];
    omf_reclen = *((uint16_t*)(omf_record+1));
    if (omf_reclen == 0)
        return 0;
    if (omf_reclen > sizeof(omf_record))
        return 0;

    for (i=0;i < 3;i++)
        sum += omf_record[i];

    if ((unsigned int)read(fd,omf_record,omf_reclen) != omf_reclen)
        return 0;

    if (omf_record[omf_reclen-1]/* checksum */ != 0) {
        for (i=0;i < omf_reclen;i++)
            sum += omf_record[i];

        if (sum != 0) {
            fprintf(stderr,"* checksum failed sum=0x%02x\n",sum);
            return 0;
        }
    }
    omf_recpos=0;
    omf_reclen--; // exclude checksum byte
    return 1;
}

unsigned int omfrec_get_lenstr(char * const dst,const size_t dstmax) {
    unsigned char len;

    if (dstmax == 0)
        return 0;

    dst[0] = 0;
    if (omfrec_eof())
        return 0;

    len = omfrec_gb();
    if (len > omfrec_avail() || (len+1/*NUL*/) > dstmax) {
        omfrec_end();
        return 0;
    }

    if (len > 0) {
        omfrec_read(dst,len);
        dst[len] = 0;
    }

    return len;
}

void dump_THEADR(void) {
    /* omf_record+0: length of the string */
    /* omf_record+1: ASCII string */
    omfrec_get_lenstr(tempstr,sizeof(tempstr));
    printf("    Name of the object: \"%s\"\n",tempstr);
}

void dump_LHEADR(void) {
    /* omf_record+0: length of the string */
    /* omf_record+1: ASCII string */
    omfrec_get_lenstr(tempstr,sizeof(tempstr));
    printf("    Name of the object in library: \"%s\"\n",tempstr);
}

const char *omfrec_COMENT_cclass_to_str(unsigned char cc) {
    switch (cc) {
        case 0x00:  return "Translator";
        case 0x01:  return "Intel copyright";
        case 0x81:  return "Library specifier";
        case 0x9C:  return "MS-DOS version";
        case 0x9D:  return "Memory model";
        case 0x9E:  return "DOSSEG";
        case 0x9F:  return "Default library search name";
        case 0xA0:  return "OMF extension";
        case 0xA1:  return "New OMF extension";
        case 0xA2:  return "Link Pass Separator";
        case 0xA3:  return "Library module comment record";
        case 0xA4:  return "Executable string";
        case 0xA6:  return "Incremental compilation error";
        case 0xA7:  return "No segment padding";
        case 0xA8:  return "Weak extern record";
        case 0xA9:  return "Lazy extern record";
        case 0xDA:  return "Comment";
        case 0xDB:  return "Compiler (pragma)";
        case 0xDC:  return "Date (pragma)";
        case 0xDD:  return "Timestamp (pragma)";
        case 0xDF:  return "User (pragma)";
        case 0xE9:  return "Dependency file";
        default:    break;
    };

    return "?";
}

void dump_COMENT(void) {
    unsigned char ctype,cclass;

    /* omf_record+0: Comment Type
     * omf_record+1: Comment Class
     * omf_record+2: byte string until end of record */
    if (omfrec_avail() < 2) return;
    ctype = omfrec_gb();
    cclass = omfrec_gb();

    printf("    COMENT Type=0x%02x ( ",ctype);
    if (ctype & 0x80) printf("No-purge ");
    if (ctype & 0x40) printf("No-list ");
    printf(") class=0x%02x (%s): ",cclass,omfrec_COMENT_cclass_to_str(cclass));
    printf("\n");
}

void dump_LNAMES(void) {
    /* string records, one after the other, until the end of the record */
    printf("    LNAMES:");

    while (!omfrec_eof()) {
        omfrec_get_lenstr(tempstr,sizeof(tempstr));
        printf(" \"%s\"",tempstr);
    }

    printf("\n");
}

int main(int argc,char **argv) {
    int i,fd;
    char *a;

    for (i=1;i < argc;) {
        a = argv[i++];

        if (*a == '-') {
            do { a++; } while (*a == '-');

            if (!strcmp(a,"i")) {
                in_file = argv[i++];
                if (in_file == NULL) return 1;
            }
            else {
                help();
                return 1;
            }
        }
        else {
            fprintf(stderr,"Unexpected arg %s\n",a);
            return 1;
        }
    }

    if (in_file == NULL) {
        help();
        return 1;
    }

    fd = open(in_file,O_RDONLY|O_BINARY);
    if (fd < 0) {
        fprintf(stderr,"Failed to open input file %s\n",strerror(errno));
        return 1;
    }

    while (read_omf_record(fd)) {
        printf("OMF record type=0x%02x (%s: %s) length=%u offset=%lu\n",
            omf_rectype,
            omf_rectype_to_str(omf_rectype),
            omf_rectype_to_str_long(omf_rectype),
            omf_reclen,
            omf_recoffs);

        switch (omf_rectype) {
            case 0x80:/* THEADR */
                dump_THEADR();
                break;
            case 0x82:/* LHEADR */
                dump_LHEADR();
                break;
            case 0x88:/* COMENT */
                dump_COMENT();
                break;
            case 0x96:/* LNAMES */
                dump_LNAMES();
                break;
        }

        omfrec_checkrange();
    }

    close(fd);
    return 0;
}

