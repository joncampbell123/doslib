 
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/necpc98/necpc98.h>

// INT DCh CL=0Ch AX=00h
#if TARGET_MSDOS == 32
void pc98_intdc_get_keycode_state(nec_pc98_intdc_keycode_state FAR * FAR s) {
    // TODO: The DOS extender probably DOESN'T auto-translate this call
}
#else
#endif

// INT DCh CL=0Ch AX=FFh
#if TARGET_MSDOS == 32
void pc98_intdc_get_keycode_state_ext(nec_pc98_intdc_keycode_state_ext FAR * FAR s) {
    // TODO: The DOS extender probably DOESN'T auto-translate this call
}
#else
#endif

// INT DCh CL=0Dh AX=00h
#if TARGET_MSDOS == 32
void pc98_intdc_set_keycode_state(const nec_pc98_intdc_keycode_state FAR * FAR s) {
    // TODO: The DOS extender probably DOESN'T auto-translate this call
}
#else
#endif

// INT DCh CL=0Dh AX=FFh
#if TARGET_MSDOS == 32
void pc98_intdc_set_keycode_state_ext(const nec_pc98_intdc_keycode_state_ext FAR * FAR s) {
    // TODO: The DOS extender probably DOESN'T auto-translate this call
}
#else
#endif

