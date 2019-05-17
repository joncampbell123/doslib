
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

