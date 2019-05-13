 
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

const int8_t vtx86_uart_IRQs[16] = {
    -1, 9, 3,10,
     4, 5, 7, 6,
     1,11,-1,12,
    -1,14,-1,15
};

const uint8_t vtx86_gpio_to_crossbar_pin_map[45] =
   {11, 10, 39, 23, 37, 20, 19, 35,
    33, 17, 28, 27, 32, 25, 12, 13,
    14, 15, 24, 26, 29, 47, 46, 45,
    44, 43, 42, 41, 40,  1,  3,  4,
    31,  0,  2,  5, 22, 30,  6, 38,
    36, 34, 16, 18, 21};

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

/* southbridge 0:7:1 */
#define VORTEX86_PCI_SB1_BUS        (0)
#define VORTEX86_PCI_SB1_DEV        (7)
#define VORTEX86_PCI_SB1_FUNC       (1)

uint8_t vtx86_sb1_readb(const uint8_t reg) {
    return pci_read_cfgb(VORTEX86_PCI_SB1_BUS,VORTEX86_PCI_SB1_DEV,VORTEX86_PCI_SB1_FUNC,reg);
}

uint16_t vtx86_sb1_readw(const uint8_t reg) {
    return pci_read_cfgw(VORTEX86_PCI_SB1_BUS,VORTEX86_PCI_SB1_DEV,VORTEX86_PCI_SB1_FUNC,reg);
}

uint32_t vtx86_sb1_readl(const uint8_t reg) {
    return pci_read_cfgl(VORTEX86_PCI_SB1_BUS,VORTEX86_PCI_SB1_DEV,VORTEX86_PCI_SB1_FUNC,reg);
}

void vtx86_sb1_writeb(const uint8_t reg,const uint8_t val) {
    pci_write_cfgb(VORTEX86_PCI_SB1_BUS,VORTEX86_PCI_SB1_DEV,VORTEX86_PCI_SB1_FUNC,reg,val);
}

void vtx86_sb1_writew(const uint8_t reg,const uint16_t val) {
    pci_write_cfgw(VORTEX86_PCI_SB1_BUS,VORTEX86_PCI_SB1_DEV,VORTEX86_PCI_SB1_FUNC,reg,val);
}

void vtx86_sb1_writel(const uint8_t reg,const uint32_t val) {
    pci_write_cfgl(VORTEX86_PCI_SB1_BUS,VORTEX86_PCI_SB1_DEV,VORTEX86_PCI_SB1_FUNC,reg,val);
}

/* MC 0:10:0 */
#define VORTEX86_PCI_MC_BUS         (0)
#define VORTEX86_PCI_MC_DEV         (0x10)
#define VORTEX86_PCI_MC_FUNC        (0)

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

struct vtx86_cfg_t {
    uint16_t        uart_config_base_io;
    uint16_t        gpio_intconfig_base_io;
    uint16_t        gpio_portconfig_base_io;
    uint16_t        crossbar_config_base_io;
    uint16_t        pwm_config_base_io;
    uint16_t        adc_config_base_io;
};

/* gpio_portconfig */
struct vtx86_gpio_port_cfg_pin_t {
    uint16_t                        dir_io;
    uint16_t                        data_io;
};

struct vtx86_gpio_port_cfg_t {
    uint32_t                                gpio_dec_enable;
    struct vtx86_gpio_port_cfg_pin_t        gpio_pin[10];
};

unsigned char   vtx86_86duino_flags = 0;
#define VTX86_86DUINO_FLAG_SB1          (1u << 0u)
#define VTX86_86DUINO_FLAG_MC           (1u << 1u)

struct vtx86_cfg_t  vtx86_cfg = {0};

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

        vtx86_86duino_flags = 0;

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

        vendor = vtx86_sb1_readw(0x00);
        device = vtx86_sb1_readw(0x02);
        if (!(vendor == 0xFFFF || device == 0xFFFF || vendor != 0x17f3))
            vtx86_86duino_flags |= VTX86_86DUINO_FLAG_SB1;

        vendor = vtx86_mc_readw(0x00);
        device = vtx86_mc_readw(0x02);
        if (!(vendor == 0xFFFF || device == 0xFFFF || vendor != 0x17f3))
            vtx86_86duino_flags |= VTX86_86DUINO_FLAG_MC;
    }

    printf("Looks like a Vortex86 SoC / 86Duino\n");
    if (vtx86_86duino_flags & VTX86_86DUINO_FLAG_SB1)
        printf("- Southbridge function 1 exists\n");
    if (vtx86_86duino_flags & VTX86_86DUINO_FLAG_MC)
        printf("- MC exists\n");

    vtx86_cfg.uart_config_base_io = vtx86_sb_readw(0x60);
    if (vtx86_cfg.uart_config_base_io & 1u)
        vtx86_cfg.uart_config_base_io &= ~1u;
    else
        vtx86_cfg.uart_config_base_io  = 0;

    printf("- UART Configuration I/O port base:     0x%04x\n",vtx86_cfg.uart_config_base_io);

    if (vtx86_cfg.uart_config_base_io != 0u) {
        unsigned int idx;

        for (idx=0;idx < 10;idx++) {
            uint32_t raw = inpd(vtx86_cfg.uart_config_base_io + (idx * 4u));

            printf("  - UART %u\n",idx+1);
            printf("    - Internal UART I/O addr decode:    %u\n",(raw & (1ul << 23ul)) ? 1 : 0);
            if (raw & (1ul << 23ul)) {
                printf("    - Half duplex:                      %u\n",(raw & (1ul << 25ul)) ? 1 : 0);
                printf("    - Forward port 80h to UART data:    %u\n",(raw & (1ul << 24ul)) ? 1 : 0);
                printf("    - UART clock select:                %u\n",(raw & (1ul << 22ul)) ? 1 : 0);
                printf("    - FIFO size select:                 %u\n",(raw & (1ul << 21ul)) ?32 :16);
                printf("    - UART clock ratio select:          1/%u\n",(raw & (1ul<<20ul)) ? 8 :16);
                printf("    - IRQ:                              %d\n",vtx86_uart_IRQs[(raw >> 16ul) & 15ul]);
                printf("    - I/O base:                         0x%04x\n",(unsigned int)(raw & 0xFFF8ul));
            }
        }
    }

    vtx86_cfg.gpio_portconfig_base_io = vtx86_sb_readw(0x62);
    if (vtx86_cfg.gpio_portconfig_base_io & 1u)
        vtx86_cfg.gpio_portconfig_base_io &= ~1u;
    else
        vtx86_cfg.gpio_portconfig_base_io  = 0;

    printf("- GPIO Port configuration I/O port base:0x%04x\n",vtx86_cfg.gpio_portconfig_base_io);
    if (vtx86_cfg.gpio_portconfig_base_io != 0u) {
        struct vtx86_gpio_port_cfg_t pins;
        unsigned int i;
        uint32_t tmp;

        pins.gpio_dec_enable = inpd(vtx86_cfg.gpio_portconfig_base_io + 0x00);
        for (i=0;i < 10;i++) {
            tmp = inpd(vtx86_cfg.gpio_portconfig_base_io + 0x04 + (i * 4u));
            pins.gpio_pin[i].data_io = (uint16_t)(tmp & 0xFFFFul);
            pins.gpio_pin[i].dir_io  = (uint16_t)(tmp >> 16ul);
        }

        printf("  - GPIO data/dir port enable:          0x%lx",(unsigned long)pins.gpio_dec_enable);
        for (i=0;i < 10;i++) printf(" P%u=%u",i,(pins.gpio_dec_enable & (1ul << (unsigned long)i)) ? 1 : 0);
        printf("\n");

        for (i=0;i < 10;i++) {
            printf("  - GPIO pin %u I/O:                     data=0x%x dir=0x%x\n",
                    i,pins.gpio_pin[i].data_io,pins.gpio_pin[i].dir_io);
        }
    }

    vtx86_cfg.gpio_intconfig_base_io = vtx86_sb_readw(0x66);
    if (vtx86_cfg.gpio_intconfig_base_io & 1u)
        vtx86_cfg.gpio_intconfig_base_io &= ~1u;
    else
        vtx86_cfg.gpio_intconfig_base_io  = 0;

    printf("- GPIO Interrupt config, I/O int base:  0x%04x\n",vtx86_cfg.gpio_intconfig_base_io);

    vtx86_cfg.crossbar_config_base_io = vtx86_sb_readw(0x64);
    if (vtx86_cfg.crossbar_config_base_io & 1u)
        vtx86_cfg.crossbar_config_base_io &= ~1u;
    else
        vtx86_cfg.crossbar_config_base_io  = 0;

    printf("- Crossbar Configuration I/O port base: 0x%04x\n",vtx86_cfg.crossbar_config_base_io);

    if (vtx86_86duino_flags & VTX86_86DUINO_FLAG_SB1) {
        vtx86_cfg.adc_config_base_io = vtx86_sb1_readw(0xE0);
        if (vtx86_cfg.adc_config_base_io & 1u)
            vtx86_cfg.adc_config_base_io &= ~1u;
        else
            vtx86_cfg.adc_config_base_io  = 0;
    }
    else {
        vtx86_cfg.adc_config_base_io = 0;
    }

    printf("- ADC I/O port base:                    0x%04x\n",vtx86_cfg.adc_config_base_io);

    if (vtx86_86duino_flags & VTX86_86DUINO_FLAG_MC) {
        vtx86_cfg.pwm_config_base_io = vtx86_mc_readw(0x10);
        if (vtx86_cfg.pwm_config_base_io & 1u)
            vtx86_cfg.pwm_config_base_io &= ~1u;
        else
            vtx86_cfg.pwm_config_base_io  = 0;
    }
    else {
        vtx86_cfg.pwm_config_base_io = 0;
    }

    printf("- PWM I/O port base:                    0x%04x\n",vtx86_cfg.pwm_config_base_io);

	return 0;
}

