#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <malloc.h>
#include <fcntl.h>
#include <ctype.h>
#include <i86.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#include <hw/vga/vgatty.h>
#include <hw/vga/vgagui.h>

#ifdef TARGET_WINDOWS
# include <hw/dos/winfcon.h>
# include <windows/apihelp.h>
# include <windows/dispdib/dispdib.h>
# include <windows/win16eb/win16eb.h>
#endif

#if defined(TARGET_PC98)
# include <hw/necpc98/necpc98.h>
void vga_tty_pc98_mapping(nec_pc98_intdc_keycode_state_ext *map) {
    unsigned int i;

    memset(map,0,sizeof(*map));

    /* function keys */
    for (i=0;i < 10;i++) {
        map->func[i].skey[0] = 0x7F;                // cannot use 0x00, that's the string terminator. VZ.EXE uses this.
        map->func[i].skey[1] = 0x30 + i;            // F1-F10 to 0x30-0x39
    }
    for (i=0;i < 5;i++) {
        map->vfunc[i].skey[0] = 0x7F;               // cannot use 0x00, that's the string terminator. VZ.EXE uses this.
        map->vfunc[i].skey[1] = 0x3A + i;           // VF1-VF10 to 0x3A-0x3E
    }
    for (i=0;i < 10;i++) {
        map->shift_func[i].skey[0] = 0x7F;          // cannot use 0x00, that's the string terminator. VZ.EXE uses this.
        map->shift_func[i].skey[1] = 0x40 + i;      // shift F1-F10 to 0x40-0x49
    }
    for (i=0;i < 5;i++) {
        map->shift_vfunc[i].skey[0] = 0x7F;         // cannot use 0x00, that's the string terminator. VZ.EXE uses this.
        map->shift_vfunc[i].skey[1] = 0x4A + i;     // shift VF1-VF10 to 0x4A-0x4E
    }
    for (i=0;i < 10;i++) {
        map->ctrl_func[i].skey[0] = 0x7F;           // cannot use 0x00, that's the string terminator. VZ.EXE uses this.
        map->ctrl_func[i].skey[1] = 0x50 + i;       // control F1-F10 to 0x50-0x59
    }
    for (i=0;i < 5;i++) {
        map->ctrl_vfunc[i].skey[0] = 0x7F;          // cannot use 0x00, that's the string terminator. VZ.EXE uses this.
        map->ctrl_vfunc[i].skey[1] = 0x5A + i;      // control VF1-VF10 to 0x5A-0x5E
    }
    for (i=0;i < 11;i++) {
        map->editkey[i].skey[0] = 0x7F;             // cannot use 0x00, that's the string terminator. VZ.EXE uses this.
        map->editkey[i].skey[1] = 0x60 + i;         // editor keys (see enum) to 0x60-0x6A
    }
}
#endif

