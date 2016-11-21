/* FIXME: This code needs to be fixed up to process the OMF in two passes.
 *        First pass is to build a list of symbols, SEGDEFs, GRPDEFs in memory.
 *        Second pass is to actually copy and patch the OMF data.
 *
 *        This code could copy and patch the data in a much safer manner if it
 *        first knew where all the PUBDEFs, EXTDEFs, symbols, etc. lie before
 *        attempting to read and follow FIXUPP entries to patch and remove
 *        segment references. */

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

static unsigned char        omf_record[16384];
static unsigned char        omf_rectype = 0;
static unsigned long        omf_recoffs = 0;
static unsigned int         omf_reclen = 0; // NTS: Does NOT include leading checksum byte
static unsigned int         omf_recpos = 0; // where we are parsing
static unsigned int         omf_lib_blocksize = 0;

static unsigned char        last_LEDATA_type = 0;
static unsigned long        last_LEDATA_ofd_omf_offset = 0;
static unsigned long        last_LEDATA_ofd_data_offset = 0;
static unsigned char        last_LEDATA_segment_index = 0;
static unsigned long        last_LEDATA_data_length = 0;
static unsigned long        last_LEDATA_data_offset = 0;

/* LNAMES collection */
#define MAX_LNAMES          255
static char*                omf_LNAMES[MAX_LNAMES];
static unsigned int         omf_LNAMES_count = 0;

static const char *omf_get_LNAME(const unsigned int i) {
    if (i == 0 || i > omf_LNAMES_count) // LNAMEs are 1-based
        return NULL;

    return omf_LNAMES[i-1];
}

static const char *omf_get_LNAME_safe(const unsigned int i) {
    const char *r = omf_get_LNAME(i);

    return (r != NULL) ? r : "[ERANGE]";
}

static void omf_LNAMES_clear(void) {
    while (omf_LNAMES_count > 0) {
        omf_LNAMES_count--;

        if (omf_LNAMES[omf_LNAMES_count] != NULL) {
            free(omf_LNAMES[omf_LNAMES_count]);
            omf_LNAMES[omf_LNAMES_count] = NULL;
        }
    }
}

static void omf_LNAMES_add(const char *name) {
    size_t len = strlen(name);
    char *p;

    if (name == NULL)
        return;
    if (omf_LNAMES_count >= MAX_LNAMES)
        return;

    p = malloc(len+1);
    if (p == NULL)
        return;

    memcpy(p,name,len+1);/* name + NUL */
    omf_LNAMES[omf_LNAMES_count++] = p;
}

struct omf_SEGDEF_t {
    unsigned char           nameidx;
};

#define MAX_SEGDEFS         255
static struct omf_SEGDEF_t  omf_SEGDEFS[MAX_SEGDEFS];
static unsigned int         omf_SEGDEFS_count = 0;

static struct omf_SEGDEF_t *omf_SEGDEFS_add(const unsigned char nameidx) {
    struct omf_SEGDEF_t *rec;

    if (nameidx == 0)
        return NULL;
    if (omf_SEGDEFS_count >= MAX_SEGDEFS)
        return NULL;

    rec = &omf_SEGDEFS[omf_SEGDEFS_count++];
    memset(rec,0,sizeof(*rec));
    rec->nameidx = nameidx;
    return rec;
}

static struct omf_SEGDEF_t *omf_get_SEGDEF(const unsigned int i) {
    if (i == 0 || i > omf_SEGDEFS_count)
        return NULL;

    return &omf_SEGDEFS[i-1];
}

static const char *omf_get_SEGDEF_name(const unsigned int i) {
    struct omf_SEGDEF_t *rec = omf_get_SEGDEF(i);
    unsigned char idx = (rec != NULL) ? rec->nameidx : 0;
    return omf_get_LNAME(idx);
}

static const char *omf_get_SEGDEF_name_safe(const unsigned int i) {
    const char *name = omf_get_SEGDEF_name(i);
    return (name != NULL) ? name : "[ERANGE]";
}

static void omf_SEGDEF_clear(void) {
    omf_SEGDEFS_count = 0;
}

struct omf_GRPDEF_t {
    unsigned char           nameidx;
};

#define MAX_GRPDEFS         255
static struct omf_GRPDEF_t  omf_GRPDEFS[MAX_GRPDEFS];
static unsigned int         omf_GRPDEFS_count = 0;

static struct omf_GRPDEF_t *omf_GRPDEFS_add(const unsigned char nameidx) {
    struct omf_GRPDEF_t *rec;

    if (nameidx == 0)
        return NULL;
    if (omf_GRPDEFS_count >= MAX_GRPDEFS)
        return NULL;

    rec = &omf_GRPDEFS[omf_GRPDEFS_count++];
    memset(rec,0,sizeof(*rec));
    rec->nameidx = nameidx;
    return rec;
}

static struct omf_GRPDEF_t *omf_get_GRPDEF(const unsigned int i) {
    if (i == 0 || i > omf_GRPDEFS_count)
        return NULL;

    return &omf_GRPDEFS[i-1];
}

static const char *omf_get_GRPDEF_name(const unsigned int i) {
    struct omf_GRPDEF_t *rec = omf_get_GRPDEF(i);
    unsigned char idx = (rec != NULL) ? rec->nameidx : 0;
    return omf_get_LNAME(idx);
}

static const char *omf_get_GRPDEF_name_safe(const unsigned int i) {
    const char *name = omf_get_GRPDEF_name(i);
    return (name != NULL) ? name : "[none]";
}

static void omf_GRPDEF_clear(void) {
    omf_GRPDEFS_count = 0;
}

/* EXTDEF collection */
#define MAX_EXTDEF          255
static char*                omf_EXTDEF[MAX_EXTDEF];
static unsigned int         omf_EXTDEF_count = 0;

static const char *omf_get_EXTDEF(const unsigned int i) {
    if (i == 0 || i > omf_EXTDEF_count) // EXTDEFs are 1-based
        return NULL;

    return omf_EXTDEF[i-1];
}

static const char *omf_get_EXTDEF_safe(const unsigned int i) {
    const char *r = omf_get_EXTDEF(i);

    return (r != NULL) ? r : "[ERANGE]";
}

static void omf_EXTDEF_clear(void) {
    while (omf_EXTDEF_count > 0) {
        omf_EXTDEF_count--;

        if (omf_EXTDEF[omf_EXTDEF_count] != NULL) {
            free(omf_EXTDEF[omf_EXTDEF_count]);
            omf_EXTDEF[omf_EXTDEF_count] = NULL;
        }
    }
}

static void omf_EXTDEF_add(const char *name) {
    size_t len = strlen(name);
    char *p;

    if (name == NULL)
        return;
    if (omf_EXTDEF_count >= MAX_EXTDEF)
        return;

    p = malloc(len+1);
    if (p == NULL)
        return;

    memcpy(p,name,len+1);/* name + NUL */
    omf_EXTDEF[omf_EXTDEF_count++] = p;
}

static void omf_reset(void) {
    omf_LNAMES_clear();
    omf_SEGDEF_clear();
    omf_GRPDEF_clear();
    omf_EXTDEF_clear();
}

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

static inline unsigned int omfrec_gw(void) {
    unsigned int v = *((unsigned short*)(omf_record+omf_recpos));
    omf_recpos += 2;
    return v;
}

static void omfrec_backspace_to(unsigned int p) {
    unsigned int rem;

    if (p >= omf_recpos)
        return;
    if (omf_recpos > omf_reclen)
        return;

    // p < omf_recpos
    rem = omfrec_avail(); // could be zero
    if (rem != 0) memmove(omf_record+p,omf_record+omf_recpos,rem);
    omf_reclen = p + rem;
    omf_recpos = p;
}

static inline unsigned int omfrec_gindex(void) {
    // 1 or 2 bytes.
    // 1 byte if less than 0x80
    // 2 bytes if more than 0x7F
    unsigned int t;

    if (omfrec_eof()) return 0;
    t = omfrec_gb();
    if (t & 0x80) {
        t = (t & 0x7F) << 8U;
        if (omfrec_eof()) return 0;
        t += omfrec_gb();
    }

    return t;
}

static inline unsigned long omfrec_gd(void) {
    unsigned long v = *((uint32_t*)(omf_record+omf_recpos));
    omf_recpos += 4;
    return v;
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
static char*                out_file = NULL;

static void help(void) {
    fprintf(stderr,"omfsegfl [options]\n");
    fprintf(stderr,"Copy and patch 16-bit OMF files to replace segment relocations with\n");
    fprintf(stderr,"references to CS or DS (for use in non-EXE formats).\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"  -i <file>    OMF file to read\n");
    fprintf(stderr,"  -o <file>    OMF file to write (with hacks)\n");
}

static int read_omf_record(int fd) {
    unsigned char sum = 0;
    unsigned int i;

    omf_recoffs = lseek(fd,0,SEEK_CUR);
    if (read(fd,omf_record,3) != 3)
        return 0;

    omf_rectype = omf_record[0];
    omf_reclen = *((uint16_t*)(omf_record+1));
    if (omf_rectype == 0 || omf_reclen == 0)
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

unsigned int omfrec_get_remstr(char * const dst,const size_t dstmax) {
    unsigned int len;

    if (dstmax == 0)
        return 0;

    dst[0] = 0;
    if (omfrec_eof())
        return 0;

    len = omfrec_avail();
    if (len == 0 || len > 255)
        return 0;

    omfrec_read(dst,len);
    dst[len] = 0;
    return len;
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

void dump_LNAMES(void) {
    /* string records, one after the other, until the end of the record */
//    printf("    LNAMES:");

    while (!omfrec_eof()) {
        omfrec_get_lenstr(tempstr,sizeof(tempstr));
//        printf(" \"%s\"",tempstr);
        omf_LNAMES_add(tempstr);
    }

//    printf("\n");
}

void dump_EXTDEF(void) {
    unsigned int typidx;

//    printf("    EXTDEF:\n");
    while (!omfrec_eof()) {
        omfrec_get_lenstr(tempstr,sizeof(tempstr));

        typidx = omfrec_gindex(); // 1 or 2 bytes

//        printf("        '%s' typidx=%u\n",tempstr,typidx);

        omf_EXTDEF_add(tempstr);
    }
}

void dump_LEXTDEF(const unsigned char b32) {
    unsigned int typidx;

//    printf("    LEXTDEF:\n");
    while (!omfrec_eof()) {
        omfrec_get_lenstr(tempstr,sizeof(tempstr));

        typidx = omfrec_gindex(); // 1 or 2 bytes

//        printf("        '%s' typidx=%u\n",tempstr,typidx);

        omf_EXTDEF_add(tempstr);
    }
}

static unsigned char ledata[1024];
static unsigned char ledata_loaded = 0;

int load_LEDATA(int ofd) {
    if (ledata_loaded)
        return 0;
    if (last_LEDATA_data_length >= 1024)
        return -1;
    if (lseek(ofd,last_LEDATA_ofd_data_offset,SEEK_SET) != last_LEDATA_ofd_data_offset)
        return -1;
    if (last_LEDATA_data_length == 0)
        return 0;
    if (read(ofd,ledata,last_LEDATA_data_length) != last_LEDATA_data_length)
        return -1;

    ledata_loaded = 1;
    return 0;
}

int range_check_LEDATA(unsigned int reco,unsigned int sz) {
    if ((reco+sz) > last_LEDATA_data_length)
        return -1;

    return 0;
}

const char *cpu_regs[8] = {
    "AX",   "CX",   "DX",   "BX",
    "SP",   "BP",   "SI",   "DI"
};

int segbase_patch_LEDATA(unsigned int reco) {
    uint16_t *psegval;
    unsigned char *op;

    if (reco == 0 || (reco+2) > last_LEDATA_data_length)
        return -1;

    /* one pointer to point AT the 16-bit WORD the linker would patch with segment base */
    psegval = (uint16_t*)(ledata + reco);

    /* another to examine the opcode surrounding the fixup (this is why we check reco == 0) */
    op = ledata + reco - 1;

    /* DEBUG */
//    fprintf(stderr,"        * opcode byte just before FIXUP: 0x%02X\n",*op);
//    fprintf(stderr,"        * 16-bit word to patch: 0x%04X\n",*psegval);

    /* is it "MOV <reg>,<imm>"? (3 bytes) */
    if ((*op & 0xF8) == 0xB8) {
        unsigned char rg = *op & 7;

        fprintf(stderr,"* patching MOV %s,0x%04x -> MOV %s,CS\n",cpu_regs[rg],*psegval,cpu_regs[rg]);

        /* change it to "MOV <reg>,CS" + NOP (3 bytes) */
        op[0] = 0x8C;
        op[1] = 0xC8 + rg;
        op[2] = 0x90;//NOP

        return 0;
    }
    /* is it "MOV WORD PTR <mrm>,<imm>"? (4 bytes) */
    else if ((op-1) >= ledata && op[-1] == 0xC7 && *op <= 0x07) {
        unsigned char rg = *op & 7;

        fprintf(stderr,"* patching MOV WORD PTR [...],0x%04x -> ,CS\n",*psegval);

        /* change it to "MOV WORD PTR <mrm>,CS" + NOP + NOP (4 bytes) */
        op[-1] = 0x8C;
        op[ 0] = rg + (1 << 3);
        op[ 1] = 0x90;
        op[ 2] = 0x90;

        return 0;
    }

    return -1;
}
 
int dump_FIXUPP(const unsigned char b32,const int ofd) {
    unsigned char write_ledata = 0;
    unsigned long rec_ofs;
    unsigned int c;

    ledata_loaded = 0;
    if (last_LEDATA_ofd_data_offset == 0UL)
        return -1;
    if (last_LEDATA_ofd_omf_offset == 0UL)
        return -1;
    if (last_LEDATA_data_length >= 1024UL)
        return -1;

    /* remember where the file pointer is now */
    rec_ofs = lseek(ofd,0,SEEK_CUR);

//    printf("    FIXUPP%u:\n",b32?32:16);

    /* whoo, dense structures */
    while (!omfrec_eof()) {
        unsigned char delete_entry = 0;
        unsigned int beg_pos = omf_recpos;
        unsigned int target_datum;
        unsigned int frame_datum;

        c = omfrec_gb();

        if ((c&0xA0/*0x80+0x20*/) == 0x00/* 0 D 0 x x x x x */) {
            printf("        * error THREAD records not supported\n");
            break;
        }
        else if (c&0x80) {
            if (omfrec_eof()) break;

            {
                unsigned long target_disp = 0;
                unsigned char fix_f,fix_frame,fix_t,fix_p,fix_target;
                unsigned char fixdata;
                unsigned char segrel_fixup = (c >> 6) & 1;
                unsigned char locat = (c >> 2) & 0xF;
                unsigned int recoff = ((c & 3U) << 8U) + (unsigned int)omfrec_gb();

//                printf("        FIXUP %s loctofix=",segrel_fixup?"SEG-RELATIVE":"SELF-RELATIVE");
//                switch (locat) {
//                    case 0: printf("LOBYTE"); break;
//                    case 1: printf("16BIT-OFFSET"); break;
//                    case 2: printf("16BIT-SEGBASE"); break;
//                    case 3: printf("16SEG:16OFS-FAR-POINTER"); break;
//                    case 4: printf("HIBYTE"); break;
//                    case 5: printf("16BIT-LOADER-OFFSET"); break;
//                    case 9: printf("32BIT-OFFSET"); break;
//                    case 11:printf("16SEG:32OFS-FAR-POINTER"); break;
//                    case 13:printf("32BIT-LOADER-OFFSET"); break;
//                };
//                printf(" datarecofs=(rel)0x%lX,(abs)0x%lX",recoff,recoff+last_LEDATA_data_offset);
//                printf("\n");

                // FIXME: Is "Fix Data" conditional? If so, when does it happen??
                //        The OMF spec doesn't say! This is a part of the spec
                //        where the denseness and conditional nature is VERY
                //        problematic because the OMF spec is very bad at explaining
                //        WHEN these conditional fields appear.
                if (omfrec_eof()) break;
                fixdata = omfrec_gb();

                fix_f = (fixdata >> 7) & 1;     /* [7:7] */
                fix_frame = (fixdata >> 4) & 7; /* [6:4] */
                fix_t = (fixdata >> 3) & 1;     /* [3:3] */
                fix_p = (fixdata >> 2) & 1;     /* [2:2] */
                fix_target = (fixdata & 3);     /* [1:0] */

//                printf("            FD=0x%02X ",fixdata);
//                if (fix_f)
//                    printf("F=1 frame=%u",fix_frame);
//                else
//                    printf("F=0 framemethod=%u",fix_frame);

#if 0
                if (!fix_f) {
                    // again, if the OMF spec doesn't explain this and I have to read VirtualBox source code
                    // to get this that says how reliable your spec is.
                    switch (fix_frame) {
                        case 0: // F0: segment
                            printf(" F0:frame=segment");
                            break;
                        case 1: // F1: group
                            printf(" F1:frame=group");
                            break;
                        case 2: // F2: external symbol
                            printf(" F2:frame=external-symbol");
                            break;
                        case 4: // F4: frame = source
                            printf(" F4:frame=source");
                            break;
                        case 5: // F5: frame = target
                            printf(" F5:frame=target");
                            break;
                    }
                }
#endif

                if (!fix_f && fix_frame < 4) {
                    if (omfrec_eof()) break;

                    c = frame_datum = omfrec_gindex();
//                    printf(" framedatum=%u",c);

#if 0
                    switch (fix_frame) {
                        case 0: // segment
                            printf("(\"%s\")",omf_get_SEGDEF_name_safe(c));
                            break;
                        case 1: // group
                            printf("(\"%s\")",omf_get_GRPDEF_name_safe(c));
                            break;
                        case 2: // external symbol
                            printf("(\"%s\")",omf_get_EXTDEF_safe(c));
                            break;
                    }
#endif
                }

                /* FIXME: WHEN is the Target Datum field prsent???? This is a shitty guess! The OMF spec doesn't say! */
                // NTS: To the people who wrote the OMF spec: your doc is confusing. The fact I had to read VirtualBox
                //      source code for clarification means your spec needs clarification.
                if (fix_t) {
//                    printf(" target=%u",fix_target);
                }
                else {
                    if (omfrec_eof()) break;

#if 0
                    switch (fix_target) {
                        case 0: // T0/T4: Target = segment
                            printf(" T0:target=segment");
                            break;
                        case 1: // T1/T5: Target = segment group
                            printf(" T0:target=segment-group");
                            break;
                        case 2: // T2/T6: Target = external symbol
                            printf(" T0:target=extern-sym");
                            break;
                    };
#endif

                    c = target_datum = omfrec_gindex();
//                    printf(" targetdatum=%u",c);

#if 0
                    switch (fix_target) {
                        case 0: // T0/T4: Target = segment
                            printf("(\"%s\")",omf_get_SEGDEF_name_safe(c));
                            break;
                        case 1: // T1/T5: Target = segment group
                            printf("(\"%s\")",omf_get_GRPDEF_name_safe(c));
                            break;
                        case 2: // T2/T6: Target = external symbol
                            printf("(\"%s\")",omf_get_EXTDEF_safe(c));
                            break;
                    };
#endif
                }

                if (!fix_p) {
                    if (b32) {
                        if (omfrec_avail() < 4) break;
                        target_disp = omfrec_gd();
                    }
                    else {
                        if (omfrec_avail() < 2) break;
                        target_disp = omfrec_gw();
                    }

//                    printf(" targetdisp=%lu(0x%lx)",target_disp,target_disp);
                }

//                printf("\n");

                /* we care whether or not it's a segment reference.
                 * if it is, we either patch the opcode around it or emit an error */
                if (locat == 2/*SEGBASE*/) {
                    if (!fix_f && fix_frame == 5/*target*/) {
                        if (!fix_t && fix_target == 2/*extern*/) {
                            /* TODO: Add symbol to table, because often we first see this extern FIXUPP
                             *       then later see the LPUBDEF that declares it and the segment it belongs
                             *       to. If it turns out the extern we just patched was NOT part of DGROUP
                             *       then we need to print an error. */

                            /* OK, load the LEDATA segment from ofd if not already */
                            if (load_LEDATA(ofd) < 0) {
                                fprintf(stderr,"Failed to load LEDATA\n");
                                return -1;
                            }
                            if (range_check_LEDATA(recoff,2) < 0) {
                                fprintf(stderr,"Range check LEDATA recoff failed\n");
                                return -1; /* the FIXUPP is 2 bytes */
                            }

                            /* look at the instruction
                             *
                             * FIXME: This is WHY we need to know what segment it belongs to! Then if we know
                             * it's a data segment we can error out knowing we cannot patch it, and if it's
                             * a code segment we can patch as expected. */
                            if (segbase_patch_LEDATA(recoff) < 0) {
                                fprintf(stderr,"Unable to patch segment SEGBASE\n");
                                return -1;
                            }
                            write_ledata = 1;

                            /* don't carry this entry through */
                            delete_entry = 1;
                        }
                        else {
                            return -1;
                        }
                    }
                    else {
                        return -1;
                    }
                }
                else if (locat == 3/*16 SEG:OFF*/ || locat == 11/*32 SEG:OFF*/) {
                    return -1;
                }
            }

            if (delete_entry) {
                // rewind position, moving the omf record contents with it
                omfrec_backspace_to(beg_pos);

//                printf("        * deleting record\n");
            }
        }
        else {
            printf("        * error unknown record\n");
            break;
        }
    }

    /* update the LEDATA */
    if (ledata_loaded && write_ledata) {
        unsigned int i;
        unsigned char tmp[16];
        unsigned char sum = 0;
        unsigned long len = last_LEDATA_ofd_data_offset - last_LEDATA_ofd_omf_offset;

        if (len == 0 || len >= 16UL)
            return -1;
        if (lseek(ofd,last_LEDATA_ofd_omf_offset,SEEK_SET) != last_LEDATA_ofd_omf_offset)
            return -1;
        if (read(ofd,tmp,(int)len) != (int)len)
            return -1;

        for (i=0;i < (int)len;i++)
            sum += tmp[i];

        if (lseek(ofd,0,SEEK_CUR) != last_LEDATA_ofd_data_offset)
            return -1;

        if (last_LEDATA_data_length >= 1024UL)
            return -1;

        for (i=0;i < last_LEDATA_data_length;i++)
            sum += ledata[i];

        ledata[i] = 0x100 - sum;

        if (write(ofd,ledata,last_LEDATA_data_length+1) != (last_LEDATA_data_length+1))
            return -1;
    }

    /* now write the FIXUPP */
    if (lseek(ofd,rec_ofs,SEEK_SET) != rec_ofs)
        return -1;

    {
        unsigned char tmp[3];
        unsigned char sum;
        unsigned int i;

        sum = 0;

        tmp[0] = omf_rectype;
        *((uint16_t*)(tmp+1)) = omf_reclen + 1/*checksum*/;
        if (write(ofd,tmp,3) != 3) return -1;
        if (omf_reclen >= sizeof(omf_record)) return -1;
        for (i=0;i < 3;i++) sum += tmp[i];
        for (i=0;i < omf_reclen;i++) sum += omf_record[i];
        omf_record[omf_reclen] = 0x100 - sum; /* checksum */

        if (write(ofd,omf_record,omf_reclen+1) != (omf_reclen+1))
            return -1;
    }

    return 0;
}

/* NTS: I'm seeing Open Watcom emit .LIB files where more than one GRPDEF can be
 *      emitted with the same name, different segments listed. This is legal,
 *      it means you just combine the segment lists together (OMF spec). */
void dump_GRPDEF(const unsigned char b32) {
    unsigned int grpnamidx;
    unsigned int index;
    unsigned char segdefidx;

    if (omfrec_eof()) return;
    grpnamidx = omfrec_gindex();
//    printf("    GRPDEF nameidx=\"%s\"(%u):\n",omf_get_LNAME_safe(grpnamidx),grpnamidx);

    omf_GRPDEFS_add(grpnamidx);

    while (!omfrec_eof()) {
        index = omfrec_gb();

        if (index == 0xFF) {
            /* segment def index */
            segdefidx = omfrec_gindex();
//            printf("        SEGDEF index=\"%s\"(%u)\n",omf_get_SEGDEF_name_safe(segdefidx),segdefidx);
        }
        else {
            printf("* Whoops, don't know how to handle type 0x%02x\n",index);
            break;
        }
    }
}

void dump_SEGDEF(const unsigned char b32) {
    struct omf_SEGDEF_t *segdef;
    unsigned char align_f;
    unsigned char comb_f;
    unsigned char use32;
    unsigned char big;
    unsigned short frame_number;
    unsigned char offset;
    unsigned long seg_length;
    unsigned short segnamidx;
    unsigned short classnamidx;
    unsigned short ovlnamidx;
    unsigned char c;

    if (omfrec_eof()) return;
    c = omfrec_gb();
    align_f = (c >> 5) & 7; /* [7:5] = alignment code */
    comb_f = (c >> 2) & 7;  /* [4:2] = combination code */
    big = (c >> 1) & 1;     /* [1:1] = "BIG" meaning the segment is 64K or 4GB exactly and seg length == 0 */
    use32 = (c >> 0) & 1;   /* [0:0] = if set, 32-bit code/data  if clear, 16-bit code/data */

    if (align_f == 0) {
        if (omfrec_avail() < (2+1)) return;
        frame_number = omfrec_gw();
        offset = omfrec_gb();
    }

    if (b32) {
        if (omfrec_avail() < (4+1+1+1)) return;
        seg_length = omfrec_gd();
        segnamidx = omfrec_gindex();
        classnamidx = omfrec_gindex();
        ovlnamidx = omfrec_gindex();
    }
    else {
        if (omfrec_avail() < (2+1+1+1)) return;
        seg_length = omfrec_gw();
        segnamidx = omfrec_gindex();
        classnamidx = omfrec_gindex();
        ovlnamidx = omfrec_gindex();
    }

#if 0
    printf("    SEGDEF%u: USE%u",b32?32:16,use32?32:16);
    if (big && seg_length == 0) printf(" length=%s(max)",b32?"4GB":"64KB");
    else printf(" length=%lu",seg_length);
    printf(" nameidx=\"%s\"(%u) classidx=\"%s\"(%u) ovlidx=\"%s\"(%u)",
        omf_get_LNAME_safe(segnamidx),      segnamidx,
        omf_get_LNAME_safe(classnamidx),    classnamidx,
        omf_get_LNAME_safe(ovlnamidx),      ovlnamidx);
    printf("\n");

    switch (align_f) {
        case 0:
            printf("        ABSOLUTE frameno=%u offset=%u\n",frame_number,offset);
            break;
        case 1:
            printf("        RELOCATABLE, BYTE ALIGNMENT\n");
            break;
        case 2:
            printf("        RELOCATABLE, WORD (16-bit) ALIGNMENT\n");
            break;
        case 3:
            printf("        RELOCATABLE, PARAGRAPH (16-byte) ALIGNMENT\n");
            break;
        case 4:
            printf("        RELOCATABLE, PAGE ALIGNMENT\n");
            break;
        case 5:
            printf("        RELOCATABLE, DWORD (32-bit) ALIGNMENT\n");
            break;
    };

    switch (comb_f) {
        case 0:
            printf("        PRIVATE, do not combine with any other segment\n");
            break;
        case 2:
        case 4:
        case 7:
            printf("        PUBLIC, combine by appending at aligned offset\n");
            break;
        case 5:
            printf("        STACK, combine by appending at aligned offset\n");
            break;
        case 6:
            printf("        COMMON, combine by overlaying using max size\n");
            break;
    };
#endif

    segdef = omf_SEGDEFS_add(segnamidx);
}

void dump_LEDATA(const unsigned char b32,const int ofd) {
    unsigned long enum_data_offset,doh;
    unsigned int segment_index;
    unsigned int len,i,colo;
    unsigned char tmp[16];

    if (b32) {
        if (omfrec_avail() < (1+4)) return;
        segment_index = omfrec_gindex();
        enum_data_offset = omfrec_gd();
    }
    else {
        if (omfrec_avail() < (1+2)) return;
        segment_index = omfrec_gindex();
        enum_data_offset = omfrec_gw();
    }

    last_LEDATA_type = b32 ? 32 : 16;
    last_LEDATA_ofd_data_offset = lseek(ofd,0,SEEK_CUR) + 3 + omf_recpos; // where this record WILL BE in the output OBJ
    last_LEDATA_ofd_omf_offset = lseek(ofd,0,SEEK_CUR);
    last_LEDATA_data_length = omf_reclen - omf_recpos;
    last_LEDATA_segment_index = segment_index;
    last_LEDATA_data_offset = enum_data_offset;
}

void dump_LIDATA(const unsigned char b32) {
    unsigned long enum_data_offset,doh;
    unsigned int segment_index;

    if (b32) {
        if (omfrec_avail() < (1+4)) return;
        segment_index = omfrec_gindex();
        enum_data_offset = omfrec_gd();
    }
    else {
        if (omfrec_avail() < (1+2)) return;
        segment_index = omfrec_gindex();
        enum_data_offset = omfrec_gw();
    }

    last_LEDATA_type = 0;
    last_LEDATA_ofd_data_offset = 0;
    last_LEDATA_ofd_omf_offset = 0;
    last_LEDATA_data_length = 0;
    last_LEDATA_segment_index = segment_index;
    last_LEDATA_data_offset = enum_data_offset;
}

int copy_omf_record(int ofd) {
    unsigned char tmp[3];
    unsigned char sum;
    unsigned int i;

    sum = 0;

    tmp[0] = omf_rectype;
    *((uint16_t*)(tmp+1)) = omf_reclen + 1/*checksum*/;
    if (write(ofd,tmp,3) != 3) return -1;
    if (omf_reclen >= sizeof(omf_record)) return -1;
    for (i=0;i < 3;i++) sum += tmp[i];
    for (i=0;i < omf_reclen;i++) sum += omf_record[i];
    omf_record[omf_reclen] = 0x100 - sum; /* checksum */

    if (write(ofd,omf_record,omf_reclen+1) != (omf_reclen+1))
        return -1;

    return 0;
}

int main(int argc,char **argv) {
    char *temp_out = NULL;
    unsigned char lasttype;
    unsigned long lastofs;
    unsigned int lastlen;
    int i,fd,ofd;
    char *a;

    for (i=1;i < argc;) {
        a = argv[i++];

        if (*a == '-') {
            do { a++; } while (*a == '-');

            if (!strcmp(a,"i")) {
                in_file = argv[i++];
                if (in_file == NULL) return 1;
            }
            else if (!strcmp(a,"o")) {
                out_file = argv[i++];
                if (out_file == NULL) return 1;
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

    if (in_file == NULL || out_file == NULL) {
        help();
        return 1;
    }

    fd = open(in_file,O_RDONLY|O_BINARY);
    if (fd < 0) {
        fprintf(stderr,"Failed to open input file %s\n",strerror(errno));
        return 1;
    }

    if (!strcmp(in_file,out_file)) {
        temp_out = strdup(out_file);
        if (temp_out == NULL)
            return 1;

        /* change .obj to .obt */
        {
            char *x = strrchr(temp_out,'.');
            if (x == NULL) return 1;
            if (strcasecmp(x,".obj") != 0) return 1;
            strcpy(x,".obt");
        }

        ofd = open(temp_out,O_RDWR|O_CREAT|O_BINARY);
        if (ofd < 0) {
            fprintf(stderr,"Failed to open output file %s\n",strerror(errno));
            return 1;
        }
    }
    else {
        ofd = open(out_file,O_RDWR|O_CREAT|O_BINARY);
        if (ofd < 0) {
            fprintf(stderr,"Failed to open output file %s\n",strerror(errno));
            return 1;
        }
    }

    while (read_omf_record(fd)) {
        lastlen = omf_reclen;
        lastofs = omf_recoffs;
        lasttype = omf_rectype;

#if 0
        printf("OMF record type=0x%02x (%s: %s) length=%u offset=%lu\n",
                omf_rectype,
                omf_rectype_to_str(omf_rectype),
                omf_rectype_to_str_long(omf_rectype),
                omf_reclen,
                omf_recoffs);
#endif

        switch (omf_rectype) {
            case 0x80:/* THEADR */
                if (copy_omf_record(ofd) < 0) return 1;
                break;
            case 0x82:/* LHEADR */
                if (copy_omf_record(ofd) < 0) return 1;
                break;
            case 0x88:/* COMENT */
                if (copy_omf_record(ofd) < 0) return 1;
                break;
            case 0x8A:/* MODEND */
            case 0x8B:/* MODEND32 */
                if (copy_omf_record(ofd) < 0) return 1;
                break;
            case 0x8C:/* EXTDEF */
                dump_EXTDEF();
                if (copy_omf_record(ofd) < 0) return 1;
                break;
            case 0x90:/* PUBDEF */
            case 0x91:/* PUBDEF32 */
                if (copy_omf_record(ofd) < 0) return 1;
                break;
            case 0x96:/* LNAMES */
                dump_LNAMES();
                if (copy_omf_record(ofd) < 0) return 1;
                break;
            case 0x98:/* SEGDEF */
            case 0x99:/* SEGDEF32 */
                dump_SEGDEF(omf_rectype&1);
                if (copy_omf_record(ofd) < 0) return 1;
                break;
            case 0x9A:/* GRPDEF */
            case 0x9B:/* GRPDEF32 */
                dump_GRPDEF(omf_rectype&1);
                if (copy_omf_record(ofd) < 0) return 1;
                break;
            case 0x9C:/* FIXUPP */
            case 0x9D:/* FIXUPP32 */
                if (dump_FIXUPP(omf_rectype&1,ofd) < 0) return 1;
                break;
            case 0xA0:/* LEDATA */
            case 0xA1:/* LEDATA32 */
                dump_LEDATA(omf_rectype&1,ofd);
                if (copy_omf_record(ofd) < 0) return 1;
                break;
            case 0xA2:/* LIDATA */
            case 0xA3:/* LIDATA32 */
                dump_LIDATA(omf_rectype&1);
                if (copy_omf_record(ofd) < 0) return 1;
                break;
            case 0xB4:/* LEXTDEF */
            case 0xB5:/* LEXTDEF32 */
                dump_LEXTDEF(omf_rectype&1);
                if (copy_omf_record(ofd) < 0) return 1;
                break;
            case 0xB6:/* LPUBDEF */
            case 0xB7:/* LPUBDEF32 */
                if (copy_omf_record(ofd) < 0) return 1;
                break;
            case 0xF0:/* LIBHEAD */
                fprintf(stderr,"** ERROR: Do not use this tool on .LIB files\n");
                return 1;
            case 0xF1:/* LIBEND */
                fprintf(stderr,"** ERROR: Do not use this tool on .LIB files\n");
                return 1;
            default:
                fprintf(stderr,"** ERROR: do not yet support OMF record type 0x%02x\n",omf_rectype);
                return 1;
        }

        omfrec_checkrange();
    }

    omf_reset();
    close(ofd);
    close(fd);

    if (temp_out != NULL) {
        unlink(out_file);
        if (rename(temp_out,out_file) < 0)
            return 1;
    }

    return 0;
}

