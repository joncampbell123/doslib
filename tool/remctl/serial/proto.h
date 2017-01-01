
#pragma pack(push,1)
/* this is loosely based on Kermit, but not the same protocol */
struct remctl_serial_packet_header {
    unsigned char       mark;
    unsigned char       length;
    unsigned char       sequence;
    unsigned char       type;
    unsigned char       chksum;
};

struct remctl_serial_packet {
    struct remctl_serial_packet_header      hdr;
    unsigned char                           data[256];
};
#pragma pack(pop)

#define REMCTL_SERIAL_MARK      0x01

enum {
    REMCTL_SERIAL_TYPE_DOS=0x44,        /* MS-DOS specific */
    REMCTL_SERIAL_TYPE_ERROR=0x45,
    REMCTL_SERIAL_TYPE_FILE=0x46,       /* file I/O specific */
    REMCTL_SERIAL_TYPE_HALT=0x48,       /* halt/un-halt system */
    REMCTL_SERIAL_TYPE_INPORT=0x49,     /* input from port */
    REMCTL_SERIAL_TYPE_OUTPORT=0x4F,    /* output to port */
    REMCTL_SERIAL_TYPE_MEMREAD=0x52,    /* read from memory */
    REMCTL_SERIAL_TYPE_PING=0x50,
    REMCTL_SERIAL_TYPE_MEMWRITE=0x57    /* write to memory */
};

/* REMCTL_SERIAL_TYPE_DOS */
enum {
    REMCTL_SERIAL_TYPE_DOS_LOL=0x4C,    /* return List of Lists pointer */
    REMCTL_SERIAL_TYPE_DOS_INDOS=0x49,  /* return InDOS pointer */
    REMCTL_SERIAL_TYPE_DOS_STUFF_BIOS_KEYBOARD=0x42 /* stuff scancode into BIOS keyboard buffer */
};

/* REMCTL_SERIAL_TYPE_FILE */
enum {
    REMCTL_SERIAL_TYPE_FILE_MSDOS_IS_BUSY=0x02,     /* response code CTRL+B */
    REMCTL_SERIAL_TYPE_FILE_MSDOS_ERROR=0x05,       /* response code CTRL+E */
    REMCTL_SERIAL_TYPE_FILE_MSDOS_FINISHED=0x06,    /* response code CTRL+F */
    REMCTL_SERIAL_TYPE_FILE_CHDIR=0x43,             /* chdir ASCIIZ (WARNING: CwD is GLOBAL in MS-DOS, affects application!) */
    REMCTL_SERIAL_TYPE_FILE_FIND=0x46,              /* find (will return multiple packets until end) */
    REMCTL_SERIAL_TYPE_FILE_MKDIR=0x4D,             /* mkdir ASCIIZ */
    REMCTL_SERIAL_TYPE_FILE_OPEN=0x4F,              /* open() a file. ONLY ONE FILE AT A TIME. will close prior file. will not create. */
    REMCTL_SERIAL_TYPE_FILE_PWD=0x50,               /* get current directory */
    REMCTL_SERIAL_TYPE_FILE_RMDIR=0x52,             /* rmdir ASCIIZ */
    REMCTL_SERIAL_TYPE_FILE_TRUNCATE=0x54,          /* truncate open file to file pointer */
    REMCTL_SERIAL_TYPE_FILE_CLOSE=0x63,             /* close the open file */
    REMCTL_SERIAL_TYPE_FILE_CREATE=0x72,            /* create a new file. will close prior file. */
    REMCTL_SERIAL_TYPE_FILE_READ=0x79,              /* read file */
    REMCTL_SERIAL_TYPE_FILE_WRITE=0x7A,             /* write file */
    REMCTL_SERIAL_TYPE_FILE_SEEK=0x7B               /* seek file pointer */
};

