
#include <windows.h>
#include <stdlib.h>
#include <stdint.h>
#include <conio.h>

#include <hw/cpu/cpu.h>
#include <hw/dosboxid/iglib.h>

#include "mouse.h"

#pragma pack(push,1)
typedef struct MOUSEINFO {
    uint8_t         msExist;
    uint8_t         msRelative;
    uint16_t        msNumButtons;
    uint16_t        msRate;
    uint16_t        msXThreshold;
    uint16_t        msYThreshold;
    uint16_t        msXRes;
    uint16_t        msYRes;
} MOUSEINFO;
typedef MOUSEINFO* PMOUSEINFO;
typedef MOUSEINFO FAR* LPMOUSEINFO;
#pragma pack(pop)

static uint16_t IntCS = 0; // DS alias of CS
static MOUSEINFO my_mouseinfo = {0};

extern const void far *AssignedEventProc;

void far *old_interrupt_handler;
extern void far interrupt_handler();

WORD PASCAL __loadds Inquire(LPMOUSEINFO mouseinfo) {
    *mouseinfo = my_mouseinfo;
    return sizeof(my_mouseinfo);
}

int WINAPI __loadds MouseGetIntVect(void) {
    return -1;
}

void WINAPI __loadds Enable(const void far * const EventProc) {
    if (EventProc == NULL)
        return;

    if (!AssignedEventProc) {
        _cli();

        {
            uint16_t vds = 0;

            __asm   mov vds,seg interrupt_handler

            IntCS = AllocDStoCSAlias(vds);
        }

        // replace interrupt vector, save old
        __asm {
            mov     ah,0x35         ; get interrupt vector
            mov     al,(0x08+13)    ; IRQ13
            int     21h
            mov     word ptr old_interrupt_handler+0,bx
            mov     word ptr old_interrupt_handler+2,es

            push    ds
            mov     ah,0x25         ; set interrupt vector
            mov     al,(0x08+13)    ; IRQ13
            mov     dx,IntCS        ; cannot use CS, protected mode will not allow it
            mov     ds,dx
            mov     dx,offset interrupt_handler
            int     21h
            pop     ds
        }

        AssignedEventProc = EventProc;

        dosbox_id_write_regsel(DOSBOX_ID_REG_USER_MOUSE_CURSOR_NORMALIZED);
        dosbox_id_write_data(1); /* PS/2 notification */

        // 60Hz bus mouse rate
        outp(0xBFDB,0x01);

        // turn on interrupts
        outp(0x7FDD,0x00);

        _sti();
    }
}

void WINAPI __loadds Disable(void) {
    if (AssignedEventProc) {
        _cli();

        dosbox_id_write_regsel(DOSBOX_ID_REG_USER_MOUSE_CURSOR_NORMALIZED);
        dosbox_id_write_data(0); /* disable */

        // 120Hz bus mouse rate
        outp(0xBFDB,0x00);

        // restore interrupt vector
        __asm {
            push    ds
            mov     ah,0x25         ; set interrupt vector
            mov     al,(0x08+13)    ; IRQ13
            mov     dx,word ptr old_interrupt_handler+0
            mov     ds,word ptr old_interrupt_handler+2
            int     21h
            pop     ds
        }

        AssignedEventProc = NULL;
        _sti();
    }
}

#if TARGET_WINDOWS >= 31
WORD WINAPI WEP(BOOL bSystemExit) {
    return 1;
}
#endif

unsigned char dos_version();
#pragma aux dos_version = \
    "mov    ah,0x30" \
    "int    21h" \
    modify [ah bx] \
    value [al]

WORD MiniLibMain(void) {
    /* we must return 1.
     * returning 0 makes Windows drop back to DOS.
     * Microsoft DDK example code always returns 1 so that failure to detect mouse
     * results in Windows coming up with no cursor. */
    if (!probe_dosbox_id())
        return 1;
    if (dos_version() < 3)
        return 1;

    /* it exists. fill in the struct. take shortcuts, struct is initialized as zero. */
    my_mouseinfo.msExist = 1;
    my_mouseinfo.msNumButtons = 2;
    my_mouseinfo.msRate = 60;
    return 1;
}

