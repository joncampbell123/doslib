 
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
#include <hw/86duino/86duino.h>

unsigned char                   vtx86_86duino_flags = 0;

struct vtx86_cfg_t              vtx86_cfg = {0};
struct vtx86_gpio_port_cfg_t    vtx86_gpio_port_cfg = {0};

/* NTS: This so far has only been tested against the 86Duino embedded systems */
int probe_vtx86(void) {
    if (!(vtx86_86duino_flags & VTX86_86DUINO_FLAG_DETECTED)) {
        uint16_t vendor,device;

        vtx86_86duino_flags = 0;

        cpu_probe();
        /* Vortex86 systems are (as far as I know) at least Pentium level (maybe 486?), and support CPUID with vendor "Vortex86 SoC" */
        if (cpu_basic_level < CPU_486 || !(cpu_flags & CPU_FLAG_CPUID) || memcmp(cpu_cpuid_vendor,"Vortex86 SoC",12) != 0)
            return 0;
        /* They also have a PCI bus that's integral to locating and talking to the GPIO pins, etc. */
        if (pci_probe(PCI_CFG_TYPE1/*Vortex86 uses this type, definitely!*/) == PCI_CFG_NONE)
            return 0;

        /* check the northbridge and southbridge */
        vendor = vtx86_nb_readw(0x00);
        device = vtx86_nb_readw(0x02);
        if (vendor == 0xFFFF || device == 0xFFFF || vendor != 0x17f3)
            return 0;

        vendor = vtx86_sb_readw(0x00);
        device = vtx86_sb_readw(0x02);
        if (vendor == 0xFFFF || device == 0xFFFF || vendor != 0x17f3)
            return 0;

        vendor = vtx86_sb1_readw(0x00);
        device = vtx86_sb1_readw(0x02);
        if (!(vendor == 0xFFFF || device == 0xFFFF || vendor != 0x17f3))
            vtx86_86duino_flags |= VTX86_86DUINO_FLAG_SB1;

        vendor = vtx86_mc_readw(0x00);
        device = vtx86_mc_readw(0x02);
        if (!(vendor == 0xFFFF || device == 0xFFFF || vendor != 0x17f3))
            vtx86_86duino_flags |= VTX86_86DUINO_FLAG_MC;

        vtx86_86duino_flags |= VTX86_86DUINO_FLAG_DETECTED;
    }

    return !!(vtx86_86duino_flags & VTX86_86DUINO_FLAG_DETECTED);
}

