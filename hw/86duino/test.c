 
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

const int8_t vtx86_uart_IRQs[16] = {
    -1, 9, 3,10,
     4, 5, 7, 6,
     1,11,-1,12,
    -1,14,-1,15
};

int main(int argc,char **argv) {
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

    printf("- GPIO Port configuration I/O port base:0x%04x\n",vtx86_cfg.gpio_portconfig_base_io);
    if (vtx86_cfg.gpio_portconfig_base_io != 0u) {
        if (read_vtx86_gpio_pin_config()) {
            unsigned int i;

            printf("  - GPIO data/dir port enable:          0x%lx",(unsigned long)vtx86_gpio_port_cfg.gpio_dec_enable);
            for (i=0;i < 10;i++) printf(" PG%u=%u",i,(vtx86_gpio_port_cfg.gpio_dec_enable & (1ul << (unsigned long)i)) ? 1 : 0);
            printf("\n");

            for (i=0;i < 10;i++) {
                printf("  - GPIO pin group %u I/O:               data=0x%x dir=0x%x\n",i,
                        vtx86_gpio_port_cfg.gpio_pingroup[i].data_io,
                        vtx86_gpio_port_cfg.gpio_pingroup[i].dir_io);
            }
        }
        else {
            printf("  - Unable to read configuration\n");
        }
    }

    printf("- GPIO Interrupt config, I/O int base:  0x%04x\n",vtx86_cfg.gpio_intconfig_base_io);

    printf("- Crossbar Configuration I/O port base: 0x%04x\n",vtx86_cfg.crossbar_config_base_io);

    printf("- ADC I/O port base:                    0x%04x\n",vtx86_cfg.adc_config_base_io);

#if 0
    if (vtx86_init_adc()) {
        unsigned int i;
        uint16_t v;

        printf("  - ADC I/O port base after init:       0x%04x\n",vtx86_cfg.adc_config_base_io);

        for (i=0;i < 32;i++) {
            v = vtx86_analogRead(0);
            printf("%04x ",v);
        }
        printf("\n");
    }
#endif

    printf("- PWM I/O port base:                    0x%04x\n",vtx86_cfg.pwm_config_base_io);

#if 0
    vtx86_pinMode(3,VTX86_OUTPUT);
    {
        unsigned int msk = 1u << VTX86_PWM_RESOLUTION;
        unsigned int i;
        unsigned int v;
        unsigned int wv;
        double d;

        for (i=0;i < 10000u;i++) {
            d = ((double)i * msk * 10) / 10000;
            v = ((unsigned int)d) & (msk - 1u);
            wv=  (unsigned int)floor(d / msk);
            if (wv & 1u) v ^= msk - 1u;

            vtx86_analogWrite(3,v);

            t8254_wait(t8254_us2ticks(1000));
        }
    }
#endif

    return 0;
}

