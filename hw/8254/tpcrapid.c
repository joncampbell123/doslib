
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
    printf("rapid changes to PC speaker frequency.\n");

	write_8254_system_timer(0); /* restore normal function to prevent BIOS from going crazy */
    write_8254_pc_speaker(0); /* make sure the PIT timer is in mode 3 (square wave) */

#if defined(NO_PORT_43)
# define write_8254_pc_speaker writenm_8254_pc_speaker
#endif

    {
        uint16_t freqs[4];
        unsigned int freqi;
        unsigned long i,delta;
        t8254_time_t pc,cc;
        int32_t tim,spk;

        freqs[0] = (uint16_t)(T8254_REF_CLOCK_HZ / 110ul);
        freqs[1] = (uint16_t)(T8254_REF_CLOCK_HZ / 220ul);
        freqs[2] = (uint16_t)(T8254_REF_CLOCK_HZ / 440ul);
        freqs[3] = (uint16_t)(T8254_REF_CLOCK_HZ / 880ul);

        t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_ON);

        cc = read_8254(0);

        for (i=(T8254_REF_CLOCK_HZ * 1ul);i >= (T8254_REF_CLOCK_HZ / 2000ul);i /= 2ul) {
            tim = T8254_REF_CLOCK_HZ * 4ul;
            freqi = 0;
            spk = 0;

            if (kbhit()) {
                int c = getch();
                if (c == 27 || c == ' ')
                    break;
            }

            while (tim >= 0l) {
                if (spk <= 0l) {
                    write_8254_pc_speaker(freqs[freqi]);
                    freqi = (freqi + 1u) & 3u;
                    spk += i;
                }

                pc = cc;
                cc = read_8254(0);
                delta = (pc - cc) & 0xFFFFul; /* counts DOWN */

                tim -= delta;
                spk -= delta;
            }
        }

        t8254_pc_speaker_set_gate(PC_SPEAKER_GATE_OFF);
    }

	write_8254_system_timer(0); /* restore normal function to prevent BIOS from going crazy */
	return 0;
}

