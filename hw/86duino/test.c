 
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

/* northbridge 0:0:0 */
#define VORTEX86_PCI_NB_BUS         (0)
#define VORTEX86_PCI_NB_DEV         (0)
#define VORTEX86_PCI_NB_FUNC        (0)

uint8_t vtx86_nb_readb(const uint8_t reg) {
    return pci_read_cfgb(VORTEX86_PCI_NB_BUS,VORTEX86_PCI_NB_DEV,VORTEX86_PCI_NB_FUNC,reg);
}

uint16_t vtx86_nb_readw(const uint8_t reg) {
    return pci_read_cfgw(VORTEX86_PCI_NB_BUS,VORTEX86_PCI_NB_DEV,VORTEX86_PCI_NB_FUNC,reg);
}

uint32_t vtx86_nb_readl(const uint8_t reg) {
    return pci_read_cfgl(VORTEX86_PCI_NB_BUS,VORTEX86_PCI_NB_DEV,VORTEX86_PCI_NB_FUNC,reg);
}

void vtx86_nb_writeb(const uint8_t reg,const uint8_t val) {
    pci_write_cfgb(VORTEX86_PCI_NB_BUS,VORTEX86_PCI_NB_DEV,VORTEX86_PCI_NB_FUNC,reg,val);
}

void vtx86_nb_writew(const uint8_t reg,const uint16_t val) {
    pci_write_cfgw(VORTEX86_PCI_NB_BUS,VORTEX86_PCI_NB_DEV,VORTEX86_PCI_NB_FUNC,reg,val);
}

void vtx86_nb_writel(const uint8_t reg,const uint32_t val) {
    pci_write_cfgl(VORTEX86_PCI_NB_BUS,VORTEX86_PCI_NB_DEV,VORTEX86_PCI_NB_FUNC,reg,val);
}

/* southbridge 0:7:0 */
#define VORTEX86_PCI_SB_BUS         (0)
#define VORTEX86_PCI_SB_DEV         (7)
#define VORTEX86_PCI_SB_FUNC        (0)

uint8_t vtx86_sb_readb(const uint8_t reg) {
    return pci_read_cfgb(VORTEX86_PCI_SB_BUS,VORTEX86_PCI_SB_DEV,VORTEX86_PCI_SB_FUNC,reg);
}

uint16_t vtx86_sb_readw(const uint8_t reg) {
    return pci_read_cfgw(VORTEX86_PCI_SB_BUS,VORTEX86_PCI_SB_DEV,VORTEX86_PCI_SB_FUNC,reg);
}

uint32_t vtx86_sb_readl(const uint8_t reg) {
    return pci_read_cfgl(VORTEX86_PCI_SB_BUS,VORTEX86_PCI_SB_DEV,VORTEX86_PCI_SB_FUNC,reg);
}

void vtx86_sb_writeb(const uint8_t reg,const uint8_t val) {
    pci_write_cfgb(VORTEX86_PCI_SB_BUS,VORTEX86_PCI_SB_DEV,VORTEX86_PCI_SB_FUNC,reg,val);
}

void vtx86_sb_writew(const uint8_t reg,const uint16_t val) {
    pci_write_cfgw(VORTEX86_PCI_SB_BUS,VORTEX86_PCI_SB_DEV,VORTEX86_PCI_SB_FUNC,reg,val);
}

void vtx86_sb_writel(const uint8_t reg,const uint32_t val) {
    pci_write_cfgl(VORTEX86_PCI_SB_BUS,VORTEX86_PCI_SB_DEV,VORTEX86_PCI_SB_FUNC,reg,val);
}

uint16_t    uart_config_base_io = 0;

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
    /* check the northbridge and southbridge */
    {
        uint16_t vendor,device;

        vendor = vtx86_nb_readw(0x00);
        device = vtx86_nb_readw(0x02);
        if (vendor == 0xFFFF || device == 0xFFFF || vendor != 0x17f3) {
            printf("Northbridge does not seem correct\n");
            return 1;
        }

        vendor = vtx86_sb_readw(0x00);
        device = vtx86_sb_readw(0x02);
        if (vendor == 0xFFFF || device == 0xFFFF || vendor != 0x17f3) {
            printf("Southbridge does not seem correct\n");
            return 1;
        }
    }

    printf("Looks like a Vortex86 SoC / 86Duino\n");

    uart_config_base_io = vtx86_sb_readw(0x60);
    if (uart_config_base_io & 1u)
        uart_config_base_io &= ~1u;
    else
        uart_config_base_io  = 0;

    printf("- UART Configuration I/O port base:     0x%04x\n",uart_config_base_io);

	return 0;
}

