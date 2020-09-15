
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
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
#include "vrlimg.h"
#include "dbgheap.h"
#include "unicode.h"
#include "commtmp.h"
#include "keyboard.h"
#include "fzlibdec.h"
#include "fataexit.h"

/*---------------------------------------------------------------------------*/
/* unhook IRQ call                                                           */
/*---------------------------------------------------------------------------*/

void unhook_irqs() {
    restore_timer_irq();
    restore_keyboard_irq();
}

/*---------------------------------------------------------------------------*/
/* fatal abort                                                               */
/*---------------------------------------------------------------------------*/

void fatal(const char *msg,...) {
    va_list va;

    unhook_irqs();
    restore_text_mode();

    dbg_heap_list();

    printf("FATAL ERROR: ");

    va_start(va,msg);
    vprintf(msg,va);
    va_end(va);
    printf("\n");

    exit(1);
}

