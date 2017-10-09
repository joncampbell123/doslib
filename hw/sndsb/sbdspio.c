
#include <hw/sndsb/sndsb.h>

/* this MUST follow conio.h */
#define DOSLIB_REDEFINE_INP
#include <hw/cpu/liteio.h>

/* uncomment this to enable debugging messages */
//#define DBG

#if defined(DBG)
# define DEBUG(x) (x)
#else
# define DEBUG(x)
#endif

int sndsb_read_dsp(struct sndsb_ctx * const cx) {
    const unsigned int status_port = cx->dspio+SNDSB_BIO_DSP_READ_STATUS;
    const unsigned int read_data_port = cx->dspio+SNDSB_BIO_DSP_READ_DATA;
    register unsigned char c; /* encourage C/C++ optimizer to convert c = inp() ... return c to just return inp() */

    /* check and test in the negative, not the affirmative,
     * to encourage Open Watcom C to generate something like:
     *
     * L$1:
     * in al,dx
     * test al,80h
     * jnz code_to_read_data_port
     * dec patience
     * jnz L$1
     *
     * Open Watcom does not have GCC's likely/unlikely macros,
     * therefore the only way to encourage this is to write the
     * if() statement to test for what we think is most likely
     * to happen. */

    {
#if TARGET_MSDOS == 32
        register unsigned int patience = (unsigned int)(SNDSB_IO_COUNTDOWN);

        do {
            if (!(inp(status_port) & 0x80)) {/* data NOT available? */
                if (--patience == 0U)
                    break;
            }
            else {
                c = inp(read_data_port);
                DEBUG(fprintf(stdout,"sndsb_read_dsp() == 0x%02X\n",c));
                return c;
            }
        } while (1);
#else
        register unsigned int pa_hi = SNDSB_IO_COUNTDOWN_16HI;

        do {
            register unsigned int pa_lo = SNDSB_IO_COUNTDOWN_16LO;

            do {
                if (!(inp(status_port) & 0x80)) {/* data NOT available? */
                    if (--pa_lo == 0U)
                        break; /* from inner while loop, to check and loop while --pa_hi != 0U */
                }
                else {
                    c = inp(read_data_port);
                    DEBUG(fprintf(stdout,"sndsb_read_dsp() == 0x%02X\n",c));
                    return c;
                }
            } while (1);
        } while (--pa_hi != 0U);
#endif
    }

    DEBUG(fprintf(stdout,"sndsb_read_dsp() read timeout\n"));
    return -1;
}

int sndsb_write_dsp(struct sndsb_ctx * const cx,const uint8_t d) {
    const unsigned int status_port = cx->dspio+SNDSB_BIO_DSP_WRITE_STATUS;
    const unsigned int write_data_port = cx->dspio+SNDSB_BIO_DSP_WRITE_DATA;

    DEBUG(fprintf(stdout,"sndsb_write_dsp(0x%02X)\n",d));

    /* check and test in the negative, not the affirmative,
     * to encourage Open Watcom C to generate something like:
     *
     * L$1:
     * in al,dx
     * test al,80h
     * jnz code_to_write_data_port
     * dec patience
     * jnz L$1
     *
     * Open Watcom does not have GCC's likely/unlikely macros,
     * therefore the only way to encourage this is to write the
     * if() statement to test for what we think is most likely
     * to happen. */

    {
#if TARGET_MSDOS == 32
        register unsigned int patience = (unsigned int)(SNDSB_IO_COUNTDOWN);

        do {
            if (inp(status_port) & 0x80) {/* DSP busy? */
                if (--patience == 0U)
                    break;
            }
            else {
                outp(write_data_port,d);
                return 1;
            }
        } while (1);
#else
        register unsigned int pa_hi = SNDSB_IO_COUNTDOWN_16HI;

        do {
            register unsigned int pa_lo = SNDSB_IO_COUNTDOWN_16LO;

            do {
                if (inp(status_port) & 0x80) {/* DSP busy? */
                    if (--pa_lo == 0U)
                        break;
                }
                else {
                    outp(write_data_port,d);
                    return 1;
                }
            } while (1);
        } while (--pa_hi != 0U);
#endif
    }

    DEBUG(fprintf(stdout,"sndsb_write_dsp() timeout i=%02X p=%03X\n",inp(status_port),status_port));
    return 0;
}

/*
 * vim: set tabstop=4
 * vim: set softtabstop=4
 * vim: set shiftwidth=4
 * vim: set expandtab
 */
