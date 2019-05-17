
#ifndef __DOSLIB_HW_86DUINO_86DUINO_H
#define __DOSLIB_HW_86DUINO_86DUINO_H

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

#define VTX86_GPIO_PINS         (45)

#define VTX86_ADC_RESOLUTION      (11u) // for 86Duino
#define VTX86_PWM_RESOLUTION      (13u) // for 86duino

#define NOPWM    (-1)

#define MCPWM_RELOAD_CANCEL         (0x00<<4)
#define MCPWM_RELOAD_PEREND         (0x01<<4)
#define MCPWM_RELOAD_PERCE          (0x02<<4)
#define MCPWM_RELOAD_SCEND          (0x03<<4)
#define MCPWM_RELOAD_SIFTRIG        (0x05<<4)
#define MCPWM_RELOAD_EVT            (0x06<<4)
#define MCPWM_RELOAD_NOW            (0x07<<4)

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

/* northbridge 0:0:0 */
#define VORTEX86_PCI_NB_BUS         (0)
#define VORTEX86_PCI_NB_DEV         (0)
#define VORTEX86_PCI_NB_FUNC        (0)

uint8_t vtx86_nb_readb(const uint8_t reg);
uint16_t vtx86_nb_readw(const uint8_t reg);
uint32_t vtx86_nb_readl(const uint8_t reg);
void vtx86_nb_writeb(const uint8_t reg,const uint8_t val);
void vtx86_nb_writew(const uint8_t reg,const uint16_t val);
void vtx86_nb_writel(const uint8_t reg,const uint32_t val);

/* southbridge 0:7:0 */
#define VORTEX86_PCI_SB_BUS         (0)
#define VORTEX86_PCI_SB_DEV         (7)
#define VORTEX86_PCI_SB_FUNC        (0)

uint8_t vtx86_sb_readb(const uint8_t reg);
uint16_t vtx86_sb_readw(const uint8_t reg);
uint32_t vtx86_sb_readl(const uint8_t reg);
void vtx86_sb_writeb(const uint8_t reg,const uint8_t val);
void vtx86_sb_writew(const uint8_t reg,const uint16_t val);
void vtx86_sb_writel(const uint8_t reg,const uint32_t val);

/* southbridge 0:7:1 */
#define VORTEX86_PCI_SB1_BUS        (0)
#define VORTEX86_PCI_SB1_DEV        (7)
#define VORTEX86_PCI_SB1_FUNC       (1)

uint8_t vtx86_sb1_readb(const uint8_t reg);
uint16_t vtx86_sb1_readw(const uint8_t reg);
uint32_t vtx86_sb1_readl(const uint8_t reg);
void vtx86_sb1_writeb(const uint8_t reg,const uint8_t val);
void vtx86_sb1_writew(const uint8_t reg,const uint16_t val);
void vtx86_sb1_writel(const uint8_t reg,const uint32_t val);

/* MC 0:10:0 */
#define VORTEX86_PCI_MC_BUS         (0)
#define VORTEX86_PCI_MC_DEV         (0x10)
#define VORTEX86_PCI_MC_FUNC        (0)

uint8_t vtx86_mc_readb(const uint8_t reg);
uint16_t vtx86_mc_readw(const uint8_t reg);
uint32_t vtx86_mc_readl(const uint8_t reg);
void vtx86_mc_writeb(const uint8_t reg,const uint8_t val);
void vtx86_mc_writew(const uint8_t reg,const uint16_t val);
void vtx86_mc_writel(const uint8_t reg,const uint32_t val);

void vtx86_mc_outp(const int mc, const uint32_t idx, const uint32_t val);
uint32_t vtx86_mc_inp(const int mc, const uint32_t idx);
void vtx86_mc_outpb(const int mc, const uint32_t idx, const unsigned char val);
unsigned char vtx86_mc_inpb(const int mc, const uint32_t idx);

extern uint8_t                              vtx86_mc_md_inuse[VTX86_GPIO_PINS];
extern const int8_t                         vtx86_arduino_to_mc_md[2/*MC/MD*/][45/*PINS*/];

struct vtx86_cfg_t {
    uint16_t                                uart_config_base_io;
    uint16_t                                gpio_intconfig_base_io;
    uint16_t                                gpio_portconfig_base_io;
    uint16_t                                crossbar_config_base_io;
    uint16_t                                pwm_config_base_io;
    uint16_t                                adc_config_base_io;
};

/* gpio_portconfig */
struct vtx86_gpio_port_cfg_pin_t {
    uint16_t                                dir_io;
    uint16_t                                data_io;
};

struct vtx86_gpio_port_cfg_t {
    uint32_t                                gpio_dec_enable;
    struct vtx86_gpio_port_cfg_pin_t        gpio_pingroup[10];
};

extern unsigned char                        vtx86_86duino_flags;
#define VTX86_86DUINO_FLAG_SB1              (1u << 0u)
#define VTX86_86DUINO_FLAG_MC               (1u << 1u)
#define VTX86_86DUINO_FLAG_DETECTED         (1u << 2u)

extern struct vtx86_cfg_t                   vtx86_cfg;
extern struct vtx86_gpio_port_cfg_t         vtx86_gpio_port_cfg;

extern const uint8_t                        vtx86_gpio_to_crossbar_pin_map[45/*PINS*/];

int                                         probe_vtx86(void);
int                                         read_vtx86_config(void);
int                                         read_vtx86_gpio_pin_config(void);

uint8_t                                     vtx86_mc_IsEnabled(const int mc);
void                                        vtx86_mcpwm_Enable(const uint8_t mc, const uint8_t module);
void                                        vtx86_mcpwm_Disable(const uint8_t mc, const uint8_t module);
void                                        vtx86_mcpwm_DisableProtect(const int mc, const uint8_t module);
void                                        vtx86_Close_Pwm(const uint8_t pin);
void                                        vtx86_mcpwm_SetDeadband(const int mc, const int module, const uint32_t db);
void                                        vtx86_mcpwm_ReloadPWM(const int mc, const int module, const unsigned char mode);
void                                        vtx86_mcpwm_ReloadEVT(const int mc, const int module, const unsigned char mode);
void                                        vtx86_mcpwm_ReloadOUT(const int mc, const int module, const unsigned char mode);
void                                        vtx86_mcpwm_ReloadOUT_Unsafe(const int mc, const int module, const unsigned char mode);
void                                        mcspwm_ReloadOUT_Unsafe(const int mc, const unsigned char mode);
void                                        vtx86_mcpwm_SetOutMask(const int mc, const int module, const uint32_t mask);
void                                        vtx86_mcpwm_SetOutPolarity(const int mc, const int module, const uint32_t pol);
void                                        vtx86_mcpwm_SetMPWM(const int mc, const int module, const uint32_t mpwm);
void                                        vtx86_mcpwm_SetWaveform(const int mc, const int module, const uint32_t mode);
void                                        vtx86_mcpwm_SetWidth(const int mc, const int module, const uint32_t period, const uint32_t phase0);
void                                        vtx86_mcpwm_SetSamplCycle(const int mc, const int module, const uint32_t sc);

void                                        vtx86_pinMode(const uint8_t pin, const uint8_t mode);

void                                        vtx86_digitalWrite(const uint8_t pin,const uint8_t val);
unsigned int                                vtx86_digitalRead(const uint8_t pin);

int                                         vtx86_init_adc(void);
void                                        vtx86_analogWrite(const uint8_t pin, const uint16_t val);
uint16_t                                    vtx86_analogRead(const uint8_t pin);

static inline unsigned int vtx86_analogFIFO_Pending(void) {
    return (inp(vtx86_cfg.adc_config_base_io + 2u) & 1u); /* data ready in FIFO */
}

static inline uint16_t vtx86_analogFIFO_ReadRaw(void) {
    return inpw(vtx86_cfg.adc_config_base_io + 4u); /* ADC data register   bits [15:13] indicate which channel, bits [10:0] contain the value */
}

static inline uint16_t vtx86_analogFIFO_Read(void) {
    return vtx86_analogFIFO_ReadRaw() & 0x7FFu;
}

#endif //__DOSLIB_HW_86DUINO_86DUINO_H

