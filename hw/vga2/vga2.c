
#include <i86.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/vga2/vga2.h>

#ifndef TARGET_PC98
/* INT 10h AX=0x1A00 GET DISPLAY COMBINATION CODE (PS,VGA/MCGA) values of BL [http://www.ctyme.com/intr/rb-0219.htm] */
# define probe_vga2_dcc_to_flags_sz 0x0D
static const uint8_t probe_vga2_dcc_to_flags[probe_vga2_dcc_to_flags_sz] = {
    VGA2_FLAG_NONE,                                                                         // 0x00
    VGA2_FLAG_MDA | VGA2_FLAG_DIGITAL_DISPLAY | VGA2_FLAG_MONO_DISPLAY,                     // 0x01
    VGA2_FLAG_CGA | VGA2_FLAG_DIGITAL_DISPLAY,                                              // 0x02
    VGA2_FLAG_NONE,                                                                         // 0x03 reserved
    VGA2_FLAG_CGA | VGA2_FLAG_EGA | VGA2_FLAG_DIGITAL_DISPLAY,                              // 0x04
    VGA2_FLAG_CGA | VGA2_FLAG_EGA | VGA2_FLAG_DIGITAL_DISPLAY | VGA2_FLAG_MONO_DISPLAY,     // 0x05
    VGA2_FLAG_CGA | VGA2_FLAG_PGA,                                                          // 0x06
    VGA2_FLAG_CGA | VGA2_FLAG_EGA | VGA2_FLAG_VGA | VGA2_FLAG_MONO_DISPLAY,                 // 0x07
    VGA2_FLAG_CGA | VGA2_FLAG_EGA | VGA2_FLAG_VGA,                                          // 0x08
    VGA2_FLAG_NONE,                                                                         // 0x09 reserved
    VGA2_FLAG_CGA | VGA2_FLAG_MCGA | VGA2_FLAG_DIGITAL_DISPLAY,                             // 0x0A
    VGA2_FLAG_CGA | VGA2_FLAG_MCGA | VGA2_FLAG_MONO_DISPLAY,                                // 0x0B
    VGA2_FLAG_CGA | VGA2_FLAG_MCGA                                                          // 0x0C
};
#endif

#ifndef TARGET_PC98
/* INT 10h AH=0x12 BL=0x10 GET EGA INFO values of CL [http://www.ctyme.com/intr/rb-0162.htm] */
# define probe_vga2_egasw_to_flags_sz 0x0C
static const uint8_t probe_vga2_egasw_to_flags[probe_vga2_egasw_to_flags_sz] = {
    VGA2_FLAG_MDA | VGA2_FLAG_DIGITAL_DISPLAY | VGA2_FLAG_MONO_DISPLAY,                     // 0x00 also secondary EGA+ 40x25
    VGA2_FLAG_MDA | VGA2_FLAG_DIGITAL_DISPLAY | VGA2_FLAG_MONO_DISPLAY,                     // 0x01 also secondary EGA+ 80x25
    VGA2_FLAG_MDA | VGA2_FLAG_DIGITAL_DISPLAY | VGA2_FLAG_MONO_DISPLAY,                     // 0x02 also secondary EGA+ 80x25
    VGA2_FLAG_MDA | VGA2_FLAG_DIGITAL_DISPLAY | VGA2_FLAG_MONO_DISPLAY,                     // 0x03 also secondary EGA+ 80x25
    VGA2_FLAG_CGA | VGA2_FLAG_DIGITAL_DISPLAY,                                              // 0x04 also secondary EGA+ 80x25 mono, primary CGA 40x25
    VGA2_FLAG_CGA | VGA2_FLAG_DIGITAL_DISPLAY,                                              // 0x05 also secondary EGA+ 80x25 mono, primary CGA 80x25
    VGA2_FLAG_CGA | VGA2_FLAG_EGA,                                                          // 0x06 also optional secondary MDA/Herc
    VGA2_FLAG_CGA | VGA2_FLAG_EGA,                                                          // 0x07 also optional secondary MDA/Herc
    VGA2_FLAG_CGA | VGA2_FLAG_EGA,                                                          // 0x08 also optional secondary MDA/Herc
    VGA2_FLAG_CGA | VGA2_FLAG_EGA,                                                          // 0x09 also optional secondary MDA/Herc
    VGA2_FLAG_CGA | VGA2_FLAG_EGA | VGA2_FLAG_MONO_DISPLAY,                                 // 0x0A also optional secondary CGA 40x25
    VGA2_FLAG_CGA | VGA2_FLAG_EGA | VGA2_FLAG_MONO_DISPLAY                                  // 0x0B also optional secondary CGA 80x25
};
#endif

#ifdef TARGET_PC98
/* nothing */
#else
uint8_t                         vga2_flags = 0;
#endif

/* Unlike the first VGA library, this probe function only focuses on the primary
 * classification of video hardware: MDA, CGA, EGA, VGA, MCGA, and PGA. The code
 * is kept minimal to keep executable size down. Tandy and PCjr will show up as
 * CGA, Hercules as MDA, any SVGA chipset as VGA.
 *
 * Tandy, PCjr, SVGA, Hercules, and any other secondary classification will have
 * their own detection routines if your program is interested in them. The goal
 * is that, if a program is not interested in PCjr or Tandy support then it can
 * keep code bloat down by not calling the probe function for them and sticking
 * with the primary classification here.
 *
 * This probing code does NOT initialize the alphanumeric pointer and state
 * information, so that if the host does not call anything related to that, then
 * nothing of that sort is linked into the executable (i.e. games that do not
 * use text mode).
 *
 * Please note that on MCGA hardware (IBM PS/2 Model 30), if the monitor is not
 * connected, the MCGA hardware will be configured to emit a CGA-like 60Hz 240p
 * mode and the API will return DCC code 0x00 (no monitor). This check and
 * configuration is done every time the BIOS runs POST, whether first power on
 * or CTRL+ALT+DEL reset.
 *
 * If your program prefers to know whether or not it is on MCGA in that condition
 * (when the user has attached a monitor after the fact), you will need to do
 * additional checks on the model byte to confirm it is MCGA despite the unhelpful
 * DCC from the BIOS. However that is an inconvenient use case the user is not
 * likely to set up, so this function will not do it for you.
 *
 * Naturally when MCGA is in CGA-like 200-line mode, the 640x480 monochrome mode
 * is not available. */
#ifdef TARGET_PC98
    /*nothing*/
#else
void probe_vga2(void) {
    if (vga2_flags != 0)
        return;

    /* First: VGA/MCGA detection. Ask the BIOS. */
    {
        const unsigned char dcc = vga2_get_dcc_inline();
        if (dcc < probe_vga2_dcc_to_flags_sz) {
            vga2_flags = probe_vga2_dcc_to_flags[dcc];
            goto done;
        }
    }

    /* Then: EGA. Ask the BIOS */
    {
        const unsigned char egasw = vga2_alt_ega_switches_inline();
        if (egasw < probe_vga2_egasw_to_flags_sz) {
            vga2_flags = probe_vga2_egasw_to_flags[egasw];
            goto done;
        }
    }

    /* Then: MDA/CGA detection. Prefer whatever the BIOS started up with. */
    /*       Please NOTE on original IBM 5150 hardware this comes from the DIP switches on the board
     *       that tells the BIOS whether MDA or CGA is installed. The design allows both MDA and CGA
     *       to be installed at the same time in which case the DIP switches tell the BIOS which one
     *       to use by default, and therefore this library as well. */
    {
        /* equipment word, bits [5:4]   3=80x25 mono  2=80x25 color  1=40x25 color  0=EGA, VGA, PGA */
        if (((unsigned char)vga2_int11_equipment() & (unsigned char)0x30u) == (unsigned char)0x30u)
            vga2_flags = VGA2_FLAG_MDA | VGA2_FLAG_DIGITAL_DISPLAY | VGA2_FLAG_MONO_DISPLAY;
        else
            vga2_flags = VGA2_FLAG_CGA | VGA2_FLAG_DIGITAL_DISPLAY;
    }

done:
    { }
}
#endif

