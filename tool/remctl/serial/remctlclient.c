/* client-side Linux (eventually Windows?) */

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>

/* for connecting to localhost (DOSBox null modem emulation) */
#include <netinet/in.h>
#include <netinet/ip.h>

/* for connecting to serial port */
#include <termios.h>

#include <unistd.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#include "proto.h"

static int              localhost_port = -1;
static char*            serial_tty = NULL;
static int              localhost = 0;
static int              waitconn = 0;
static char*            command = NULL;

static int              conn_fd = -1;

static void help(void) {
    fprintf(stderr,"remctlclient [options]\n");
    fprintf(stderr,"Remote control client for RS-232 control of a DOS system.\n");
    fprintf(stderr,"  -h --help         Show this help\n");
    fprintf(stderr,"  -l                Connect to localhost (DOSBox VM)\n");
    fprintf(stderr,"  -p <N>            localhost port number (default 23)\n");
    fprintf(stderr,"  -s <dev>          Connect to serial port device\n");
    fprintf(stderr,"  -w                Wait for connection\n");
    fprintf(stderr,"  -c <command>      Command to execute (default: ping)\n");
}

static int parse_argv(int argc,char **argv) {
    char *a;
    int i=1;

    while (i < argc) {
        a = argv[i++];

        if (*a == '-') {
            do { a++; } while (*a == '-');

            if (!strcmp(a,"h") || !strcmp(a,"help")) {
                help();
                return 1;
            }
            else if (!strcmp(a,"l")) {
                localhost = 1;
            }
            else if (!strcmp(a,"p")) {
                a = argv[i++];
                if (a == NULL) return 1;
                localhost_port = atoi(a);
                localhost = 1;
            }
            else if (!strcmp(a,"c")) {
                a = argv[i++];
                if (a == NULL) return 1;
                command = a;
            }
            else if (!strcmp(a,"s")) {
                a = argv[i++];
                if (a == NULL) return 1;
                serial_tty = a;
            }
            else if (!strcmp(a,"w")) {
                waitconn = 1;
            }
            else {
                fprintf(stderr,"Unknown switch %s\n",a);
                return 1;
            }
        }
        else {
            fprintf(stderr,"Unexpected argv\n");
            return 1;
        }
    }

    if (command == NULL)
        command = "ping";

    if (serial_tty != NULL) {
        fprintf(stderr,"Serial port support not yet implemented\n");
        return 1;
    }
    else if (localhost) {
        if (localhost_port < 0)
            localhost_port = 23;

        if (localhost_port < 1 || localhost_port > 65534) {
            fprintf(stderr,"Invalid localhost port\n");
            return 1;
        }
    }
    else {
        help();
        return 1;
    }

    return 0;
}

void drop_connection(void) {
    if (conn_fd >= 0) {
        close(conn_fd);
        conn_fd = -1;
    }
}

int do_connection(void) {
    if (conn_fd >= 0)
        return 0;

    if (localhost) {
        struct sockaddr_in sin;

        conn_fd = socket(AF_INET,SOCK_STREAM,PF_UNSPEC);
        if (conn_fd < 0) {
            fprintf(stderr,"Socket() failed, %s\n",strerror(errno));
            drop_connection();
            return -1;
        }

        memset(&sin,0,sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_port = htons(localhost_port);
        sin.sin_addr.s_addr = htonl(0x7F000001UL); /* 127.0.0.1 */
        if (connect(conn_fd,(const struct sockaddr*)(&sin),sizeof(sin)) < 0) {
            fprintf(stderr,"Connect failed, %s\n",strerror(errno));
            drop_connection();
            return -1;
        }
    }
    else {
        return -1;
    }

    return 0;
}

int do_connect(void) {
    while (conn_fd < 0) {
        if (do_connection()) {
            if (!waitconn)
                return -1;
            else
                usleep(250);
        }
    }

    return 0;
}

int do_disconnect(void) {
    drop_connection();
    return 0;
}

struct remctl_serial_packet         cur_pkt;
unsigned char                       cur_pkt_seq=0xFF;
unsigned char                       cur_pkt_recv_seq=0xFF;

void remctl_serial_packet_begin(struct remctl_serial_packet * const pkt,const unsigned char type) {
    pkt->hdr.mark = REMCTL_SERIAL_MARK;
    pkt->hdr.length = 0;
    pkt->hdr.sequence = cur_pkt_seq;
    pkt->hdr.type = type;
    pkt->hdr.chksum = 0;

    if (cur_pkt_seq == 0xFF)
        cur_pkt_seq = 0;
    else
        cur_pkt_seq = (cur_pkt_seq+1)&0x7F;
}

void remctl_serial_packet_end(struct remctl_serial_packet * const pkt) {
    unsigned char sum = 0;
    unsigned int i;

    for (i=0;i < pkt->hdr.length;i++)
        sum += pkt->data[i];

    pkt->hdr.chksum = 0x100 - sum;
}

int do_send_packet(const struct remctl_serial_packet * const pkt) {
    if (conn_fd < 0)
        return -1;

    if (send(conn_fd,(const void*)(&(pkt->hdr)),sizeof(pkt->hdr),MSG_NOSIGNAL) != sizeof(pkt->hdr)) {
        fprintf(stderr,"Send failed: header\n");
        drop_connection();
        return -1;
    }

    if (pkt->hdr.length != 0 && send(conn_fd,pkt->data,pkt->hdr.length,MSG_NOSIGNAL) != pkt->hdr.length) {
        fprintf(stderr,"Send failed: data\n");
        drop_connection();
        return -1;
    }

    return 0;
}

int do_recv_packet(struct remctl_serial_packet * const pkt) {
    if (conn_fd < 0)
        return -1;

    do {
        if (recv(conn_fd,&(pkt->hdr.mark),1,MSG_WAITALL) != 1) {
            drop_connection();
            return -1;
        }
    } while (pkt->hdr.mark != REMCTL_SERIAL_MARK);

    if (recv(conn_fd,&(pkt->hdr.length),sizeof(pkt->hdr)-offsetof(struct remctl_serial_packet_header,length),MSG_WAITALL) !=
        (sizeof(pkt->hdr)-offsetof(struct remctl_serial_packet_header,length))) {
        drop_connection();
        return -1;
    }

    if (pkt->hdr.length != 0) {
        if (recv(conn_fd,pkt->data,pkt->hdr.length,MSG_WAITALL) != pkt->hdr.length) {
            drop_connection();
            return -1;
        }
    }

    {
        unsigned char sum = pkt->hdr.chksum;
        unsigned int i;

        for (i=0;i < pkt->hdr.length;i++)
            sum += pkt->data[i];

        if (sum != 0) {
            fprintf(stderr,"Checksum failed\n");
            drop_connection();
            return -1;
        }
    }

    if (cur_pkt_recv_seq != 0xFF) {
        if (pkt->hdr.sequence != cur_pkt_recv_seq) {
            fprintf(stderr,"Sequence failure\n");
            drop_connection();
            return -1;
        }

        cur_pkt_recv_seq = (cur_pkt_recv_seq+1)&0x7F;
    }
    else {
        cur_pkt_recv_seq = (pkt->hdr.sequence+1)&0x7F;
    }

    return 0;
}

int main(int argc,char **argv) {
    if (parse_argv(argc,argv))
        return 1;

    if (do_connect())
        return 1;

    if (!strcmp(command,"ping")) {
        remctl_serial_packet_begin(&cur_pkt,REMCTL_SERIAL_TYPE_PING);

        strncpy((char*)(cur_pkt.data+cur_pkt.hdr.length),"PING",4);
        cur_pkt.hdr.length += 4;

        remctl_serial_packet_end(&cur_pkt);

        if (do_send_packet(&cur_pkt) < 0) {
            fprintf(stderr,"Failed to send packet\n");
            return 1;
        }

        alarm(5);
        if (do_recv_packet(&cur_pkt) < 0) {
            fprintf(stderr,"Failed to recv packet\n");
            return 1;
        }
    }
    else {
        fprintf(stderr,"Unknown command\n");
        return 1;
    }

    do_disconnect();
    return 0;
}

