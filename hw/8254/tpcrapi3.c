
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/8254/8254.h>

#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/dos/dosbox.h>
#include <hw/dos/dosntvdm.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

int main() {
#if defined(TARGET_WINDOWS) && TARGET_WINDOWS == 32 && !defined(WIN386)
    /* As a 32-bit Windows application, we *can* talk directly to the 8254 but only if running under Windows 3.1/95/98/ME.
     * Windows NT would never permit us to directly talk to IO.
     *
     * However if we're a Win16 program or Watcom 386 application, Windows NT will trap and emulate the 8254 through NTVDM.EXE */
    detect_windows();
    if (!(windows_mode == WINDOWS_REAL || windows_mode == WINDOWS_STANDARD || windows_mode == WINDOWS_ENHANCED)) {
        printf("This test program is only applicable to Windows 3.x/9x\n");
        return 1;
    }
#endif

	printf("8254 library test program\n");
	if (!probe_8254()) {
		printf("Chip not present. Your computer might be 2010-era hardware that dropped support for it.\n");
		return 1;
	}

    printf("This program tests the ability for emulation to handle\n");
    printf("rapid changes to PC speaker frequency. In this test, the\n");
    printf("same counter value (frequency) is repeatedly loaded into\n");
    printf("the PIT timer tied to the PC speaker, at varying intervals,\n");
    printf("to determine it's effects on the output.\n");

	write_8254_system_timer(0); /* restore normal function to prevent BIOS from going crazy */

    {
        uint16_t freqs;
        unsigned long i,delta;
        t8254_time_t pc,cc;
        int32_t tim,spk;

        freqs = (uint16_t)(T8254_REF_CLOCK_HZ / 100ul);

        t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_ON);

        cc = read_8254(0);

        spk = 0;
        for (i=(T8254_REF_CLOCK_HZ * 1ul);i > 10ul;) {
            tim = T8254_REF_CLOCK_HZ / 1000ul;

            if (kbhit()) {
                int c = getch();
                if (c == 27 || c == ' ')
                    break;
            }

            while (tim >= 0l) {
                if (spk <= 0l) {
                    write_8254_pc_speaker(freqs);
                    spk += i;
                }

                pc = cc;
                cc = read_8254(0);
                delta = (pc - cc) & 0xFFFFul; /* counts DOWN */

                tim -= delta;
                spk -= delta;
            }

            if (i >= 50000)
                i -= 100ul;
            else if (i >= 40000)
                i -= 10ul;
            else
                i--;
        }

        t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_OFF);
    }

	write_8254_system_timer(0); /* restore normal function to prevent BIOS from going crazy */
	return 0;
}

