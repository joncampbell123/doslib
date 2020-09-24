
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <malloc.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/emm.h>
#include <hw/dos/himemsys.h>
#include <hw/vga/vga.h>
#include <hw/vga/vrl.h>
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>
#include <fmt/minipng/minipng.h>

#include "timer.h"
#include "vmode.h"
#include "fonts.h"
#include "vrlimg.h"
#include "freein.h"
#include "dbgheap.h"
#include "fontbmp.h"
#include "unicode.h"
#include "commtmp.h"
#include "sin2048.h"
#include "vrldraw.h"
#include "seqcomm.h"
#include "keyboard.h"
#include "dumbpack.h"
#include "fzlibdec.h"
#include "fataexit.h"
#include "sorcpack.h"
#include "rotozoom.h"
#include "seqcanvs.h"
#include "cutscene.h"

#include <hw/8042/8042.h>

/* keyboard (IRQ1) handler, IF the game needs it at that time.
 * The hook will be installed and uninstalled as needed.
 * If the hook is installed, you must use the key down map to detect keys.
 * If the hook is not installed, you must use INT 16h, or DOS input functions like getch() and kbhit().
 * Do not use getch and kbhit with the hook installed, they won't pick up anything. */

static uint16_t kbd_buf[KBD_BUF_SIZE];
static unsigned char kbd_buf_head,kbd_buf_tail;/*write to the head, read from the tail*/

static void (__interrupt __far *prev_kbirq)() = NULL;

/* XT/AT keyboard input.
 * Most scan codes are single byte, with bit 7 set to indicate break.
 * Some scan codes are 0xE0 <xx> with bit 7 of <xx> the same.
 * The pause key has a make code and no break code. Code is 0xE1 0x1D 0x45 0xE1 0x9D 0xC5 */
static unsigned char kbseqbuf[3];
static unsigned char kbseqbufp/*pos*/,kbseqbufm/*length*/;

/* 0x00-0x7F: Standard single byte keys
 * 0x80-0xFF: 0xE0 <xx> double byte keys
 * 0x100:     Pause key was hit (no way to detect if still held)
 * 0x101:     Unknown codes */
unsigned char kbdown[KBDOWN_BYTES]; /* bitfield */
unsigned char kbd_flags = 0;

/* do not call with interrupts enabled and the IRQ handler installed */
static void kbd_buf_add(const unsigned int k) {
    const unsigned char nh = (kbd_buf_head+1u)&KBD_BUF_MASK;
    if (nh != kbd_buf_tail) {/*write unless it would overrun the circular buffer*/
        kbd_buf[kbd_buf_head] = k;
        kbd_buf_head = nh;
    }
}

static void kbdbuf_reset() {
    memset(kbdown,0,sizeof(kbdown));
    kbd_buf_tail = 0;
    kbd_buf_head = 0;
    kbseqbufp = 0;
    kbseqbufm = 1;
}

static inline void kbirq_on_input_code(/*in kbseqbuf*/) {
    unsigned int k = KBDOWN_MAXBITS - 1u;/*unknown*/

    switch (kbseqbufm) {
        case 1:     k = kbseqbuf[0] & 0x7Fu; break;                 /* <xx> */
        case 2:     k = (kbseqbuf[1] & 0x7Fu) + 0x80u; break;       /* 0xE0 <xx> */
        case 3: /* 0xE1 <xx> <xx> 0xE1 <xx> <xx> */
            if ((kbseqbuf[1] & 0x7Fu) == 0x1D && (kbseqbuf[2] & 0x7Fu) == 0x45)
                k = 0x100; // pause key
            break;
    };

    /* keep it simple, go by the last byte for make/break */
    if (kbseqbuf[kbseqbufm-1u] & 0x80) {/*break*/
        kbdown_clear(k);
    }
    else {/*make*/
        kbdown_set(k);
        if (k != (KBDOWN_MAXBITS - 1u)) kbd_buf_add(k);
    }

    kbseqbufp = 0;
    kbseqbufm = 1;
}

static inline void kbirq_handle_input(const unsigned char c) {
    if (kbseqbufp == 0) {
        if (c == 0xE0) /* IBM's scheme here to escape extended codes seems to be: 0xE0 <xx> */
            kbseqbufm = 2;
        else if (c == 0xE1) /* ...and 0xE1 <xx> <xx> */
            kbseqbufm = 3;
    }

    kbseqbuf[kbseqbufp] = c;
    if ((++kbseqbufp) == kbseqbufm)
        kbirq_on_input_code();
}

/* TODO: I know of at least one 386 (486?) motherboard in my collection who's "output buffer full" status bit
 *       takes a significantly long time to clear after reading the output buffer. Long enough that a loop
 *       like this would falsely read many repeating characters. Of course this is also the same board where
 *       if you send the command to hit the CPU's reset line it takes long enough for the CPU to execute a good
 *       1000-2000 instructions before the reset signal finally comes! */
static void __interrupt __far kbirq_at() { /* AT keyboard handler (ones that are sane) */
    /* NTS: 0x60 = input (if read) / output (if write)
     *      0x64 = status (if read) / command (if write)
     *
     *      0x64 input status
     *        bit 0: 1=output buffer full, meaning the controller has data ready for you to read from 0x60. Output FROM the controller.
     *        bit 1: 1=input buffer full, meaning that data was written to port 0x60 and the controller hasn't processed it yet. Input TO the controller.
     *        bit 2: reflects the "system flag" in the controller's command byte
     *        bit 3: 1=the last byte written was to port 0x64  0=the last byte written was to port 0x60
     *        bit 4: 0=keyboard inhibited by lock
     *        bit 5: 1=transmit time-out
     *        bit 6: 1=receive time-out
     *        bit 7: 1=parity error
     *
     *      Note that this code assumes the system has set the "convert scan codes" bit in the command byte,
     *      meaning that the AT keyboard scan codes are converted to codes that PC/XT programs expect. That's
     *      normally the case in the DOS world, anyway, and it allows our input handler to possibly support
     *      XT keyboards as well. This is where the standard is messy for backwards compat: In a standard AT
     *      system the keyboard is sending scan code set 2, and the 8042 is converting it to the XT compatible
     *      scan code set 1. So all we have to worry about is scan code set 1. Got it? :)
     *
     *      If it turns out modern systems violate that assumption and leave the scan codes untranslated for
     *      any reason, then this game is going to have to do some 8042 programming itself.
     */

    /* any data? */
    while (inp(K8042_STATUS) & 0x01/*bit 0*/)
        kbirq_handle_input(inp(K8042_DATA));

    p8259_OCW2(1,P8259_OCW2_SPECIFIC_EOI | 1);
}

int kbd_buf_read(void) {
    int r = -1;

    SAVE_CPUFLAGS( _cli() ) {
        if (kbd_buf_tail != kbd_buf_head) {
            r = (int)kbd_buf[kbd_buf_tail];
            kbd_buf_tail = (kbd_buf_tail+1u)&KBD_BUF_MASK;
        }
    } RESTORE_CPUFLAGS();

    return r;
}

/* detect keyboard (TODO) */
void detect_keyboard() {
    /* TODO: Can we detect XT vs AT keyboard? */
    /* TODO: Can we detect 8042s like the one on that old 386 of mine that's REALLY SLOW and has slow unreliable status bits after read? */
    /* NTS: This will involve probing ports 60h-64h to detect XT/AT, sending 8042 commands to check things, etc. */
}

/* init keyboard IRQ */
void init_keyboard_irq() {
    if (prev_kbirq == NULL) {
        p8259_mask(1);
        kbdbuf_reset();
        inp(K8042_STATUS);
        inp(K8042_DATA);// flush the 8042
        p8259_OCW2(1,P8259_OCW2_SPECIFIC_EOI | 1);
        prev_kbirq = _dos_getvect(irq2int(1));
        _dos_setvect(irq2int(1),kbirq_at);
        p8259_unmask(1);

        // drain keyboard buffer
        while (kbhit()) getch();

        // alter BIOS keyboard status in case user is holding modifier (shift/ctrl/alt) keys [https://www.matrix-bios.nl/system/bda.html]
        *((unsigned char far*)MK_FP(0x40,0x17)) &= ~0x0F; // clear alt/ctrl/lshift/rshift down status
        *((unsigned char far*)MK_FP(0x40,0x18))  =  0x00; // clear other key down status
    }
}

/* restore keyboard IRQ */
void restore_keyboard_irq() {
    if (prev_kbirq != NULL) {
        p8259_mask(1);
        inp(K8042_STATUS);
        inp(K8042_DATA);// flush the 8042
        p8259_OCW2(1,P8259_OCW2_SPECIFIC_EOI | 1);
        _dos_setvect(irq2int(1),prev_kbirq);
        prev_kbirq = NULL;
        p8259_unmask(1);
    }
}

