 
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/pci/pci.h>
#include <hw/cpu/cpu.h>
#include <hw/8254/8254.h>
#include <hw/86duino/86duino.h>

int main(int argc,char **argv) {
    int pin;

    if (argc < 2) {
        printf("specify pin number\n");
        return 1;
    }
    pin = atoi(argv[1]);

    cpu_probe();
    probe_dos();
    if (!probe_vtx86()) { // NTS: Will ALSO probe for the PCI bus
        printf("This program requires a 86duino embedded SoC to run\n");
        return 1;
    }
    if (!probe_8254()) {
        printf("Chip not present. Your computer might be 2010-era hardware that dropped support for it.\n");
        return 1;
    }

    printf("Looks like a Vortex86 SoC / 86Duino\n");
    if (vtx86_86duino_flags & VTX86_86DUINO_FLAG_SB1)
        printf("- Southbridge function 1 exists\n");
    if (vtx86_86duino_flags & VTX86_86DUINO_FLAG_MC)
        printf("- MC exists\n");

    if (!read_vtx86_config()) {
        printf("Failed to read config\n");
        return 1;
    }

    vtx86_pinMode(pin,VTX86_OUTPUT);
    {
        unsigned int i;

        for (i=0;i < 10;i++) {
            vtx86_digitalWrite(pin,((i & 1) == 0) ? 0 : 1);
            t8254_wait(t8254_us2ticks(500000));
        }
    }

    return 0;
}

