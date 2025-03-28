
#if !defined(TARGET_PC98)

#include <hw/dos/dos.h>
#include <hw/8250/8250.h>
#include <hw/dos/doswin.h>

/* ISA PnP version. The program calling us does the work of scanning BIOS device nodes and ISA PnP device isolation,
 * then presents us with the IRQ and port number. We take the caller's word for it. If for any reason the caller
 * did not find the IRQ, it should pass irq == -1 */
int add_pnp_8250(uint16_t port,int irq) {
    unsigned char ier,dlab1,dlab2,c,fcr;
    struct info_8250 *inf;

    if (already_got_8250_port(port))
        return 0;
    if (base_8250_full())
        return 0;

    inf = &info_8250_port[base_8250_ports];
    inf->type = TYPE_8250_IS_8250;
    inf->port = port;
    inf->irq = irq;
    if (windows_mode == WINDOWS_NONE || windows_mode == WINDOWS_REAL) {
        /* in real-mode DOS we can play with the UART to our heart's content. so we play with the
         * DLAB select and interrupt enable registers to detect the UART in a manner non-destructive
         * to the hardware state. */

        /* switch registers 0+1 back to RX/TX and interrupt enable, and then test the Interrupt Enable register */
        _cli();
        outp(port+3,inp(port+3) & 0x7F);
        if (inp(port+3) == 0xFF) { _sti(); return 0; }
        ier = inp(port+1);
        outp(port+1,0);
        if (inp(port+1) == 0xFF) { _sti(); return 0; }
        outp(port+1,ier);
        if ((inp(port+1) & 0xF) != (ier & 0xF)) { _sti(); return 0; }
        /* then switch 0+1 to DLAB (divisor registers) and see if values differ from what we read the first time */
        outp(port+3,inp(port+3) | 0x80);
        dlab1 = inp(port+0);
        dlab2 = inp(port+1);
        outp(port+0,ier ^ 0xAA);
        outp(port+1,ier ^ 0x55);
        if (inp(port+1) == ier || inp(port+0) != (ier ^ 0xAA) || inp(port+1) != (ier ^ 0x55)) {
            outp(port+0,dlab1);
            outp(port+1,dlab2);
            outp(port+3,inp(port+3) & 0x7F);
            _sti();
            return 0;
        }
        outp(port+0,dlab1);
        outp(port+1,dlab2);
        outp(port+3,inp(port+3) & 0x7F);

        /* now figure out what type */
        fcr = inp(port+2);
        outp(port+2,0xE7);  /* write FCR */
        c = inp(port+2);    /* read IIR */
        if (c & 0x40) { /* if FIFO */
            if (c & 0x80) {
                if (c & 0x20) inf->type = TYPE_8250_IS_16750;
                else inf->type = TYPE_8250_IS_16550A;
            }
            else {
                inf->type = TYPE_8250_IS_16550;
            }
        }
        else {
            unsigned char oscratch = inp(port+7);

            /* no FIFO. try the scratch register */
            outp(port+7,0x55);
            if (inp(port+7) == 0x55) {
                outp(port+7,0xAA);
                if (inp(port+7) == 0xAA) {
                    outp(port+7,0x00);
                    if (inp(port+7) == 0x00) {
                        inf->type = TYPE_8250_IS_16450;
                    }
                }
            }

            outp(port+7,oscratch);
        }

        outp(port+2,fcr);
        _sti();
    }
    else {
        unsigned int i;

        /* if we were to actually do our self-test in a VM, Windows would mistakingly assume we
         * were trying to use it and would allocate the port. we're just enumerating at this point.
         * play it safe and assume it works if the port is listed as one of the BIOS ports.
         * we also don't use interrupts. */
        for (i=0;i < bios_8250_ports && port != get_8250_bios_port(i);) i++;
        if (i >= bios_8250_ports) return 0;
    }

    base_8250_port[base_8250_ports++] = port;
    return 1;
}

#endif //!defined(TARGET_PC98)

/* vim: set tabstop=4 softtabstop=4 shiftwidth=4 expandtab */
