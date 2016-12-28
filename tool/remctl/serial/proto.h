
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
    REMCTL_SERIAL_TYPE_ERROR=0x45,
    REMCTL_SERIAL_TYPE_HALT=0x48,       /* halt/un-halt system */
    REMCTL_SERIAL_TYPE_INPORT=0x49,     /* input from port */
    REMCTL_SERIAL_TYPE_PING=0x50
};

