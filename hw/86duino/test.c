 
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

void vtx86_analogWrite(const uint8_t pin, const uint16_t val) {
    unsigned int mc, md;

    if (pin >= 45/*PINS*/) return;
    vtx86_pinMode(pin, VTX86_OUTPUT);

    if (val == 0L)
        vtx86_digitalWrite(pin, VTX86_LOW);
    else if (val >= ((0x00000001L<<VTX86_PWM_RESOLUTION)-1ul))
        vtx86_digitalWrite(pin, VTX86_HIGH);
    else
    {
        mc = vtx86_arduino_to_mc_md[VTX86_MCM_MC][pin];
        md = vtx86_arduino_to_mc_md[VTX86_MCM_MD][pin];
        if(mc == NOPWM || md == NOPWM)
        {
            if (val < (0x00000001L<<(VTX86_PWM_RESOLUTION-1ul)))
                vtx86_digitalWrite(pin, VTX86_LOW);
            else
                vtx86_digitalWrite(pin, VTX86_HIGH);
            return;
        }

        // Init H/W PWM
        SAVE_CPUFLAGS( _cli() ) {
            if (vtx86_mc_md_inuse[pin] == 0) {
                vtx86_mcpwm_ReloadPWM(mc, md, MCPWM_RELOAD_CANCEL);
                vtx86_mcpwm_SetOutMask(mc, md, MCPWM_HMASK_NONE + MCPWM_LMASK_NONE);
                vtx86_mcpwm_SetOutPolarity(mc, md, MCPWM_HPOL_NORMAL + MCPWM_LPOL_NORMAL);
                vtx86_mcpwm_SetDeadband(mc, md, 0L);
                vtx86_mcpwm_ReloadOUT_Unsafe(mc, md, MCPWM_RELOAD_NOW);

                vtx86_mcpwm_SetWaveform(mc, md, MCPWM_EDGE_A0I1);
                vtx86_mcpwm_SetSamplCycle(mc, md, 1999L);   // sample cycle: 20ms

                vtx86_cfg.crossbar_config_base_io = vtx86_sb_readw(0x64)&0xfffe;
                if (pin <= 9)
                    outp(vtx86_cfg.crossbar_config_base_io + 2, 0x01); // GPIO port2: 0A, 0B, 0C, 3A
                else if (pin > 28)
                    outp(vtx86_cfg.crossbar_config_base_io, 0x03); // GPIO port0: 2A, 2B, 2C, 3C
                else
                    outp(vtx86_cfg.crossbar_config_base_io + 3, 0x02); // GPIO port3: 1A, 1B, 1C, 3B
            }

            vtx86_mcpwm_SetWidth(mc, md, 1000L*VTX86_SYSCLK, val << (16u - VTX86_PWM_RESOLUTION));
            vtx86_mcpwm_ReloadPWM(mc, md, MCPWM_RELOAD_PEREND);

            if (vtx86_mc_md_inuse[pin] == 0) {
                vtx86_mcpwm_Enable(mc, md);
                outp(vtx86_cfg.crossbar_config_base_io + 0x90 + vtx86_gpio_to_crossbar_pin_map[pin], 0x08);
                vtx86_mc_md_inuse[pin] = 1;
            }
        } RESTORE_CPUFLAGS();
    }
}

int vtx86_init_adc(void) {
    if (!(vtx86_86duino_flags & VTX86_86DUINO_FLAG_DETECTED))
        return 0;
    if (vtx86_cfg.adc_config_base_io == 0)
        return 0;

    // FIXME: I can't find in the datasheet the list of PCI configuration space registers
    //        that includes these mystery registers here. This is mystery I/O borrowed from
    //        the 86Duino core library code.
    vtx86_sb_writel(0xBC,vtx86_sb_readl(0xBC) & (~(1ul << 28ul)));      // activate ADC (??)
    vtx86_sb1_writew(0xDE,vtx86_sb1_readw(0xDE) | 0x02);                // ???
    vtx86_sb1_writel(0xE0,0x00500000ul | (unsigned long)vtx86_cfg.adc_config_base_io); // baseaddr and disable IRQ

    return 1;
}

uint16_t vtx86_analogRead(const uint8_t pin) {
    uint16_t r;

    if (pin >= 8) return (uint16_t)(~0u);

    /* drain FIFO */
    while (vtx86_analogFIFO_Pending())
        vtx86_analogFIFO_ReadRaw();

    /* TODO: Enforce a timeout, just as the 86Duino core code does.
     *       Perhaps there is a case where the ADC fails to function. */
    SAVE_CPUFLAGS( _cli() ) {
        outp(vtx86_cfg.adc_config_base_io + 1, 0x08);           // disable ADC
        outp(vtx86_cfg.adc_config_base_io + 0, 1u << pin);      // enable AUX scan for the pin we want
        outp(vtx86_cfg.adc_config_base_io + 1, 0x01);           // enable ADC, start ADC, one shot only

        while (!vtx86_analogFIFO_Pending());
        r = vtx86_analogFIFO_Read();
    } RESTORE_CPUFLAGS();

    return r;
}

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

