 
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/pci/pci.h>
#include <hw/cpu/cpu.h>

int main(int argc,char **argv) {
	cpu_probe();
    probe_dos();
    /* Vortex86 systems are (as far as I know) at least Pentium level (maybe 486?), and support CPUID with vendor "Vortex86 SoC" */
    if (cpu_basic_level < CPU_486 || !(cpu_flags & CPU_FLAG_CPUID) || memcmp(cpu_cpuid_vendor,"Vortex86 SoC",12) != 0) {
        printf("Doesn't look like a Vortex86 SoC\n");
        return 1;
    }
    /* They also have a PCI bus that's integral to locating and talking to the GPIO pins, etc. */
	if (pci_probe(PCI_CFG_TYPE1/*Vortex86 uses this type, definitely!*/) == PCI_CFG_NONE) {
		printf("PCI bus not found\n");
		return 1;
	}
    printf("Looks like a Vortex86 SoC / 86Duino\n");
	
	return 0;
}

