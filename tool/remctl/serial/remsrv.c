
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>
#include <conio.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>

#ifdef TARGET_PC98
# include <hw/8251/8251.h>
#else
# include <hw/8250/8250.h>
# include <hw/8250/8250pnp.h>
#endif

#include <hw/isapnp/isapnp.h>
#include <hw/flatreal/flatreal.h>

#include "proto.h"

#ifdef TARGET_PC98
static struct uart_8251 *uart = NULL;
#else
static struct info_8250 *uart = NULL;
#endif

static unsigned char tsr_init = 0;
static int           user_sel_port = -1;
static unsigned long baud_rate = 115200;
static unsigned long ic_delay_counter = 0;
static unsigned char use_interrupts = 1;
static unsigned char in_packet_handling = 0;                        // if within packet handling loop (reentrancy protection)
static unsigned char halt_system = 0;
static unsigned char inside_int28 = 0;                              // if within INT 28h
static unsigned short my_resident_psp = 0;                          // nonzero if resident TSR, else we're still running
static unsigned short saved_psp = 0;                                // if switched, switch back
static unsigned char stop_bits = 1;

static int open_file_fd = -1;                                       // currently open file

static unsigned char far *saved_dta = NULL;                         // within TSR, saved DTA

static unsigned char far *InDOS_ptr = NULL;                         // MS-DOS InDOS flag
#define InDOSFlag           (InDOS_ptr[0])
#define ErrorModeFlag       (InDOS_ptr[-1])

static unsigned char far *DOS_LOL = NULL;                           // MS-DOS List of Lists (not funny)

static struct remctl_serial_packet      cur_pkt_in = {0};
static unsigned char                    cur_pkt_in_write = 0;       // from 0 to < sizeof(cur_pkt_in)
static unsigned char                    cur_pkt_in_seq = 0xFF;

static struct remctl_serial_packet      cur_pkt_out = {0};
static unsigned char                    cur_pkt_out_write = 0;      // from 0 to < sizeof(cur_pkt_out)
static unsigned char                    cur_pkt_out_seq = 0xFF;

#ifdef TARGET_PC98
void pc98_uart_irq_update(void) {
    /* NTS: Unlike the IBM PC UARTs, we can't just leave all interrupt signals
     *      turned on and filter for it, we must selectively enable interrupt
     *      signal masks for the RS-232 port.
     *
     *      Apparently this is the behavior of the Intel 8251 UART.
     *
     *      Leaving the RxREADY interrupt signal enabled at all times is fine,
     *      but TxEMPTY and TxREADY are problematic in that, if the UART signals
     *      an interrupt for them and we don't send a byte, the UART will not
     *      signal again even if a byte is ready. To keep the ISR working we
     *      must switch the TxEMPTY and TxREADY masks on and off depending on
     *      whether we have data to send */
    outp(0x35,(inp(0x35) & (~7)) | 1/*RxRDY*/ | (cur_pkt_out.hdr.mark == REMCTL_SERIAL_MARK ? 4/*TxRDY*/ : 0));
}
#endif

void process_input(void);
void process_output(void);
void do_process_output(void);

int uart_waiting_read = 0;
int uart_waiting_write = 0;

unsigned short dos_get_psp(void) {
    unsigned short psp=0;

    __asm {
        push    ax
        push    bx
        mov     ah,0x51     ; get PSP segment
        int     21h
        mov     psp,bx
        pop     bx
        pop     ax
    }

    return psp;
}

void dos_set_psp(const unsigned short psp) {
    __asm {
        push    ax
        push    bx
        mov     ah,0x50     ; set PSP segment
        mov     bx,psp
        int     21h
        pop     bx
        pop     ax
    }
}

unsigned char far *dos_get_dta(void) {
    unsigned short so=0,oo=0;

    __asm {
        push    ax
        push    bx
        push    es
        mov     ah,0x2F             ; get DTA
        int     21h
        mov     so,es
        mov     oo,bx
        pop     es
        pop     bx
        pop     ax
    }

    return (unsigned char far*)MK_FP(so,oo);
}

void dos_set_dta(const unsigned char far * const dta) {
    __asm {
        push    ax
        push    bx
        push    ds
        mov     ah,0x1A             ; set DTA
        lds     dx,dta              ; DS:DX = dta
        int     21h
        pop     ds
        pop     bx
        pop     ax
    }
}

#ifdef TARGET_PC98
static void irq_uart_handle_iir(struct uart_8251 *uart) {
    unsigned char act_reason = 0;

    /* why the interrupt? */
    /* NOTE: we loop a maximum of 8 times in case the UART we're talking to happens
     *       to be some cheap knockoff chipset that never clears the IIR register
     *       when all events are read */
    /* if there was actually an interrupt, then handle it. loop until all interrupt conditions are depleted */
    if ((uart_8251_status(uart)&7) != 0) {
        // 2 = data avail
        // 1 = transmit empty
        act_reason = 1;
    }

    if (act_reason) {
        uart_waiting_read = 0;
        uart_waiting_write = 0;
        process_input();
        process_output();
    }

    pc98_uart_irq_update();
}
#else
/* NOTE: You're supposed to call this function with interrupts disabled,
 *       or from within an interrupt handler in response to the UART's IRQ signal. */
static void irq_uart_handle_iir(struct info_8250 *uart) {
    unsigned char reason,c,patience = 8,act_reason = 0;

    /* why the interrupt? */
    /* NOTE: we loop a maximum of 8 times in case the UART we're talking to happens
     *       to be some cheap knockoff chipset that never clears the IIR register
     *       when all events are read */
    /* if there was actually an interrupt, then handle it. loop until all interrupt conditions are depleted */
    while (((reason=inp(uart->port+PORT_8250_IIR)&7)&1) == 0) {
        reason >>= 1;

        if (reason == 3) { /* line status */
            c = inp(uart->port+PORT_8250_LSR);
            /* do what you will with this info */
        }
        else if (reason == 0) { /* modem status */
            c = inp(uart->port+PORT_8250_MSR);
            /* do what you will with this info */
        }
        else {
            // 2 = data avail
            // 1 = transmit empty
            act_reason = 1;
        }

        if (--patience == 0)
            break;
    }

    if (act_reason) {
        uart_waiting_read = 0;
        uart_waiting_write = 0;
        process_input();
        process_output();
    }
}
#endif

void halt_system_loop(void) {
    /* interrupts at this point should be disabled */
    do {
        uart_waiting_read = 0;
        uart_waiting_write = 0;
        process_input();
        process_output();
#ifdef TARGET_PC98
        pc98_uart_irq_update();
#endif
    } while (halt_system);
}

void close_open_file(void) {
    if (open_file_fd >= 0) {
        close(open_file_fd);
        open_file_fd = -1;
    }
}

static void (interrupt *old_int28)() = NULL;
static void interrupt my_int28() {
    /* chain to previous FIRST, then do our work */
    old_int28();

    /* OK, do our work. It's safe to do MS-DOS INT 21h from here except console I/O functions AH < 0x0C */
    inside_int28 = 1;

    if (ic_delay_counter != 0UL) {
        unsigned long c = ic_delay_counter;

        _sti();
        while ((--c) != 0UL) {
            __asm nop
        }
    }

    _cli();
    process_input();
    process_output();
#ifdef TARGET_PC98
    pc98_uart_irq_update();
#endif

    /* halt here if instructed */
    if (halt_system) halt_system_loop();

    /* done */
    inside_int28 = 0;
}

static unsigned char safe_to_use_msdos_fs_io(void) {
    if (ErrorModeFlag) return 0; /* if MS-DOS is in critical state, NO */
    if (!InDOSFlag) return 1; /* if MS-DOS says it's safe to enter, YES */
    if (inside_int28) return 1; /* if inside MS-DOS, but inside INT 28h IDLE call, YES */
    return 0; /* else, NO */
}

static void (interrupt *old_timer_irq)() = NULL;
static void interrupt timer_irq() {
    if (use_interrupts && uart->irq != -1) {
        unsigned char fix_irq = 0;
#ifndef TARGET_PC98
        unsigned int i;
#endif

#ifdef TARGET_PC98
        if (uart_8251_status(uart)&2) {
            if ((++uart_waiting_read) >= 9) {
                uart_waiting_read = 0;
                fix_irq = 1;
            }
        }
        if (uart_8251_status(uart)&1) {
            if ((++uart_waiting_write) >= 9) {
                uart_waiting_write = 0;
                fix_irq = 1;
            }
        }

        if (fix_irq) {
            _cli();
            p8259_mask(uart->irq);

            if (uart->irq >= 8) p8259_OCW2(8,P8259_OCW2_SPECIFIC_EOI | (uart->irq&7));
            p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | (uart->irq&7));

            process_input();
            process_output();
            pc98_uart_irq_update();
            p8259_unmask(uart->irq);
        }
#else
        if (uart_8250_can_read(uart)) {
            if ((++uart_waiting_read) >= 9) {
                uart_waiting_read = 0;
                fix_irq = 1;
            }
        }
        if (uart_8250_can_write(uart)) {
            if ((++uart_waiting_write) >= 9) {
                uart_waiting_write = 0;
                fix_irq = 1;
            }
        }

        if (fix_irq) {
            _cli();
            p8259_mask(uart->irq);

            if (uart->irq >= 8) p8259_OCW2(8,P8259_OCW2_SPECIFIC_EOI | (uart->irq&7));
            p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | (uart->irq&7));

            uart_8250_enable_interrupt(uart,0x0);
            uart_8250_set_MCR(uart,3);      /* turn on RTS and DTS */
            uart_8250_set_line_control(uart,UART_8250_LCR_8BIT | UART_8250_LCR_PARITY | (stop_bits == 2 ? UART_8250_LCR_TWO_STOP_BITS : 0)); /* 8 bit 1 stop bit odd parity */
            uart_8250_set_baudrate(uart,uart_8250_baud_to_divisor(uart,baud_rate));

            for (i=0;i < 256 && (inp(uart->port+PORT_8250_IIR) & 1) == 0;i++);

            uart_8250_enable_interrupt(uart,0xF);
            process_input();
            process_output();
            p8259_unmask(uart->irq);
        }
#endif
    }

    /* halt here if instructed */
    if (halt_system) halt_system_loop();

    old_timer_irq();
}

static void (interrupt *old_irq)() = NULL;
static void interrupt uart_irq() {
    /* clear interrupts, just in case. NTS: the nature of interrupt handlers
     * on the x86 platform (IF in EFLAGS) ensures interrupts will be reenabled on exit */
    _cli();

    /* ack PIC */
    if (uart->irq >= 8) p8259_OCW2(8,P8259_OCW2_NON_SPECIFIC_EOI);
    p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);

    /* After acknowledging the interrupt, THEN process the data
     * and possibly call into DOS. There's a reason for this.
     * On some systems, the RS-232 IRQ might be higher priority
     * than the disk drive we're trying to access. Failure to
     * ack the serial IRQ before disk I/O isn't an issue on the
     * IBM PC because IRQ 15 is higher priority than IRQ 3, BUT
     * on PC-98 this is vital because IRQ 4 (RS-232C) is higher
     * priority than the hard drive. Failure to ack our IRQ
     * prevents the hard drive from working and a long "hang"
     * within MS-DOS and the BIOS! */
    irq_uart_handle_iir(uart);

    /* halt here if instructed */
    if (halt_system) halt_system_loop();
}

void begin_output_packet(const unsigned char type) {
    cur_pkt_out.hdr.mark = REMCTL_SERIAL_MARK;
    cur_pkt_out.hdr.length = 0;
    cur_pkt_out.hdr.sequence = cur_pkt_out_seq;
    cur_pkt_out.hdr.type = type;
    cur_pkt_out.hdr.chksum = 0;

    if (cur_pkt_out_seq == 0xFF)
        cur_pkt_out_seq = 0;
    else
        cur_pkt_out_seq = (cur_pkt_out_seq + 1) & 0x7F;
}

void end_output_packet(void) {
    unsigned char sum = 0;
    unsigned int i;

    for (i=0;i < cur_pkt_out.hdr.length;i++)
        sum += cur_pkt_out.data[i];

    cur_pkt_out.hdr.chksum = 0x100 - sum;
}

unsigned char is_v86_mode(void) {
#if TARGET_MSDOS == 16
    unsigned int tmp = 0;

    if (cpu_basic_level < 3)
        return 0;

    __asm {
        .286
        smsw    tmp
    }

    return tmp & 1;
#else
    return 0;
#endif
}

void save_and_switch_psp(void) {
    if (saved_psp == 0) {
        saved_psp = dos_get_psp();
        dos_set_psp(my_resident_psp);
    }
}

void restore_psp(void) {
    if (saved_psp != 0) {
        dos_set_psp(saved_psp);
        saved_psp = 0;
    }
}

void save_dta(void) {
    if (saved_dta == NULL)
        saved_dta = dos_get_dta();
}

void restore_dta(void) {
    if (saved_dta != NULL) {
        dos_set_dta(saved_dta);
        saved_dta = NULL;
    }
}

int has_output(void) {
    if (cur_pkt_out.hdr.mark == REMCTL_SERIAL_MARK)
        return 1;

    return 0;
}

static unsigned char my_dta[128];

void send_dta_find(void) {
    /* DOS FileInfoRec occupies DTA bytes 21-42 (43 total) */
    begin_output_packet(REMCTL_SERIAL_TYPE_FILE);
    cur_pkt_out.data[0] = REMCTL_SERIAL_TYPE_FILE_FIND;
    cur_pkt_out.hdr.length = 1 + (43 - 21);
    _fmemcpy((unsigned char far*)cur_pkt_out.data + 1,(unsigned char far*)my_dta + 21,43 - 21);
    end_output_packet();

    while (has_output()) {
        do_process_output();
#ifdef TARGET_PC98
        pc98_uart_irq_update();
#endif
    }
}

void do_file_find_command(void) {
    const char far *spec;
    unsigned short retv;

    cur_pkt_in.data[cur_pkt_in.hdr.length] = 0; // ASCIIZ snip
    spec = (const char far*)(cur_pkt_in.data+1);

    /* find functions use the DTA (at least the first one) */
    save_dta();

    /* use our DTA buffer for file enum */
    dos_set_dta(my_dta);

    /* start enum */
    retv = 0;
    __asm {
        push    ax
        push    cx
        push    dx
        push    ds
        mov     ah,0x4E             ; find first
        lds     dx,word ptr [spec]
        mov     cx,0x37             ; match archive+dir+sys+hidden+readonly
        int     21h
        jnc     l1
        mov     retv,ax
l1:     pop     ds
        pop     dx
        pop     cx
        pop     ax
    }

    if (retv != 0)
        return;

    send_dta_find();

    spec = (const char far*)my_dta;
    do {
        retv = 0;
        __asm {
            push    ax
            push    cx
            push    dx
            push    ds
            mov     ah,0x4F             ; find next
            lds     dx,word ptr [spec]
            int     21h
            jnc     l2
            mov     retv,ax
l2:         pop     ds
            pop     dx
            pop     cx
            pop     ax
        }

        if (retv != 0)
            return;

        send_dta_find();
    } while (1);
}

void do_file_rmdir_command(void) {
    cur_pkt_in.data[cur_pkt_in.hdr.length] = 0; // ASCIIZ snip

    {
        unsigned char far *p = (unsigned char far*)(cur_pkt_in.data + 1);
        unsigned short retv = 0;

        __asm {
            push    ds
            mov     ah,0x3A         ; rmdir
            lds     dx,word ptr [p]
            int     21h
            jnc     l1
            mov     retv,ax         ; grab error code
l1:         pop     ds
        }

        if (retv != 0) {
            cur_pkt_out.data[0] = REMCTL_SERIAL_TYPE_FILE_MSDOS_ERROR;
            cur_pkt_out.hdr.length = 1;
        }
    }
}

void do_file_mkdir_command(void) {
    cur_pkt_in.data[cur_pkt_in.hdr.length] = 0; // ASCIIZ snip

    {
        unsigned char far *p = (unsigned char far*)(cur_pkt_in.data + 1);
        unsigned short retv = 0;

        __asm {
            push    ds
            mov     ah,0x39         ; mkdir
            lds     dx,word ptr [p]
            int     21h
            jnc     l1
            mov     retv,ax         ; grab error code
l1:         pop     ds
        }

        if (retv != 0) {
            cur_pkt_out.data[0] = REMCTL_SERIAL_TYPE_FILE_MSDOS_ERROR;
            cur_pkt_out.hdr.length = 1;
        }
    }
}

void do_file_chdir_command(void) {
    cur_pkt_in.data[cur_pkt_in.hdr.length] = 0; // ASCIIZ snip

    {
        unsigned char far *p = (unsigned char far*)(cur_pkt_in.data + 1);
        unsigned short retv = 0;

        if (isalpha(p[0]) && p[1] == ':') {
            unsigned char drv = tolower(p[0]) - 'a';

            __asm {
                mov     ah,0x0E     ; set default disk
                mov     dl,drv
                int     21h
            }
        }

        __asm {
            push    ds
            mov     ah,0x3B         ; set default dos directory
            lds     dx,word ptr [p]
            int     21h
            jnc     l1
            mov     retv,ax         ; grab error code
l1:         pop     ds
        }

        if (retv != 0) {
            cur_pkt_out.data[0] = REMCTL_SERIAL_TYPE_FILE_MSDOS_ERROR;
            cur_pkt_out.hdr.length = 1;
        }
    }
}

void do_file_pwd_command(void) {
    unsigned char curdrv = 'A';

    __asm {
        mov     ah,0x19             ; get default drive
        int     21h
        add     curdrv,al
    }

    cur_pkt_out.data[1] = curdrv;
    cur_pkt_out.data[2] = ':';
    cur_pkt_out.data[3] = '\\';
    cur_pkt_out.hdr.length = 4;

    {
        unsigned char far *p;

        p = (unsigned char far*)cur_pkt_out.data + 4;

        /* NTS: we assume MS-DOS paths cannot be longer than 188 (isn't the limit something like 88?) */
        __asm {
            push    ds
            mov     ah,0x47         ; get current directory
            xor     dl,dl           ; default drive
            lds     si,word ptr [p]
            int     21h
            pop     ds
        }
    }

    while (cur_pkt_out.hdr.length < 190 && cur_pkt_out.data[cur_pkt_out.hdr.length] != 0)
        cur_pkt_out.hdr.length++;

    cur_pkt_out.data[cur_pkt_out.hdr.length] = 0;
}

void do_file_open_command(void) {
    cur_pkt_in.data[cur_pkt_in.hdr.length] = 0; // ASCIIZ snip

    close_open_file();

    /* NTS: use the extended version for it's benefits.
     *      this limits us to MS-DOS 4.0 or higher, but it's worth it */
    {
        unsigned char far *p = (unsigned char far*)(cur_pkt_in.data + 1);
        unsigned short retv = 0;
        unsigned short fd = 0;

        if (dos_version >= 0x400) {
            __asm {
                push    ax
                push    bx
                push    cx
                push    dx
                push    si
                push    ds
                mov     ah,0x6C         ; extended open/create
                mov     bx,0x2002       ; read/write, don't use INT 24h
                xor     cx,cx           ; normal file
                mov     dx,0x0001       ; open if exists, fail otherwise
                lds     si,word ptr [p]
                int     21h
                mov     fd,ax           ; grab error code/file handle
                jnc     l1
                mov     retv,ax         ; grab error code
l1:             pop     ds
                pop     si
                pop     dx
                pop     cx
                pop     bx
                pop     ax
            }
        }
        else {
            __asm {
                push    ax
                push    bx
                push    cx
                push    dx
                push    si
                push    ds
                mov     ax,0x3D02       ; open read/write
                xor     cx,cx           ; attribute to search for
                lds     dx,word ptr [p] ; DS:DX = open existing file
                int     21h
                mov     fd,ax           ; grab error code/file handle
                jnc     l1
                mov     retv,ax         ; grab error code
l1:             pop     ds
                pop     si
                pop     dx
                pop     cx
                pop     bx
                pop     ax
            }
        }

        if (retv != 0) {
            cur_pkt_out.data[0] = REMCTL_SERIAL_TYPE_FILE_MSDOS_ERROR;
            cur_pkt_out.hdr.length = 1;
        }
        else {
            open_file_fd = fd;
        }
    }
}

void do_file_create_command(void) {
    cur_pkt_in.data[cur_pkt_in.hdr.length] = 0; // ASCIIZ snip

    close_open_file();

    /* NTS: use the extended version for it's benefits.
     *      this limits us to MS-DOS 4.0 or higher, but it's worth it */
    {
        unsigned char far *p = (unsigned char far*)(cur_pkt_in.data + 1);
        unsigned short retv = 0;
        unsigned short fd = 0;

        if (dos_version >= 0x400) {
            __asm {
                push    ax
                push    bx
                push    cx
                push    dx
                push    si
                push    ds
                mov     ah,0x6C         ; extended open/create
                mov     bx,0x2002       ; read/write, don't use INT 24h
                xor     cx,cx           ; normal file
                mov     dx,0x0012       ; create file, truncate if exists
                lds     si,word ptr [p]
                int     21h
                mov     fd,ax           ; grab error code/file handle
                jnc     l1
                mov     retv,ax         ; grab error code
l1:             pop     ds
                pop     si
                pop     dx
                pop     cx
                pop     bx
                pop     ax
            }
        }
        else {
            __asm {
                push    ax
                push    bx
                push    cx
                push    dx
                push    si
                push    ds
                mov     ah,0x3C         ; create
                xor     cx,cx           ; attribute to search for
                lds     dx,word ptr [p] ; DS:DX = open existing file
                int     21h
                mov     fd,ax           ; grab error code/file handle
                jnc     l1
                mov     retv,ax         ; grab error code
l1:             pop     ds
                pop     si
                pop     dx
                pop     cx
                pop     bx
                pop     ax
            }
        }

        if (retv != 0) {
            cur_pkt_out.data[0] = REMCTL_SERIAL_TYPE_FILE_MSDOS_ERROR;
            cur_pkt_out.hdr.length = 1;
        }
        else {
            open_file_fd = fd;
        }
    }
}

void do_file_seek_command(void) {
    unsigned short fd = open_file_fd;
    unsigned short retv = 0;
    unsigned char how = cur_pkt_in.data[1];
    unsigned long poff =
        ((unsigned long)cur_pkt_in.data[2] << 0UL) +
        ((unsigned long)cur_pkt_in.data[3] << 8UL) +
        ((unsigned long)cur_pkt_in.data[4] << 16UL) +
        ((unsigned long)cur_pkt_in.data[5] << 24UL);

    if (open_file_fd < 0) {
        cur_pkt_out.data[0] = REMCTL_SERIAL_TYPE_FILE_MSDOS_ERROR;
        cur_pkt_out.hdr.length = 1;
        return;
    }

    __asm {
        push    ax
        push    bx
        push    cx
        push    dx
        mov     ah,0x42                 ; seek
        mov     al,how                  ; how to seek
        mov     bx,fd                   ; file handle
        mov     cx,word ptr poff + 2    ; CX:DX = file pointer
        mov     dx,word ptr poff
        int     21h
        jnc     l1
        mov     retv,ax
l1:     mov     word ptr poff + 2,dx  ; if not error, DX:AX is new file pointer
        mov     word ptr poff,ax
        pop     dx
        pop     cx
        pop     bx
        pop     ax
    }

    if (retv != 0) {
        cur_pkt_out.data[0] = REMCTL_SERIAL_TYPE_FILE_MSDOS_ERROR;
        cur_pkt_out.hdr.length = 1;
    }
    else {
        cur_pkt_out.data[2] = (unsigned char)(poff >> 0UL);
        cur_pkt_out.data[3] = (unsigned char)(poff >> 8UL);
        cur_pkt_out.data[4] = (unsigned char)(poff >> 16UL);
        cur_pkt_out.data[5] = (unsigned char)(poff >> 24UL);
        cur_pkt_out.hdr.length = 6;
    }
}

void do_file_read_command(void) {
    unsigned char far *p = (unsigned char far*)cur_pkt_out.data + 2;
    unsigned short length = cur_pkt_in.data[1];
    unsigned short fd = open_file_fd;
    unsigned short retv = 0;

    if (open_file_fd < 0) {
        cur_pkt_out.data[0] = REMCTL_SERIAL_TYPE_FILE_MSDOS_ERROR;
        cur_pkt_out.hdr.length = 1;
        return;
    }

    __asm {
        push    ds
        push    ax
        push    bx
        push    cx
        push    dx
        mov     ah,0x3F                 ; read
        mov     bx,fd                   ; file handle
        mov     cx,length
        lds     dx,word ptr [p]
        int     21h
        jnc     l1
        mov     retv,ax
l1:     mov     length,ax
        pop     dx
        pop     cx
        pop     bx
        pop     ax
        pop     ds
    }

    if (length > 252) length = 252;

    if (retv != 0) {
        cur_pkt_out.data[0] = REMCTL_SERIAL_TYPE_FILE_MSDOS_ERROR;
        cur_pkt_out.hdr.length = 1;
    }
    else {
        cur_pkt_out.data[1] = length;
        cur_pkt_out.hdr.length = 2 + length;
    }
}

void do_dos_stuff_bios_keyboard_command(void) {
    unsigned short head = *((unsigned short far*)MK_FP(0x40,0x1A));
    unsigned short tail = *((unsigned short far*)MK_FP(0x40,0x1C));
    unsigned char ascii = cur_pkt_in.data[1];
    unsigned char scan = cur_pkt_in.data[2];

    if (head < 0x1E || head >= 0x3E) {
        cur_pkt_out.data[0] = REMCTL_SERIAL_TYPE_FILE_MSDOS_ERROR;
        cur_pkt_out.hdr.length = 1;
        return;
    }
    if (tail < 0x1E || tail >= 0x3E) {
        cur_pkt_out.data[0] = REMCTL_SERIAL_TYPE_FILE_MSDOS_ERROR;
        cur_pkt_out.hdr.length = 1;
        return;
    }

    if (tail == 0x3E)
        tail = 0x1E;

    if (((head+2-0x1E)&0x1F) == (tail-0x1E)) {
        /* buffer full */
        cur_pkt_out.data[0] = REMCTL_SERIAL_TYPE_FILE_MSDOS_FULL;
        cur_pkt_out.hdr.length = 1;
        return;
    }

    *((unsigned char far*)MK_FP(0x40,tail)) = ascii;
    *((unsigned char far*)MK_FP(0x40,tail+1)) = scan;

    tail += 2;
    if (tail == 0x3E)
        tail = 0x1E;

    *((unsigned short far*)MK_FP(0x40,0x1C)) = tail;
}

void do_file_write_command(void) {
    unsigned char far *p = (unsigned char far*)cur_pkt_in.data + 2;
    unsigned short length = cur_pkt_in.data[1];
    unsigned short fd = open_file_fd;
    unsigned short retv = 0;

    if (open_file_fd < 0) {
        cur_pkt_out.data[0] = REMCTL_SERIAL_TYPE_FILE_MSDOS_ERROR;
        cur_pkt_out.hdr.length = 1;
        return;
    }

    if (length == 0) {
        // write with length == 0 truncates the file. that's not the user's intent.
        // don't want surprises.
        cur_pkt_out.data[1] = length;
        cur_pkt_out.hdr.length = 2 + length;
        return;
    }

    __asm {
        push    ds
        push    ax
        push    bx
        push    cx
        push    dx
        mov     ah,0x40                 ; write
        mov     bx,fd                   ; file handle
        mov     cx,length
        lds     dx,word ptr [p]
        int     21h
        jnc     l1
        mov     retv,ax
l1:     mov     length,ax
        pop     dx
        pop     cx
        pop     bx
        pop     ax
        pop     ds
    }

    if (length > 252) length = 252;

    if (retv != 0) {
        cur_pkt_out.data[0] = REMCTL_SERIAL_TYPE_FILE_MSDOS_ERROR;
        cur_pkt_out.hdr.length = 1;
    }
    else {
        cur_pkt_out.data[1] = length;
        cur_pkt_out.hdr.length = 2;
    }
}

void do_file_truncate_command(void) {
    unsigned char far *p = (unsigned char far*)cur_pkt_in.data + 2;
    unsigned short fd = open_file_fd;
    unsigned short retv = 0;

    if (open_file_fd < 0) {
        cur_pkt_out.data[0] = REMCTL_SERIAL_TYPE_FILE_MSDOS_ERROR;
        cur_pkt_out.hdr.length = 1;
        return;
    }

    __asm {
        push    ds
        push    ax
        push    bx
        push    cx
        push    dx
        mov     ah,0x40                 ; write
        mov     bx,fd                   ; file handle
        xor     cx,cx                   ; CX == 0 causes truncate instead of write
        lds     dx,word ptr [p]
        int     21h
        jnc     l1
        mov     retv,ax
l1:     pop     dx
        pop     cx
        pop     bx
        pop     ax
        pop     ds
    }

    if (retv != 0) {
        cur_pkt_out.data[0] = REMCTL_SERIAL_TYPE_FILE_MSDOS_ERROR;
        cur_pkt_out.hdr.length = 1;
    }
}

void handle_packet(void) {
    unsigned int port,data;

    switch (cur_pkt_in.hdr.type) {
        case REMCTL_SERIAL_TYPE_PING:
            begin_output_packet(REMCTL_SERIAL_TYPE_PING);
            strcpy(cur_pkt_out.data,"PING");
            cur_pkt_out.hdr.length = 4;
            end_output_packet();
            break;
        case REMCTL_SERIAL_TYPE_HALT:
            halt_system = cur_pkt_in.data[0];
            begin_output_packet(REMCTL_SERIAL_TYPE_HALT);
            cur_pkt_out.data[0] = halt_system;
            cur_pkt_out.hdr.length = 1;
            end_output_packet();
            break;
        case REMCTL_SERIAL_TYPE_DOS:
            begin_output_packet(REMCTL_SERIAL_TYPE_DOS);
            memcpy(cur_pkt_out.data,cur_pkt_in.data,8/*big enough*/);
            cur_pkt_out.hdr.length = 1;

            switch (cur_pkt_in.data[0]) {
                case REMCTL_SERIAL_TYPE_DOS_LOL:
                    cur_pkt_out.data[1] = (unsigned char)(FP_OFF(DOS_LOL) >> 0U);
                    cur_pkt_out.data[2] = (unsigned char)(FP_OFF(DOS_LOL) >> 8U);
                    cur_pkt_out.data[3] = (unsigned char)(FP_SEG(DOS_LOL) >> 0U);
                    cur_pkt_out.data[4] = (unsigned char)(FP_SEG(DOS_LOL) >> 8U);
                    cur_pkt_out.hdr.length = 5;
                    break;
                case REMCTL_SERIAL_TYPE_DOS_INDOS:
                    cur_pkt_out.data[1] = (unsigned char)(FP_OFF(InDOS_ptr) >> 0U);
                    cur_pkt_out.data[2] = (unsigned char)(FP_OFF(InDOS_ptr) >> 8U);
                    cur_pkt_out.data[3] = (unsigned char)(FP_SEG(InDOS_ptr) >> 0U);
                    cur_pkt_out.data[4] = (unsigned char)(FP_SEG(InDOS_ptr) >> 8U);
                    cur_pkt_out.hdr.length = 5;
                    break;
                case REMCTL_SERIAL_TYPE_DOS_STUFF_BIOS_KEYBOARD:
                    do_dos_stuff_bios_keyboard_command();
                    break;
            };

            end_output_packet();
            break;
        case REMCTL_SERIAL_TYPE_INPORT:
            begin_output_packet(REMCTL_SERIAL_TYPE_INPORT);
            memcpy(cur_pkt_out.data,cur_pkt_in.data,8/*big enough*/);
            cur_pkt_out.hdr.length = 3;

            port = cur_pkt_in.data[0] + (cur_pkt_in.data[1] << 8U);

            // data[2] is the I/O width
            if (cur_pkt_out.data[2] == 4) {
                // WARNING: no check is made whether your CPU is 386 or higher here!
                unsigned long ldata = inpd(port);
                cur_pkt_out.data[3] =  ldata & 0xFF;
                cur_pkt_out.data[4] = (ldata >> 8UL) & 0xFF;
                cur_pkt_out.data[5] = (ldata >> 16UL) & 0xFF;
                cur_pkt_out.data[6] = (ldata >> 24UL) & 0xFF;
                cur_pkt_out.hdr.length = 7;
            }
            else if (cur_pkt_out.data[2] == 2) {
                data = inpw(port);
                cur_pkt_out.data[3] = data & 0xFF;
                cur_pkt_out.data[4] = (data >> 8U) & 0xFF;
                cur_pkt_out.hdr.length = 5;
            }
            else {
                cur_pkt_out.data[3] = inp(port);
                cur_pkt_out.hdr.length = 4;
            }

            end_output_packet();
            break;
        case REMCTL_SERIAL_TYPE_OUTPORT:
            begin_output_packet(REMCTL_SERIAL_TYPE_OUTPORT);
            memcpy(cur_pkt_out.data,cur_pkt_in.data,8/*big enough*/);
            cur_pkt_out.hdr.length = 3;

            port = cur_pkt_in.data[0] + (cur_pkt_in.data[1] << 8U);

            // data[2] is the I/O width
            if (cur_pkt_out.data[2] == 4) {
                outpd(port,
                    (unsigned long)cur_pkt_in.data[3] +
                    ((unsigned long)cur_pkt_in.data[4] << 8UL) +
                    ((unsigned long)cur_pkt_in.data[5] << 16UL) +
                    ((unsigned long)cur_pkt_in.data[6] << 24UL));
            }
            else if (cur_pkt_out.data[2] == 2) {
                outpw(port,cur_pkt_in.data[3] + (cur_pkt_in.data[4] << 8U));
            }
            else {
                outp(port,cur_pkt_in.data[3]);
            }

            end_output_packet();
            break;
        case REMCTL_SERIAL_TYPE_MEMREAD:
            begin_output_packet(REMCTL_SERIAL_TYPE_MEMREAD);
            memcpy(cur_pkt_out.data,cur_pkt_in.data,8/*big enough*/);
            cur_pkt_out.hdr.length = 5;

            if (cur_pkt_in.data[4] != 0 && cur_pkt_in.data[4] <= 192) {
                unsigned long memaddr;

                cur_pkt_out.hdr.length = 5 + (unsigned int)cur_pkt_in.data[4];

                memaddr = ((unsigned long)cur_pkt_in.data[0]) +
                    ((unsigned long)cur_pkt_in.data[1] << 8UL) +
                    ((unsigned long)cur_pkt_in.data[2] << 16UL) +
                    ((unsigned long)cur_pkt_in.data[3] << 24UL);

#if TARGET_MSDOS == 16
                /* if any byte in the range extends past FFFF:FFFF (1MB+64KB) then use flat real mode */
                if ((memaddr+(unsigned long)cur_pkt_in.data[4]-1UL) > 0x10FFEFUL) {
                    if (cpu_basic_level >= 3 && !is_v86_mode()) {
                        if (flatrealmode_test() == 0 || flatrealmode_setup(FLATREALMODE_4GB)) {
                            for (port=0;port < (unsigned int)cur_pkt_in.data[4];port++)
                                cur_pkt_out.data[5+port] = flatrealmode_readb((uint32_t)memaddr + (uint32_t)port);
                        }
                        else {
                            memset(cur_pkt_out.data+5,'F',(unsigned int)cur_pkt_in.data[4]);
                        }
                    }
                    else {
                        memset(cur_pkt_out.data+5,'V',(unsigned int)cur_pkt_in.data[4]);
                    }
                }
                else {
                    unsigned long segv = (unsigned long)memaddr >> 4UL;
                    unsigned int ofsv = (unsigned int)(memaddr & 0xFUL);

                    if (segv > 0xFFFFUL) {
                        ofsv = memaddr - 0xFFFF0UL;
                        segv = 0xFFFFUL;
                    }

                    /* use fmemcpy using linear to segmented conversion */
                    _fmemcpy(cur_pkt_out.data+5,
                        MK_FP((unsigned int)segv,ofsv),(unsigned int)cur_pkt_in.data[4]);
                }
#endif
            }

            end_output_packet();
            break;
        case REMCTL_SERIAL_TYPE_MEMWRITE:
            begin_output_packet(REMCTL_SERIAL_TYPE_MEMWRITE);
            memcpy(cur_pkt_out.data,cur_pkt_in.data,8/*big enough*/);
            cur_pkt_out.hdr.length = 5;

            if (cur_pkt_in.data[4] != 0 && cur_pkt_in.data[4] <= 192) {
                unsigned long memaddr;

                memaddr = ((unsigned long)cur_pkt_in.data[0]) +
                    ((unsigned long)cur_pkt_in.data[1] << 8UL) +
                    ((unsigned long)cur_pkt_in.data[2] << 16UL) +
                    ((unsigned long)cur_pkt_in.data[3] << 24UL);

#if TARGET_MSDOS == 16
                /* if any byte in the range extends past FFFF:FFFF (1MB+64KB) then use flat real mode */
                if ((memaddr+(unsigned long)cur_pkt_in.data[4]-1UL) > 0x10FFEFUL) {
                    if (cpu_basic_level >= 3 && !is_v86_mode()) {
                        if (flatrealmode_test() == 0 || flatrealmode_setup(FLATREALMODE_4GB)) {
                            for (port=0;port < (unsigned int)cur_pkt_in.data[4];port++)
                                flatrealmode_writeb((uint32_t)memaddr + (uint32_t)port,cur_pkt_in.data[5+port]);
                        }
                    }
                }
                else {
                    unsigned long segv = (unsigned long)memaddr >> 4UL;
                    unsigned int ofsv = (unsigned int)(memaddr & 0xFUL);

                    if (segv > 0xFFFFUL) {
                        ofsv = memaddr - 0xFFFF0UL;
                        segv = 0xFFFFUL;
                    }

                    /* use fmemcpy using linear to segmented conversion */
                    _fmemcpy(MK_FP((unsigned int)segv,ofsv),
                        cur_pkt_in.data+5,(unsigned int)cur_pkt_in.data[4]);
                }
#endif
            }

            end_output_packet();
            break;
        case REMCTL_SERIAL_TYPE_FILE:
            begin_output_packet(REMCTL_SERIAL_TYPE_FILE);
            memcpy(cur_pkt_out.data,cur_pkt_in.data,8/*big enough*/);
            cur_pkt_out.hdr.length = 1;

            if (safe_to_use_msdos_fs_io()) {
                save_and_switch_psp();

                switch (cur_pkt_in.data[0]) {
                    case REMCTL_SERIAL_TYPE_FILE_FIND:
                        do_file_find_command();
                        begin_output_packet(REMCTL_SERIAL_TYPE_FILE);
                        cur_pkt_out.data[0] = REMCTL_SERIAL_TYPE_FILE_MSDOS_FINISHED;
                        cur_pkt_out.hdr.length = 1;
                        break;
                    case REMCTL_SERIAL_TYPE_FILE_RMDIR:
                        do_file_rmdir_command();
                        break;
                    case REMCTL_SERIAL_TYPE_FILE_MKDIR:
                        do_file_mkdir_command();
                        break;
                    case REMCTL_SERIAL_TYPE_FILE_CHDIR:
                        do_file_chdir_command();
                        break;
                    case REMCTL_SERIAL_TYPE_FILE_PWD:
                        do_file_pwd_command();
                        break;
                    case REMCTL_SERIAL_TYPE_FILE_OPEN:
                        do_file_open_command();
                        break;
                    case REMCTL_SERIAL_TYPE_FILE_CREATE:
                        do_file_create_command();
                        break;
                    case REMCTL_SERIAL_TYPE_FILE_CLOSE:
                        if (open_file_fd >= 0) {
                            close_open_file();
                        }
                        else {
                            cur_pkt_out.data[0] = REMCTL_SERIAL_TYPE_FILE_MSDOS_ERROR;
                            cur_pkt_out.hdr.length = 1;
                        }
                        break;
                    case REMCTL_SERIAL_TYPE_FILE_SEEK:
                        do_file_seek_command();
                        break;
                    case REMCTL_SERIAL_TYPE_FILE_READ:
                        do_file_read_command();
                        break;
                    case REMCTL_SERIAL_TYPE_FILE_WRITE:
                        do_file_write_command();
                        break;
                    case REMCTL_SERIAL_TYPE_FILE_TRUNCATE:
                        do_file_truncate_command();
                        break;
                    default:
                        begin_output_packet(REMCTL_SERIAL_TYPE_ERROR);
                        cur_pkt_out_seq = 0xFF;
                        break;
                };
            }
            else {
                cur_pkt_out.data[0] = REMCTL_SERIAL_TYPE_FILE_MSDOS_IS_BUSY;
                cur_pkt_out.hdr.length = 1;
            }

            end_output_packet();
            break;
        default:
            begin_output_packet(REMCTL_SERIAL_TYPE_ERROR);
            cur_pkt_out_seq = 0xFF;
            end_output_packet();
            break;
    }

    /* if we saved the PSP to switch, restore now */
    restore_psp();
    restore_dta();
}

int inpkt_validate(void) {
    unsigned char sum = cur_pkt_in.hdr.chksum;
    unsigned int i;

    // the input processor make sure the mark was read, we check the rest.
    for (i=0;i < cur_pkt_in.hdr.length;i++)
        sum += cur_pkt_in.data[i];
    if (sum != 0)
        return 0;

    // and check sequence
    if (cur_pkt_in.hdr.sequence == 0xFF)
        cur_pkt_in_seq = cur_pkt_in.hdr.sequence;
    else if (cur_pkt_in.hdr.sequence != cur_pkt_in_seq)
        return 0;

    cur_pkt_in_seq = (cur_pkt_in.hdr.sequence + 1) & 0x7F;
    return 1;
}

int process_input_packet(void) {
    /* we can't generate a new packet until the current output packet is finished sending */
    if (cur_pkt_out.hdr.mark == REMCTL_SERIAL_MARK)
        return -1;

    /* reentrancy protection: don't process another input packet if already doing one */
    if (in_packet_handling)
        return -1;

    /* send an error packet if a packet doesn't validate */
    in_packet_handling = 1;
    if (inpkt_validate()) {
        handle_packet();
    }
    else {
        begin_output_packet(REMCTL_SERIAL_TYPE_ERROR);
        cur_pkt_out_seq = 0xFF;
        end_output_packet();
    }

    in_packet_handling = 0;
    cur_pkt_in.hdr.mark = 0;
    cur_pkt_in_write = 0;
    process_output();
#ifdef TARGET_PC98
    pc98_uart_irq_update();
#endif
    return 0;
}

void process_input(void) {
    if (cur_pkt_in_write >= (sizeof(cur_pkt_in.hdr)+cur_pkt_in.hdr.length)) {
        if (process_input_packet() < 0)
            return;
    }

    do {
#ifdef TARGET_PC98
        if (!uart_8251_rxready(uart))
            break;

        ((unsigned char*)(&cur_pkt_in))[cur_pkt_in_write] = uart_8251_read(uart);
#else
        if (!uart_8250_can_read(uart))
            break;

        ((unsigned char*)(&cur_pkt_in))[cur_pkt_in_write] = uart_8250_read(uart);
#endif

        if (cur_pkt_in_write == 0 && cur_pkt_in.hdr.mark != REMCTL_SERIAL_MARK)
            continue;

        if ((++cur_pkt_in_write) >= (sizeof(cur_pkt_in.hdr)+cur_pkt_in.hdr.length)) {
            if (process_input_packet() < 0)
                break;
        }
    } while(1);
}

void process_output(void) {
    /* reentrancy protection, including against an incomplete output packet */
    if (in_packet_handling)
        return;

    do_process_output();
}

void do_process_output(void) {
    if (cur_pkt_out.hdr.mark != REMCTL_SERIAL_MARK)
        return;

#ifdef TARGET_PC98
    if (uart_8251_status(uart) & 0x38) /* if frame|overrun|parity error... */
        uart_8251_command(uart,0x17); /* error reset(4) | receive enable(2) | DTR(1) | transmit enable(0) */
#endif

    do {
#ifdef TARGET_PC98
        if (!uart_8251_txready(uart))
            break;

        uart_8251_write(uart,((unsigned char*)(&cur_pkt_out))[cur_pkt_out_write]);
#else
        if (!uart_8250_can_write(uart))
            break;

        uart_8250_write(uart,((unsigned char*)(&cur_pkt_out))[cur_pkt_out_write]);
#endif
        if ((++cur_pkt_out_write) >= (sizeof(cur_pkt_out.hdr)+cur_pkt_out.hdr.length)) {
            cur_pkt_out_write = 0;
            cur_pkt_out.hdr.mark = 0;
            break;
        }
    } while (1);
}

void do_check_io(void) {
    _cli();
    process_input();
    process_output();
#ifdef TARGET_PC98
    pc98_uart_irq_update();
#endif
    _sti();
}

#ifndef TARGET_PC98
/*--------------------------------------------------------------*/

static unsigned char devnode_raw[4096];

static void pnp_serial_scan() {
    /* most of the time the serial ports are BIOS controlled and on the motherboard.
     * they usually don't even show up in a PnP isolation scan. so we have to use
     * the "get device nodes" functions of the PnP BIOS. */
    {
        struct isa_pnp_device_node far *devn;
        unsigned int ret_ax,nodesize=0xFFFF;
        unsigned char numnodes=0xFF;
        struct isapnp_tag tag;
        unsigned char node;

        printf("Enumerating PnP system device nodes...\n");

        ret_ax = isa_pnp_bios_number_of_sysdev_nodes(&numnodes,&nodesize);
        if (ret_ax == 0 && numnodes != 0xFF && nodesize < sizeof(devnode_raw)) {
            /* NTS: How nodes are enumerated in the PnP BIOS: set node = 0, pass address of node
             *      to BIOS. BIOS, if it returns node information, will also overwrite node with
             *      the node number of the next node, or with 0xFF if this is the last one.
             *      On the last one, stop enumerating. */
            for (node=0;node != 0xFF;) {
                unsigned char far *rsc;
                int port = -1;
                int irq = -1;

                /* apparently, start with 0. call updates node to
                 * next node number, or 0xFF to signify end */
                ret_ax = isa_pnp_bios_get_sysdev_node(&node,devnode_raw,
                        ISA_PNP_BIOS_GET_SYSDEV_NODE_CTRL_NOW);

                if (ret_ax != 0)
                    break;

                devn = (struct isa_pnp_device_node far*)devnode_raw;
                if (!is_rs232_or_compat_pnp_device(devn))
                    continue;

                /* there are three config blocks, one after the other.
                 *  [allocated]
                 *  [possible]
                 *  [??]
                 * since we're not a configuration utility, we only care about the first one */
                rsc = devnode_raw + sizeof(*devn);
                if (isapnp_read_tag(&rsc,devnode_raw + devn->size,&tag)) {
                    do {
                        if (tag.tag == ISAPNP_TAG_END) /* end tag */
                            break;

                        switch (tag.tag) {
/*---------------------------------------------------------------------------------*/
case ISAPNP_TAG_IRQ_FORMAT: {
    struct isapnp_tag_irq_format far *x = (struct isapnp_tag_irq_format far*)tag.data;
    unsigned int i;
    for (i=0;i < 16 && irq < 0;i++) {
        if (x->irq_mask & (1U << (unsigned int)i))
            irq = i;
    }
} break;
case ISAPNP_TAG_IO_PORT: {
    struct isapnp_tag_io_port far *x = (struct isapnp_tag_io_port far*)tag.data;
    if (x->length >= 8 && port < 0) port = x->min_range;
} break;
case ISAPNP_TAG_FIXED_IO_PORT: {
    struct isapnp_tag_fixed_io_port far *x = (struct isapnp_tag_fixed_io_port far*)tag.data;
    if (x->length >= 8 && port < 0) port = x->base;
} break;
/*---------------------------------------------------------------------------------*/
                        };
                    } while (isapnp_read_tag(&rsc,devnode_raw + devn->size,&tag));
                }

                if (port < 0)
                    continue;

                if (add_pnp_8250(port,irq))
                    printf("Found PnP port @ 0x%03x IRQ %d\n",port,irq);
            }
        }
    }
}
#endif

void tsr_exit(void) {
    unsigned short resident_size = 0;

    /* auto-detect the size of this EXE by the MCB preceeding the PSP segment */
    /* do it BEFORE hooking in case shit goes wrong, then we can safely abort. */
    /* the purpose of this code is to compensate for Watcom C's lack of useful */
    /* info at runtime or compile time as to how large we are in memory. */
    {
        unsigned short env_seg=0;
        unsigned short psp_seg=0;
        unsigned char far *mcb;

        psp_seg = my_resident_psp;

        mcb = MK_FP(psp_seg-1,0);

        /* sanity check */
        if (!(*mcb == 'M' || *mcb == 'Z')/*looks like MCB*/ ||
            *((unsigned short far*)(mcb+1)) != psp_seg/*it's MY memory block*/) {
            printf("Can't figure out my resident size, aborting\n");
            abort();
        }

        resident_size = *((unsigned short far*)(mcb+3)); /* number of paragraphs */
        if (resident_size < 17) {
            printf("Resident size is too small, aborting\n");
            abort();
        }

        /* while we're at it, free our environment block as well, we don't need it */
        env_seg = *((unsigned short far*)MK_FP(psp_seg,0x2C));
        if (env_seg != 0) {
            if (_dos_freemem(env_seg) == 0) {
                *((unsigned short far*)MK_FP(psp_seg,0x2C)) = 0;
            }
            else {
                printf("WARNING: Unable to free environment block\n");
            }
        }
    }

    printf("Exiting to TSR\n");
    _dos_keep(0,resident_size);
}

static unsigned int do_exit = 0;

void mainloop(void) {
    printf("Ready. Type shift+S to exit to DOS to run in the background.\n");

    while (!do_exit) {
        if (kbhit()) {
            int c = getch();

            if (c == 27) {
                do_exit = 1;
            }
            else if (c == ' ') {
                // in case interrupts get wedged, or we're using the UART in a non-IRQ mode, check manually.
                // user can trigger this with spacebar.
                do_check_io();
            }
            else if (c == 'S') { // shift+S
                if (!use_interrupts || uart->irq == -1) {
                    printf("* Background (TSR) mode requires use of UART interrupts\n");
                }
                else {
                    tsr_exit();
                }
            }
        }

        // we have to poll here if not using interrupts
        if (!use_interrupts || uart->irq == -1)
            do_check_io();
    }
}

void help(void) {
    fprintf(stderr,"REMSRV [options]\n");
    fprintf(stderr,"Server-side end of remote control for DOS\n");
    fprintf(stderr,"  -noint                 Don't use UART interrupts\n");
    fprintf(stderr,"  -int                   Use UART interrupts (default)\n");
    fprintf(stderr,"  -baud <n>              Set BAUD rate\n");
    fprintf(stderr,"  -s <n>                 Stop bits (1 or 2)\n");
    fprintf(stderr,"  -ic <n>                Software delay during INT 28h\n");
    fprintf(stderr,"                         (for slow machines use 8000-16000)\n");
    fprintf(stderr,"  -p <n>                 COM port (1 to 4)\n");
    fprintf(stderr,"  -tsr                   Immediately exit as a TSR\n");
}

int parse_argv(int argc,char **argv) {
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
            else if (!strcmp(a,"tsr")) {
                tsr_init = 1;
            }
            else if (!strcmp(a,"p")) {
                a = argv[i++];
                if (a == NULL) return 1;
                user_sel_port = strtoul(a,NULL,0);
            }
            else if (!strcmp(a,"ic")) {
                a = argv[i++];
                if (a == NULL) return 1;
                ic_delay_counter = strtoul(a,NULL,0);
            }
            else if (!strcmp(a,"noint")) {
                use_interrupts = 0;
            }
            else if (!strcmp(a,"int")) {
                use_interrupts = 1;
            }
            else if (!strcmp(a,"s")) {
                a = argv[i++];
                if (a == NULL) return 1;
                stop_bits = strtoul(a,NULL,0);
                if (stop_bits < 1) stop_bits = 1;
                else if (stop_bits > 2) stop_bits = 2;
            }
            else if (!strcmp(a,"baud")) {
                a = argv[i++];
                if (a == NULL) return 1;
                baud_rate = strtoul(a,NULL,0);
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

    return 0;
}

int main(int argc,char **argv) {
    if (parse_argv(argc,argv))
        return 1;

	cpu_probe();
	probe_dos();
    detect_windows();

    memset(&cur_pkt_in,0,sizeof(cur_pkt_in));
    memset(&cur_pkt_out,0,sizeof(cur_pkt_out));

    if (windows_mode > WINDOWS_REAL) {
        fprintf(stderr,"Do not use this program under Windows!\n");
        return 1;
    }

    if (!probe_8254()) {
        printf("8254 not found (I need this for time-sensitive portions of the driver)\n");
        return 1;
    }

    if (!probe_8259()) {
        printf("8259 not found (I need this for portions of the test involving serial interrupts)\n");
        return 1;
    }

#ifdef TARGET_PC98
    if (!init_8251()) {
        printf("Cannot init 8251 library\n");
        return 1;
    }
#else
    if (!init_8250()) {
        printf("Cannot init 8250 library\n");
        return 1;
    }
#endif

    /* get my PSP segment */
    my_resident_psp = dos_get_psp();

    /* get the InDOS flag */
    {
        unsigned short so=0,oo=0;

        __asm {
            mov     ah,34h
            int     21h
            mov     so,es
            mov     oo,bx
        }

        InDOS_ptr = MK_FP(so,oo);
    }

    /* get the List of Lists */
    if ((DOS_LOL=dos_list_of_lists()) == NULL) {
        printf("Unable to obtain List of Lists\n");
        return 1;
    }

    /* this program does some serious hackery with MS-DOS, warn the user that system stability
     * is not guaranteed. */
    printf("CAUTION: This program allows remote control of the DOS machine in a way useful\n");
    printf("         for debugging, but slightly dangerous for system stability. This\n");
    printf("         program allows background filesystem I/O in a way that may conflict\n");
    printf("         with low level filesystem tools, disk caching drivers, and disk\n");
    printf("         compression drivers. It is *STRONGLY ADVISED* that you do not run this\n");
    printf("         program on any system who's filesystem you care about.\n");
    printf("\n");
    printf("         USE THIS PROGRAM AT YOUR OWN RISK. Read the README for more info.\n");
    printf("\n");

    printf("DOS ver: %u.%u\n",dos_version>>8,dos_version&0xFF);
    printf("InDOS: %04x:%04x\n",FP_SEG(InDOS_ptr),FP_OFF(InDOS_ptr));
    printf("LOL: %04x:%04x\n",FP_SEG(DOS_LOL),FP_OFF(DOS_LOL));
    printf("PSP: %04x\n",my_resident_psp);

#ifdef TARGET_PC98
    probe_common_8251();

    {
        unsigned int i;
        int choice;

        if (user_sel_port <= 0)
            printf("Which serial port should I use?\n");

        for (i=0;i < uart_8251_total();i++) {
            struct uart_8251 *c_uart = uart_8251_get(i);
            if (c_uart == NULL) continue;

            printf("  %u: UART at 0x%02X,0x%02X '%s'\n",
                    i+1,uart_8251_portidx(c_uart->base_io,0),
                    uart_8251_portidx(c_uart->base_io,1),
                    c_uart->description ? c_uart->description : "");
        }

        if (user_sel_port <= 0) {
            printf("Choice? "); fflush(stdout);
            choice = -1;
            scanf("%d",&choice);
        }
        else {
            choice = user_sel_port;
        }
        choice--;
        if (choice < 0 || choice >= uart_8251_total()) return 0;

        uart = uart_8251_get(choice);
        if (uart == NULL) return 0;

        if (uart->base_io == 0x41) return 0; /* do not allow keyboard port */
    }
#else
    probe_8250_bios_ports();

    {
        unsigned int i;

        for (i=0;!base_8250_full() && i < (int)bios_8250_ports;i++) {
            const uint16_t port = get_8250_bios_port(i);
            if (port == 0) continue;
            probe_8250(port);
        }
    }

    if (init_isa_pnp_bios() && find_isa_pnp_bios())
        pnp_serial_scan();

    {
        unsigned int i;
        int choice;

        if (user_sel_port <= 0)
            printf("Which serial port should I use?\n");

        for (i=0;i < base_8250_ports;i++) {
            struct info_8250 *inf = &info_8250_port[i];
            printf("[%u] @ %03X (type %s IRQ %d)\n",i+1,inf->port,type_8250_to_str(inf->type),inf->irq);
        }

        if (user_sel_port <= 0) {
            printf("Choice? "); fflush(stdout);
            choice = -1;
            scanf("%d",&choice);
        }
        else {
            choice = user_sel_port;
        }
        choice--;
        if (choice < 0 || choice >= base_8250_ports) return 0;
        uart = &info_8250_port[choice];
    }
#endif

    old_int28 = _dos_getvect(0x28);
    _dos_setvect(0x28,my_int28);

    if (uart->irq != -1 && use_interrupts) {
        printf("Using UART interrupt mode\n");

        old_timer_irq = _dos_getvect(irq2int(0));
        _dos_setvect(irq2int(0),timer_irq);

        old_irq = _dos_getvect(irq2int(uart->irq));
        _dos_setvect(irq2int(uart->irq),uart_irq);
        if (uart->irq >= 0) {
            p8259_unmask(uart->irq);
            if (uart->irq >= 8) p8259_OCW2(8,P8259_OCW2_SPECIFIC_EOI | (uart->irq&7));
            p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | (uart->irq&7));
        }
    }

#ifdef TARGET_PC98
    _cli();
    outp(0x35,(inp(0x35) & (~7))); /* mask all interrupt lines for a bit while we work here */
    {
        unsigned char bits = 8;
        unsigned char b;

        b  = (bits - 5) << 2; /* bits [3:2] = character length (0=5 .. 3=8) */
        b |= ((stop_bits == 2U) ? 3U : 1U) << 6U;
        b |= (2 << 0); /* bits [1:0] = 16X baud rate factor (baud rate is 1/16th the clock rate) */
        b |= 0x10;  /* bits [5:4] = 01 = odd parity, enable  (bit 4 is parity enable) */

        uart_8251_reset(uart,b,0,0);
        uart_8251_command(uart,0x17); /* error reset(4) | receive enable(2) | DTR(1) | transmit enable(0) */

        {
            unsigned long clk = T8254_REF_CLOCK_HZ / (baud_rate * 16UL/*baud rate times 16*/);
            write_8254(T8254_TIMER_RS232,clk,T8254_MODE_3_SQUARE_WAVE_MODE);
        }
    }
    pc98_uart_irq_update();
    _sti();
#else
    uart_8250_enable_interrupt(uart,0); /* disable interrupts (set IER=0) */
    uart_8250_disable_FIFO(uart);
    uart_8250_set_MCR(uart,3);      /* turn on RTS and DTS */
    uart_8250_set_line_control(uart,UART_8250_LCR_8BIT | UART_8250_LCR_PARITY | (stop_bits == 2 ? UART_8250_LCR_TWO_STOP_BITS : 0)); /* 8 bit 1 stop bit odd parity */
    uart_8250_set_baudrate(uart,uart_8250_baud_to_divisor(uart,baud_rate));

    // enable FIFO
    if (uart->type > TYPE_8250_IS_16550) {
        uart_8250_set_FIFO(uart,0); // disable/flush
        uart_8250_set_FIFO(uart,1); // enable
        uart_8250_set_FIFO(uart,7); // flush in/out
        uart_8250_set_FIFO(uart,1); // enable

        if (uart->type >= TYPE_8250_IS_16750) {
            printf("Enabling FIFO (64-byte)\n");
            uart_8250_set_FIFO(uart,0xA1); // enable (bit 0), 64-byte (bit 5), threshhold is half the buffer (bits 6-7)
        }
        else {
            printf("Enabling FIFO (16-byte)\n");
            uart_8250_set_FIFO(uart,0x81); // enable (bit 0), 16-byte (bit 5), threshhold is half the buffer (bits 6-7)
        }
    }

    if (uart->irq != -1 && use_interrupts) {
        // make sure PIC has acked any pending interrupts to that line
        if (uart->irq >= 8) p8259_OCW2(8,P8259_OCW2_SPECIFIC_EOI | (uart->irq&7));
        p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | (uart->irq&7));

        // enable interrupts. must be done HERE, not after force flushing
        uart_8250_enable_interrupt(uart,0xF);

        // force flush pending interrupts
        {
            unsigned int i;

            for (i=0;i < 256 && (inp(uart->port+PORT_8250_IIR) & 1) == 0;i++) {
                inp(uart->port);
                inp(uart->port+PORT_8250_MSR);
                inp(uart->port+PORT_8250_MCR);
                inp(uart->port+PORT_8250_LSR);
                inp(uart->port+PORT_8250_IIR);
                inp(uart->port+PORT_8250_IER);
            }
        }
    }
#endif

    if (tsr_init)
        tsr_exit(); // does not return

    // main loop
    mainloop();

#ifdef TARGET_PC98
    pc98_uart_irq_update();
    outp(0x35,(inp(0x35) & (~7)));
#else
    // okay, shutdown
    uart_8250_enable_interrupt(uart,0); /* disable interrupts (set IER=0) */
    uart_8250_set_MCR(uart,0);      /* RTS/DTR and aux lines off */
    uart_8250_disable_FIFO(uart);
    uart_8250_set_line_control(uart,UART_8250_LCR_8BIT | UART_8250_LCR_PARITY); /* 8 bit 1 stop bit odd parity */
    uart_8250_set_baudrate(uart,uart_8250_baud_to_divisor(uart,9600));
#endif

    if (uart->irq != -1 && use_interrupts) {
        _dos_setvect(irq2int(uart->irq),old_irq);
        if (uart->irq >= 0) p8259_mask(uart->irq);

        _dos_setvect(irq2int(0),old_timer_irq);
    }

    _dos_setvect(0x28,old_int28);
    return 0;
}

