
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

