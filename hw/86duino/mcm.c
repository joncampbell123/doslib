 
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
#include <hw/86duino/mcm.h>

uint8_t vtx86_mc_md_inuse[VTX86_GPIO_PINS] = {0};

static unsigned int vtx86_MCPWM_modOffset[3] = {0x08u, 0x08u + 0x2cu, 0x08u + 2u*0x2cu};

const int8_t vtx86_arduino_to_mc_md[2/*MC/MD*/][45/*PINS*/] = {
    // MC
    {NOPWM,         NOPWM,          NOPWM,          MC_MODULE3,
     NOPWM,         MC_MODULE0,     MC_MODULE0,     NOPWM,
     NOPWM,         MC_MODULE0,     MC_MODULE1,     MC_MODULE1,
     NOPWM,         MC_MODULE1,     NOPWM,          NOPWM,
     NOPWM,         NOPWM,          NOPWM,          NOPWM,
     NOPWM,         NOPWM,          NOPWM,          NOPWM,
     NOPWM,         NOPWM,          NOPWM,          NOPWM,
     NOPWM,         MC_MODULE2,     MC_MODULE2,     MC_MODULE2,
     MC_MODULE3,    NOPWM,          NOPWM,          NOPWM,
     NOPWM,         NOPWM,          NOPWM,          NOPWM,
     NOPWM,         NOPWM,          NOPWM,          NOPWM,
     NOPWM},
    // MD
    {NOPWM,         NOPWM,          NOPWM,          MCPWM_MODULEA,
     NOPWM,         MCPWM_MODULEC,  MCPWM_MODULEB,  NOPWM,
     NOPWM,         MCPWM_MODULEA,  MCPWM_MODULEC,  MCPWM_MODULEB,
     NOPWM,         MCPWM_MODULEA,  NOPWM,          NOPWM,
     NOPWM,         NOPWM,          NOPWM,          NOPWM,
     NOPWM,         NOPWM,          NOPWM,          NOPWM,
     NOPWM,         NOPWM,          NOPWM,          NOPWM,
     NOPWM,         MCPWM_MODULEA,  MCPWM_MODULEB,  MCPWM_MODULEC,
     MCPWM_MODULEB, NOPWM,          NOPWM,          NOPWM,
     NOPWM,         NOPWM,          NOPWM,          NOPWM,
     NOPWM,         NOPWM,          NOPWM,          NOPWM,
     NOPWM}
    };

uint8_t vtx86_mc_IsEnabled(const int mc) {
    const uint32_t reg1 = vtx86_mc_inp(MC_GENERAL, MCG_ENABLEREG1);
    const uint32_t reg2 = vtx86_mc_inp(MC_GENERAL, MCG_ENABLEREG2);
    unsigned int i;

    if ((reg2 & (1L << (mc+4))) != 0L) return 1;     // check SIFBEN

    for (i=0; i<3; i++)  // check PWMAEN/SERVOAEN/SIFAEN, PWMBEN/SERVOBEN, PWMCEN/SERVOCEN
    {
        int tmp = mc*3 + i;
        if (tmp <  32 && ((reg1 & (0x01L << tmp)) != 0L)) return 1;
        if (tmp >= 32 && ((reg2 & (0x01L << (tmp-32u))) != 0L)) return 1;
    }

    return 0;
}

void vtx86_mcpwm_Enable(const uint8_t mc, const uint8_t module) {
    const uint32_t val  = 1L << ((mc*3 + module) & 31);
    const uint32_t reg  = ((mc*3 + module) < 32)? MCG_ENABLEREG1 : MCG_ENABLEREG2;

    vtx86_mc_outp(MC_GENERAL, reg, vtx86_mc_inp(MC_GENERAL, reg) | val);
}

void vtx86_mcpwm_Disable(const uint8_t mc, const uint8_t module) {
    const uint32_t val  = 1L << ((mc*3 + module) & 31);
    const uint32_t reg  = ((mc*3 + module) < 32)? MCG_ENABLEREG1 : MCG_ENABLEREG2;

    vtx86_mc_outp(MC_GENERAL, reg, vtx86_mc_inp(MC_GENERAL, reg) & ~val);
}

void vtx86_mcpwm_DisableProtect(const int mc, const uint8_t module) {
    uint32_t val = ~(1L << ((mc*3 + module) & 31));
    uint32_t reg = ((mc*3 + module) < 32)? MCG_PROTECTREG1 : MCG_PROTECTREG2;

    val = vtx86_mc_inp(MC_GENERAL, reg) & val;
    if ((mc*3 + module) < 32)  // unlock the write protection register
    {
        vtx86_mc_outp(MC_GENERAL, MCG_UNLOCKREG1, 0x000055aaL);
        vtx86_mc_outp(MC_GENERAL, MCG_UNLOCKREG1, 0x0000aa55L);
    }
    else
    {
        vtx86_mc_outp(MC_GENERAL, MCG_UNLOCKREG1, 0x55aa0000L);
        vtx86_mc_outp(MC_GENERAL, MCG_UNLOCKREG1, 0xaa550000L);
    }

    vtx86_mc_outp(MC_GENERAL, reg, val);
}

void vtx86_Close_Pwm(const uint8_t pin) {
    int8_t mc, md;

    if (pin >= sizeof(vtx86_gpio_to_crossbar_pin_map)) return;

    mc = vtx86_arduino_to_mc_md[VTX86_MCM_MC][pin];
    md = vtx86_arduino_to_mc_md[VTX86_MCM_MD][pin];

    SAVE_CPUFLAGS( _cli() ) {
        if (vtx86_mc_md_inuse[pin] == 1) {
            vtx86_mcpwm_Disable(mc, md);
            vtx86_mc_md_inuse[pin] = 0;
        }
    } RESTORE_CPUFLAGS();
}

void vtx86_mcpwm_SetDeadband(const int mc, const int module, const uint32_t db) {
    uint32_t reg = vtx86_MCPWM_modOffset[module] + MCPWM_OUTCTRLREG;

    vtx86_mc_outp(mc, reg, (vtx86_mc_inp(mc, reg) & 0xff000000L) | (db & 0x00ffffffL));
}

void vtx86_mcpwm_ReloadPWM(const int mc, const int module, const unsigned char mode) {
    vtx86_mc_outpb(mc, vtx86_MCPWM_modOffset[module] + MCPWM_LDREG + 3L, mode);
}

void vtx86_mcpwm_ReloadEVT(const int mc, const int module, const unsigned char mode) {
    vtx86_mc_outpb(mc, vtx86_MCPWM_modOffset[module] + MCPWM_LDREG + 1L, mode);
}

void vtx86_mcpwm_ReloadOUT(const int mc, const int module, const unsigned char mode) {
    vtx86_mc_outpb(mc, vtx86_MCPWM_modOffset[module] + MCPWM_LDREG + 2L, mode);
}

void vtx86_mcpwm_ReloadOUT_Unsafe(const int mc, const int module, const unsigned char mode) {
    vtx86_mcpwm_DisableProtect(mc, module);
    vtx86_mc_outpb(mc, vtx86_MCPWM_modOffset[module] + MCPWM_LDREG + 2L, mode);
}

void mcspwm_ReloadOUT_Unsafe(const int mc, const unsigned char mode) {
    vtx86_mcpwm_DisableProtect(mc, MCPWM_MODULEA);
    vtx86_mcpwm_DisableProtect(mc, MCPWM_MODULEB);
    vtx86_mcpwm_DisableProtect(mc, MCPWM_MODULEC);
    vtx86_mc_outpb(mc, vtx86_MCPWM_modOffset[MCPWM_MODULEA] + MCPWM_LDREG + 2L, mode);
}

void vtx86_mcpwm_SetOutMask(const int mc, const int module, const uint32_t mask) {
    const uint32_t reg = vtx86_MCPWM_modOffset[module] + MCPWM_OUTCTRLREG;

    vtx86_mc_outp(mc, reg, (vtx86_mc_inp(mc, reg) & 0xf0ffffffL) | (mask & 0x0f000000L));
}

void vtx86_mcpwm_SetOutPolarity(const int mc, const int module, const uint32_t pol) {
    const uint32_t reg = vtx86_MCPWM_modOffset[module] + MCPWM_OUTCTRLREG;

    vtx86_mc_outp(mc, reg, (vtx86_mc_inp(mc, reg) & 0xcfffffffL) | (pol & 0x30000000L));
}

void vtx86_mcpwm_SetMPWM(const int mc, const int module, const uint32_t mpwm) {
    const uint32_t reg = vtx86_MCPWM_modOffset[module] + MCPWM_OUTCTRLREG;

    vtx86_mc_outp(mc, reg, (vtx86_mc_inp(mc, reg) & 0x3fffffffL) | (mpwm & 0xc0000000L));
}

void vtx86_mcpwm_SetWaveform(const int mc, const int module, const uint32_t mode) {
    const uint32_t reg = vtx86_MCPWM_modOffset[module] + MCPWM_CTRLREG;

    vtx86_mc_outp(mc, reg, (vtx86_mc_inp(mc, reg) & 0x3fffffffL) | mode);
}

void vtx86_mcpwm_SetWidth(const int mc, const int module, const uint32_t period, const uint32_t phase0) {
    const uint32_t reg = vtx86_MCPWM_modOffset[module];

    vtx86_mc_outp(mc, reg + MCPWM_PERREG, period);
    vtx86_mc_outp(mc, reg + MCPWM_P0REG,  phase0);
}

void vtx86_mcpwm_SetSamplCycle(const int mc, const int module, const uint32_t sc) {
    const uint32_t reg = vtx86_MCPWM_modOffset[module] + MCPWM_CTRLREG;

    vtx86_mc_outp(mc, reg, (vtx86_mc_inp(mc, reg) & 0xe0000000L) | (sc & 0x1fffffffL));
}

