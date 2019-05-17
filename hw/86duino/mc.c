 
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

uint8_t vtx86_mc_readb(const uint8_t reg) {
    return pci_read_cfgb(VORTEX86_PCI_MC_BUS,VORTEX86_PCI_MC_DEV,VORTEX86_PCI_MC_FUNC,reg);
}

uint16_t vtx86_mc_readw(const uint8_t reg) {
    return pci_read_cfgw(VORTEX86_PCI_MC_BUS,VORTEX86_PCI_MC_DEV,VORTEX86_PCI_MC_FUNC,reg);
}

uint32_t vtx86_mc_readl(const uint8_t reg) {
    return pci_read_cfgl(VORTEX86_PCI_MC_BUS,VORTEX86_PCI_MC_DEV,VORTEX86_PCI_MC_FUNC,reg);
}

void vtx86_mc_writeb(const uint8_t reg,const uint8_t val) {
    pci_write_cfgb(VORTEX86_PCI_MC_BUS,VORTEX86_PCI_MC_DEV,VORTEX86_PCI_MC_FUNC,reg,val);
}

void vtx86_mc_writew(const uint8_t reg,const uint16_t val) {
    pci_write_cfgw(VORTEX86_PCI_MC_BUS,VORTEX86_PCI_MC_DEV,VORTEX86_PCI_MC_FUNC,reg,val);
}

void vtx86_mc_writel(const uint8_t reg,const uint32_t val) {
    pci_write_cfgl(VORTEX86_PCI_MC_BUS,VORTEX86_PCI_MC_DEV,VORTEX86_PCI_MC_FUNC,reg,val);
}

