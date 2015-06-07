#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/tgusmega.h>
#include <hw/dos/tgussbos.h>
#include <hw/dos/tgusumid.h>

/* this program demonstrates how to detect Gravis Ultrasound TSR programs */
int main() {
    {
	struct mega_em_info nfo;
        if (gravis_mega_em_detect(&nfo)) {
	    printf("MEGA-EM v%u.%02u is resident on INT %02xh. (%04xh)\n",
	    	nfo.version>>8,
		nfo.version&0xFF,
	    	nfo.intnum,
		nfo.response);
        }
    }

    {
	int v = gravis_sbos_detect();
	if (v >= 0) printf("SBOS installed on INT %02xh\n",v);
    }
    
    {
	int v = gravis_ultramid_detect();
	if (v >= 0) printf("ULTRAMID installed on INT %02xh\n",v);
    }
    
    return 0;
}

