
#include <windows.h>
#include <stdlib.h>
#include <stdint.h>
#include <conio.h>

#include <hw/cpu/cpu.h>

#include "iglib.h"

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

MOUSEINFO my_mouseinfo = {0};

extern const void far * __based( __segname("_NDDATA") ) AssignedEventProc;
extern unsigned short __based( __segname("_NDDATA") ) prev_x;
extern unsigned short __based( __segname("_NDDATA") ) prev_y;
extern unsigned char __based( __segname("_NDDATA") ) prev_status;

extern void far int15_handler();

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
        unsigned char fail=1;

        _cli();

        __asm {
            stc

            mov     ax,0xC205           ; PS/2 initialize
            mov     bh,3                ; data package size
            int     15h
            jc      failed

            mov     ax,0xC201           ; PS/2 reset interface
            int     15h
            jc      failed

            mov     ax,0xC207           ; PS/2 set pointing device
            mov     bx,seg int15_handler
            mov     es,bx
            mov     bx,offset int15_handler
            int     15h
            jc      failed

            stc
            mov     ax,0xC200           ; PS/2 enable/disable
            mov     bh,0x01             ; enable
            int     15h
            jc      failed

            mov     fail,0
failed:
        }

        if (fail) return;

        AssignedEventProc = EventProc;

        dosbox_id_write_regsel(DOSBOX_ID_REG_USER_MOUSE_CURSOR_NORMALIZED);
        dosbox_id_write_data(1); /* PS/2 notification */

        _sti();
    }
}

void WINAPI __loadds Disable(void) {
    if (AssignedEventProc) {
        _cli();

        dosbox_id_write_regsel(DOSBOX_ID_REG_USER_MOUSE_CURSOR_NORMALIZED);
        dosbox_id_write_data(0); /* disable */

        __asm {
            stc

            stc
            mov     ax,0xC200           ; PS/2 enable/disable
            mov     bh,0x00             ; disable
            int     15h

            mov     ax,0xC201           ; PS/2 reset interface
            int     15h
        }

        AssignedEventProc = NULL;
        _sti();
    }
}

WORD WINAPI WEP(BOOL bSystemExit) {
    return 1;
}

unsigned short bios_equipment();
#pragma aux bios_equipment = \
    "int    11h" \
    value [ax]

unsigned char dos_version();
#pragma aux dos_version = \
    "mov    ah,0x30" \
    "int    21h" \
    modify [ah bx] \
    value [al]

unsigned char far *bios_get_config();
#pragma aux bios_get_config = \
    "stc" \
    "mov    ah,0xC0" \
    "int    15h" \
    "jnc    @F" \
    "xor    bx,bx" \
    "mov    es,bx" \
    "@F:" \
    modify [bx ax] \
    value [es bx]

WORD MiniLibMain(void) {
    /* we must return 1.
     * returning 0 makes Windows drop back to DOS.
     * Microsoft DDK example code always returns 1 so that failure to detect mouse
     * results in Windows coming up with no cursor. */
    if (!probe_dosbox_id())
        return 1;
    if (dos_version() < 3)
        return 1;

    {
        unsigned char far *p = bios_get_config();
        if (p == NULL)
            return 1;
        if (p[2] > 0xFC) /* anything older than AT is not supported */
            return 1;
    }

    if (!(bios_equipment() & 4)) /* Pointing device (PS/2 mouse) installed according to BIOS? (assuming PS/2 or higher) */
        return 1;

    /* it exists. fill in the struct. take shortcuts, struct is initialized as zero. */
    my_mouseinfo.msExist = 1;
    my_mouseinfo.msNumButtons = 2;
    my_mouseinfo.msRate = 60;
    return 1;
}

