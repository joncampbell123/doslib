 
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
#include <hw/cpu/cpu.h>
#include <hw/86duino/86duino.h>

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

