 
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

#define VTX86_SYSCLK            (100)

#define VTX86_INPUT             (0x00)
#define VTX86_OUTPUT            (0x01)
#define VTX86_INPUT_PULLUP      (0x02)
#define VTX86_INPUT_PULLDOWN    (0x03)

#define VTX86_LOW               (0x00)
#define VTX86_HIGH              (0x01)
#define VTX86_CHANGE            (0x02)
#define VTX86_FALLING           (0x03)
#define VTX86_RISING            (0x04)

#define VTX86_MCM_MC            (0)
#define VTX86_MCM_MD            (1)

static unsigned int vtx86_MCPWM_modOffset[3] = {0x08u, 0x08u + 0x2cu, 0x08u + 2u*0x2cu};

uint8_t vtx86_mc_md_inuse[45/*PINS*/] = {0};

const int8_t vtx86_uart_IRQs[16] = {
    -1, 9, 3,10,
     4, 5, 7, 6,
     1,11,-1,12,
    -1,14,-1,15
};

const uint8_t vtx86_gpio_to_crossbar_pin_map[45/*PINS*/] =
   {11, 10, 39, 23, 37, 20, 19, 35,
    33, 17, 28, 27, 32, 25, 12, 13,
    14, 15, 24, 26, 29, 47, 46, 45,
    44, 43, 42, 41, 40,  1,  3,  4,
    31,  0,  2,  5, 22, 30,  6, 38,
    36, 34, 16, 18, 21};

// MC no.
#define MC_GENERAL          (-1)
#define MC_MODULE0          (0)
#define MC_MODULE1          (1)
#define MC_MODULE2          (2)
#define MC_MODULE3          (3)
// #define MC_MODULE4          (4)
// #define MC_MODULE5          (5)
// #define MC_MODULE6          (6)
// #define MC_MODULE7          (7)
// #define MC_MODULE8          (8)
// #define MC_MODULE9          (9)
// #define MC_MODULE10         (10)
// #define MC_MODULE11         (11)

// Module no.
#define MCPWM_MODULEA       (0)
#define MCPWM_MODULEB       (1)
#define MCPWM_MODULEC       (2)
#define MCSERVO_MODULEA     (0)
#define MCSERVO_MODULEB     (1)
#define MCSERVO_MODULEC     (2)
#define MCSIF_MODULEA       (0)
#define MCSIF_MODULEB       (1)

#define NOPWM    (-1)
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
    struct vtx86_gpio_port_cfg_pin_t        gpio_pingroup[10];
};

unsigned char                   vtx86_86duino_flags = 0;
#define VTX86_86DUINO_FLAG_SB1          (1u << 0u)
#define VTX86_86DUINO_FLAG_MC           (1u << 1u)
#define VTX86_86DUINO_FLAG_DETECTED     (1u << 2u)

struct vtx86_cfg_t              vtx86_cfg = {0};
struct vtx86_gpio_port_cfg_t    vtx86_gpio_port_cfg = {0};

// MC General Registers
#define MCG_MODEREG1        (0x00L)
#define MCG_MODEREG2        (0x04L)
#define MCG_ENABLEREG1      (0x08L)
#define MCG_ENABLEREG2      (0x0cL)
#define MCG_PROTECTREG1     (0x10L)
#define MCG_PROTECTREG2     (0x14L)
#define MCG_LDRDYSYNCREG1   (0x18L)
#define MCG_LDRDYSYNCREG2   (0x1cL)
#define MCG_G1ROUTEREG1     (0x20L)
#define MCG_G1ROUTEREG2     (0x24L)
#define MCG_G2ROUTEREG1     (0x28L)
#define MCG_G2ROUTEREG2     (0x2cL)
#define MCG_G3ROUTEREG1     (0x30L)
#define MCG_G3ROUTEREG2     (0x34L)
#define MCG_INTMASKREG      (0x38L)     // 16-bit
#define MCG_INTSTATREG      (0x3aL)     // 16-bit
#define MCG_UNLOCKREG1      (0x3cL)     // 16-bit
#define MCG_UNLOCKREG2      (0x3eL)     // 16-bit

#define MCPWM_RELOAD_CANCEL         (0x00<<4)
#define MCPWM_RELOAD_PEREND         (0x01<<4)
#define MCPWM_RELOAD_PERCE          (0x02<<4)
#define MCPWM_RELOAD_SCEND          (0x03<<4)
#define MCPWM_RELOAD_SIFTRIG        (0x05<<4)
#define MCPWM_RELOAD_EVT            (0x06<<4)
#define MCPWM_RELOAD_NOW            (0x07<<4)

#define VTX86_ADC_RESOLUTION      (11u) // for 86Duino
#define VTX86_PWM_RESOLUTION      (13u) // for 86duino

#define MCPWM_HPOL_NORMAL       (0x00L << 29)
#define MCPWM_HPOL_INVERSE      (0x01L << 29)
#define MCPWM_LPOL_NORMAL       (0x00L << 28)
#define MCPWM_LPOL_INVERSE      (0x01L << 28)

#define MCPWM_MPWM_DISABLE      (0x00L << 30)
#define MCPWM_MPWM_INACTIVE     (0x02L << 30)
#define MCPWM_MPWM_ACTIVE       (0x03L << 30)

#define MCPWM_EVT_DISABLE           (0x00L)
#define MCPWM_EVT_PERSC             (0x01L)
#define MCPWM_EVT_PERPWM            (0x02L)
#define MCPWM_EVT_PERPWM2           (0x03L)

#define MCPWM_HMASK_NONE        (0x00L << 26)
#define MCPWM_HMASK_INACTIVE    (0x01L << 26)
#define MCPWM_HMASK_TRISTATE    (0x03L << 26)
#define MCPWM_LMASK_NONE        (0x00L << 24)
#define MCPWM_LMASK_INACTIVE    (0x01L << 24)
#define MCPWM_LMASK_TRISTATE    (0x03L << 24)

#define MCPWM_EDGE_I0A1     (0x00L << 30)
#define MCPWM_EDGE_A0I1     (0x02L << 30)
#define MCPWM_CENTER_I0A1   (0x01L << 30)
#define MCPWM_CENTER_A0I1   (0x03L << 30)

#define MCPFAU_CAP_1TO0EDGE          (0x00L << 28)
#define MCPFAU_CAP_0TO1EDGE          (0x02L << 28)
#define MCPFAU_CAP_CAPCNT_OVERFLOW   (0x08L << 28)
#define MCPFAU_CAP_FIFOEMPTY         (0x0fL << 28)

// MC PWM Registers
#define MCPWM_PERREG        (0x00L)
#define MCPWM_P0REG         (0x04L)
#define MCPWM_CTRLREG       (0x08L)
#define MCPWM_STATREG1      (0x0cL)
#define MCPWM_STATREG2      (0x10L)
#define MCPWM_EVTREG1       (0x14L)
#define MCPWM_EVTREG2       (0x18L)
#define MCPWM_OUTCTRLREG    (0x1cL)
#define MCPWM_FOUTREG       (0x24L)
#define MCPWM_LDREG         (0x28L)

void vtx86_mc_outp(const int mc, const uint32_t idx, const uint32_t val) {
    unsigned int cpu_flags;

    cpu_flags = get_cpu_flags();
    _cli();

    outpd(vtx86_cfg.pwm_config_base_io + 0xd0, 1UL<<(mc+1));  // paging to corresponding reg window 
    outpd(vtx86_cfg.pwm_config_base_io + (unsigned)idx, val);

    _sti_if_flags(cpu_flags);
}

uint32_t vtx86_mc_inp(const int mc, const uint32_t idx) {
    unsigned int cpu_flags;
    uint32_t tmp;

    cpu_flags = get_cpu_flags();
    _cli();

    outpd(vtx86_cfg.pwm_config_base_io + 0xd0, 1UL<<(mc+1));  // paging to corresponding reg window 
    tmp = inpd(vtx86_cfg.pwm_config_base_io + (unsigned)idx);

    _sti_if_flags(cpu_flags);

    return tmp;
}

void vtx86_mc_outpb(const int mc, const uint32_t idx, const unsigned char val) {
    unsigned int cpu_flags;

    cpu_flags = get_cpu_flags();
    _cli();

    outpd(vtx86_cfg.pwm_config_base_io + 0xd0, 1UL<<(mc+1));  // paging to corresponding reg window 
    outp(vtx86_cfg.pwm_config_base_io + (unsigned)idx, val);

    _sti_if_flags(cpu_flags);
}

unsigned char vtx86_mc_inpb(const int mc, const uint32_t idx) {
    unsigned int cpu_flags;
    unsigned char tmp;

    cpu_flags = get_cpu_flags();
    _cli();

    outpd(vtx86_cfg.pwm_config_base_io + 0xd0, 1UL<<(mc+1));  // paging to corresponding reg window 
    tmp = inp(vtx86_cfg.pwm_config_base_io + (unsigned)idx);

    _sti_if_flags(cpu_flags);
    return tmp;
}

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
    unsigned int cpu_flags;
    int8_t mc, md;

    if (pin >= sizeof(vtx86_gpio_to_crossbar_pin_map)) return;

    mc = vtx86_arduino_to_mc_md[VTX86_MCM_MC][pin];
    md = vtx86_arduino_to_mc_md[VTX86_MCM_MD][pin];

    cpu_flags = get_cpu_flags();
    _cli();

    if (vtx86_mc_md_inuse[pin] == 1) {
        vtx86_mcpwm_Disable(mc, md);
        vtx86_mc_md_inuse[pin] = 0;
    }

    _sti_if_flags(cpu_flags);
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

void vtx86_digitalWrite(const uint8_t pin,const uint8_t val) {
    unsigned char crossbar_bit;
    unsigned char cbio_bitmask;
    unsigned int cpu_flags;
    uint16_t dport;

    if (pin >= sizeof(vtx86_gpio_to_crossbar_pin_map)) return;

    crossbar_bit = vtx86_gpio_to_crossbar_pin_map[pin];
    dport = vtx86_gpio_port_cfg.gpio_pingroup[crossbar_bit / 8u].data_io;
    if (dport == 0) return;

    cbio_bitmask = 1u << (crossbar_bit % 8u);

    if (crossbar_bit >= 32u)
        outp(vtx86_cfg.crossbar_config_base_io + 0x80 + (crossbar_bit / 8u), 0x01);
    else {
        outp(vtx86_cfg.crossbar_config_base_io + 0x90 + crossbar_bit, 0x01);
        vtx86_Close_Pwm(pin);
    }

    cpu_flags = get_cpu_flags();
    _cli();

    if (val == VTX86_LOW)
        outp(dport, inp(dport) & (~cbio_bitmask));
    else
        outp(dport, inp(dport) |   cbio_bitmask );

    _sti_if_flags(cpu_flags);
}

unsigned int vtx86_digitalRead(const uint8_t pin) {
    unsigned char crossbar_bit;
    unsigned char cbio_bitmask;
    uint16_t dport;

    if (pin >= sizeof(vtx86_gpio_to_crossbar_pin_map)) return ~0u;

    crossbar_bit = vtx86_gpio_to_crossbar_pin_map[pin];
    dport = vtx86_gpio_port_cfg.gpio_pingroup[crossbar_bit / 8u].data_io;
    if (dport == 0) return ~0u;

    cbio_bitmask = 1u << (crossbar_bit % 8u);

    if (crossbar_bit >= 32u)
        outp(vtx86_cfg.crossbar_config_base_io + 0x80 + (crossbar_bit / 8u), 0x01);
    else
        outp(vtx86_cfg.crossbar_config_base_io + 0x90 + crossbar_bit, 0x01);

    return (inp(dport) & cbio_bitmask) ? VTX86_HIGH : VTX86_LOW;
}

void vtx86_pinMode(const uint8_t pin, const uint8_t mode) {
#define VTX86_PINMODE_TRI_STATE         (0x00)
#define VTX86_PINMODE_PULL_UP           (0x01)
#define VTX86_PINMODE_PULL_DOWN         (0x02)

    unsigned char crossbar_bit;
    unsigned char cbio_bitmask;
    unsigned int cpu_flags;
    uint16_t dbport;

    if (pin >= sizeof(vtx86_gpio_to_crossbar_pin_map)) return;

    crossbar_bit = vtx86_gpio_to_crossbar_pin_map[pin];
    dbport = vtx86_gpio_port_cfg.gpio_pingroup[crossbar_bit / 8u].dir_io;
    if (dbport == 0) return;

    cbio_bitmask = 1u << (crossbar_bit % 8u);

    cpu_flags = get_cpu_flags();
    _cli();

    if (mode == VTX86_INPUT)
    {
        outp(vtx86_cfg.crossbar_config_base_io + 0x30 + crossbar_bit, VTX86_PINMODE_TRI_STATE);
        outp(dbport, inp(dbport) & (~cbio_bitmask));
    }
    else if(mode == VTX86_INPUT_PULLDOWN)
    {
        outp(vtx86_cfg.crossbar_config_base_io + 0x30 + crossbar_bit, VTX86_PINMODE_PULL_DOWN);
        outp(dbport, inp(dbport) & (~cbio_bitmask));
    }
    else if (mode == VTX86_INPUT_PULLUP)
    {
        outp(vtx86_cfg.crossbar_config_base_io + 0x30 + crossbar_bit, VTX86_PINMODE_PULL_UP);
        outp(dbport, inp(dbport) & (~cbio_bitmask));
    }
    else {
        outp(dbport, inp(dbport) |   cbio_bitmask );
    }

    _sti_if_flags(cpu_flags);

#undef VTX86_PINMODE_TRI_STATE
#undef VTX86_PINMODE_PULL_UP
#undef VTX86_PINMODE_PULL_DOWN
}

void vtx86_analogWrite(const uint8_t pin, const uint16_t val) {
    unsigned int cpu_flags;
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
        cpu_flags = get_cpu_flags();
        _cli();

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

        _sti_if_flags(cpu_flags);

        cpu_flags = get_cpu_flags();
        _cli();

        vtx86_mcpwm_SetWidth(mc, md, 1000L*VTX86_SYSCLK, val << (16u - VTX86_PWM_RESOLUTION));
        vtx86_mcpwm_ReloadPWM(mc, md, MCPWM_RELOAD_PEREND);
        if (vtx86_mc_md_inuse[pin] == 0) {
            vtx86_mcpwm_Enable(mc, md);
            outp(vtx86_cfg.crossbar_config_base_io + 0x90 + vtx86_gpio_to_crossbar_pin_map[pin], 0x08);
            vtx86_mc_md_inuse[pin] = 1;
        }

        _sti_if_flags(cpu_flags);
    }
}

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

int read_vtx86_gpio_pin_config(void) {
    unsigned int i;
    uint32_t tmp;

    if (!(vtx86_86duino_flags & VTX86_86DUINO_FLAG_DETECTED))
        return 0;

    vtx86_gpio_port_cfg.gpio_dec_enable = inpd(vtx86_cfg.gpio_portconfig_base_io + 0x00);
    for (i=0;i < 10;i++) {
        tmp = inpd(vtx86_cfg.gpio_portconfig_base_io + 0x04 + (i * 4u));
        vtx86_gpio_port_cfg.gpio_pingroup[i].data_io = (uint16_t)(tmp & 0xFFFFul);
        vtx86_gpio_port_cfg.gpio_pingroup[i].dir_io  = (uint16_t)(tmp >> 16ul);
    }

    return 1;
}

int read_vtx86_config(void) {
    if (!(vtx86_86duino_flags & VTX86_86DUINO_FLAG_DETECTED))
        return 0;

    vtx86_cfg.uart_config_base_io = vtx86_sb_readw(0x60);
    if (vtx86_cfg.uart_config_base_io & 1u)
        vtx86_cfg.uart_config_base_io &= ~1u;
    else
        vtx86_cfg.uart_config_base_io  = 0;

    vtx86_cfg.gpio_portconfig_base_io = vtx86_sb_readw(0x62);
    if (vtx86_cfg.gpio_portconfig_base_io & 1u)
        vtx86_cfg.gpio_portconfig_base_io &= ~1u;
    else
        vtx86_cfg.gpio_portconfig_base_io  = 0;

    vtx86_cfg.gpio_intconfig_base_io = vtx86_sb_readw(0x66);
    if (vtx86_cfg.gpio_intconfig_base_io & 1u)
        vtx86_cfg.gpio_intconfig_base_io &= ~1u;
    else
        vtx86_cfg.gpio_intconfig_base_io  = 0;

    vtx86_cfg.crossbar_config_base_io = vtx86_sb_readw(0x64);
    if (vtx86_cfg.crossbar_config_base_io & 1u)
        vtx86_cfg.crossbar_config_base_io &= ~1u;
    else
        vtx86_cfg.crossbar_config_base_io  = 0;

    if (vtx86_86duino_flags & VTX86_86DUINO_FLAG_SB1) {
        // NTS: Bit 0 is not set and has no effect on my 86Duino --J.C.
        vtx86_cfg.adc_config_base_io = vtx86_sb1_readw(0xE0) & (~1u);
    }
    else {
        vtx86_cfg.adc_config_base_io = 0;
    }

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

    return 1;
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

static inline unsigned int vtx86_analogFIFO_Pending(void) {
    return (inp(vtx86_cfg.adc_config_base_io + 2u) & 1u); /* data ready in FIFO */
}

static inline uint16_t vtx86_analogFIFO_ReadRaw(void) {
    return inpw(vtx86_cfg.adc_config_base_io + 4u); /* ADC data register   bits [15:13] indicate which channel, bits [10:0] contain the value */
}

static inline uint16_t vtx86_analogFIFO_Read(void) {
    return vtx86_analogFIFO_ReadRaw() & 0x7FFu;
}

uint16_t vtx86_analogRead(const uint8_t pin) {
    unsigned int cpu_flags;
    uint16_t r;

    if (pin >= 8) return (uint16_t)(~0u);

    /* drain FIFO */
    while (vtx86_analogFIFO_Pending())
        vtx86_analogFIFO_ReadRaw();

    cpu_flags = get_cpu_flags();
    _cli();

    outp(vtx86_cfg.adc_config_base_io + 1, 0x08);           // disable ADC
    outp(vtx86_cfg.adc_config_base_io + 0, 1u << pin);      // enable AUX scan for the pin we want
    outp(vtx86_cfg.adc_config_base_io + 1, 0x01);           // enable ADC, start ADC, one shot only

    while (!vtx86_analogFIFO_Pending());
    r = vtx86_analogFIFO_Read();

    _sti_if_flags(cpu_flags);

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

