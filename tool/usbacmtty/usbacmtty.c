
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>

/* for connecting to serial port */
#include <termios.h>

#include <assert.h>
#include <unistd.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>

#ifndef O_BINARY
#define O_BINARY (0)
#endif

static int              baud_rate = -1;
static char*            serial_tty = NULL;
static int              waitconn = 0;
static int              debug = 0;
static int              stop_bits = 1;
static char             buffer_line = 0;

static int              conn_fd = -1;

static void help(void) {
    fprintf(stderr,"usbacmtty [options]\n");
    fprintf(stderr,"TTY interface to USB ACM devices.\n");
    fprintf(stderr,"  -h --help         Show this help\n");
    fprintf(stderr,"  -s <dev>          Connect to serial port device\n");
    fprintf(stderr,"  -w                Wait for connection\n");
    fprintf(stderr,"  -b <rate>         Baud rate\n");
    fprintf(stderr,"  -sb <n>           Stop bits (1 or 2)\n");
    fprintf(stderr,"  -lb               Buffer input by line (for 86duino USB redir)\n");
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
            else if (!strcmp(a,"b")) {
                a = argv[i++];
                if (a == NULL) return 1;
                baud_rate = strtoul(a,NULL,0);
            }
            else if (!strcmp(a,"s")) {
                a = argv[i++];
                if (a == NULL) return 1;
                serial_tty = a;
            }
            else if (!strcmp(a,"sb")) {
                a = argv[i++];
                if (a == NULL) return 1;
                stop_bits = atoi(a);
                if (stop_bits < 1) stop_bits = 1;
                else if (stop_bits > 2) stop_bits = 2;
            }
            else if (!strcmp(a,"lb")) {
                buffer_line = 1;
            }
            else if (!strcmp(a,"w")) {
                waitconn = 1;
            }
            else if (!strcmp(a,"d")) {
                debug = 1;
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

    if (baud_rate < 0)
        baud_rate = 115200;

    if (serial_tty != NULL) {
        /* Motherboard RS-232 ports are usually /dev/ttyS0, /dev/ttyS1, etc.
         * USB RS-232 ports are usually /dev/ttyUSB0, /dev/ttyUSB1, etc. */
        if (!strncmp(serial_tty,"/dev/tty",8)) {
            /* good */
        }
        else {
            return 1;
        }
    }
    else {
        help();
        return 1;
    }

    return 0;
}

/* make a file descriptor non-blocking */
int nonblocking_fd(const int fd) {
    int x,r;

    x = fcntl(fd,F_GETFL);
    if (x < 0) return x;

    x |= O_NONBLOCK;

    if ((r=fcntl(fd,F_SETFL,x)) < 0)
        return r;

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

    if (serial_tty != NULL) {
        struct termios tios;

        conn_fd = open(serial_tty,O_RDWR);
        if (conn_fd < 0) {
            fprintf(stderr,"open() failed, %s\n",strerror(errno));
            drop_connection();
            return -1;
        }

        if (tcgetattr(conn_fd,&tios) < 0) {
            fprintf(stderr,"Failed to get termios info, %s\n",strerror(errno));
            drop_connection();
            return -1;
        }
        tcflush(conn_fd,TCIOFLUSH);
        cfmakeraw(&tios);
        if (cfsetspeed(&tios,baud_rate) < 0) {
            fprintf(stderr,"Failed to set baud rate, %s\n",strerror(errno));
            drop_connection();
            return -1;
        }

        tios.c_cflag &= ~CSTOPB;
        if (stop_bits == 2) tios.c_cflag |= CSTOPB;
        tios.c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL|ECHOPRT|ECHOKE);
        tios.c_cflag &= ~(CSIZE);
        tios.c_cflag |= CS8;

        if (tcsetattr(conn_fd,TCSANOW,&tios) < 0) {
            fprintf(stderr,"Failed to set termios info, %s\n",strerror(errno));
            drop_connection();
            return -1;
        }

        if (nonblocking_fd(conn_fd) < 0)
            fprintf(stderr,"WARNING: failed to make socket non-blocking\n");
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
                usleep(250000);
        }
    }

    return 0;
}

int do_disconnect(void) {
    drop_connection();
    return 0;
}

int write_persistent(const int fd,const void *p,int sz) {
    int rd = 0;
    int rt;

    while (sz > 0) {
        rt = write(fd,p,sz);
        if (rt < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* non-blocking handling */
                usleep(1000);
                continue;
            }
            return rt;
        }
        if (rt == 0) break; /* EOF */
        assert(rt <= sz);
        p = (const void*)((const char*)p + rt);
        sz -= rt;
        rd += rt;
    }

    return rd;
}

int main(int argc,char **argv) {
    struct termios stdin_oldios;
    char linebuffer[512];
    int linebufpos = 0;
    char c;

    if (parse_argv(argc,argv))
        return 1;

    if (do_connect())
        return 1;

    if (isatty(0)) {
        nonblocking_fd(0);

        {
            struct termios tios;

            if (tcgetattr(0,&tios) < 0)
                return -1;

            stdin_oldios = tios;
            tcflush(conn_fd,TCIOFLUSH);

            tios.c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL|ECHOPRT|ECHOKE|ICANON);
            tios.c_cflag &= ~(CSIZE);
            tios.c_cflag |= CS8;

            if (tcsetattr(0,TCSANOW,&tios) < 0)
                return -1;
        }
    }

    if (isatty(1)) {
        nonblocking_fd(1);
    }

    do {
        {
            int r = read(0/*stdin*/,&c,1);
            if (r == 0)
                break;
            else if (r < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                }
                else {
                    break;
                }
            }
            else {
                if (buffer_line) {
                    if (c == 13 || c == 10 || (size_t)linebufpos >= sizeof(linebuffer)) {
                        if (write_persistent(1/*stdout*/,"\r\n",2) != 2)
                            break;
                        if (write_persistent(conn_fd,linebuffer,linebufpos) != linebufpos)
                            break;

                        linebufpos = 0;
                    }
                    else if (c == 8 || c == 127) {
                        if (linebufpos > 0) {
                            linebufpos--;
                            if (write_persistent(1/*stdout*/,"\x08 \x08",3) != 3)
                                break;
                        }
                    }
                    else {
                        if (write_persistent(1/*stdout*/,&c,1) != 1)
                            break;

                        linebuffer[linebufpos++] = c;
                    }
                }
                else {
                    if (write_persistent(conn_fd,&c,1) != 1)
                        break;
                }
            }
        }

        {
            int r = read(conn_fd,&c,1);
            if (r == 0)
                break;
            else if (r < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                }
                else {
                    break;
                }
            }
            else {
                if (write_persistent(1/*stdout*/,&c,1) != 1)
                    break;
            }
        }
    } while (1);

    if (tcsetattr(0,TCSANOW,&stdin_oldios) < 0)
        return -1;

    do_disconnect();
    return 0;
}

