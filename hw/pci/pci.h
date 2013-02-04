
#include <hw/cpu/cpu.h>
#include <stdint.h>

/* there are 3 methods supported by this library
 * (FIXME: Well, I don't have code to support Configuration Type 2 because I don't have hardware that speaks that...) */
enum {
	PCI_CFG_NONE=0,
	PCI_CFG_TYPE1,		/* Configuration Type 1 (most common) */
	PCI_CFG_BIOS,		/* Use the BIOS (PCI 2.0c interface) */
	PCI_CFG_BIOS1,		/* Use the BIOS (PCI 1.x interface) (FIXME: I don't have any hardware who's BIOS implements this) */
	PCI_CFG_TYPE2,		/* Configuration Type 2 (FIXME: I don't have any hardware that emulates or supports this method) */
	PCI_CFG_MAX
};

#define pci_read_cfgl(b,c,f,r) pci_read_cfg_array[pci_cfg](b,c,f,r,2)
#define pci_read_cfgw(b,c,f,r) pci_read_cfg_array[pci_cfg](b,c,f,r,1)
#define pci_read_cfgb(b,c,f,r) pci_read_cfg_array[pci_cfg](b,c,f,r,0)

#define pci_write_cfgl(b,c,f,r,d) pci_write_cfg_array[pci_cfg](b,c,f,r,d,2)
#define pci_write_cfgw(b,c,f,r,d) pci_write_cfg_array[pci_cfg](b,c,f,r,d,1)
#define pci_write_cfgb(b,c,f,r,d) pci_write_cfg_array[pci_cfg](b,c,f,r,d,0)

extern unsigned char			pci_cfg;
extern unsigned char			pci_cfg_probed;
extern uint32_t				pci_bios_protmode_entry_point;
extern uint8_t				pci_bios_hw_characteristics;
extern uint16_t				pci_bios_interface_level;
extern uint8_t				pci_bus_decode_bits;
extern int16_t				pci_bios_last_bus;

void pci_type1_select(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg);
void pci_type2_select(uint8_t bus,uint8_t func);
uint32_t pci_read_cfg_TYPE1(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint8_t size);
uint32_t pci_read_cfg_TYPE2(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint8_t size);
uint32_t pci_read_cfg_BIOS(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint8_t size);
uint32_t pci_read_cfg_NOTIMPL(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint8_t size);
void pci_write_cfg_TYPE1(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint32_t data,uint8_t size);
void pci_write_cfg_TYPE2(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint32_t data,uint8_t size);
void pci_write_cfg_BIOS(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint32_t data,uint8_t size);
void pci_write_cfg_NOTIMPL(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint32_t data,uint8_t size);
uint8_t pci_probe_device_functions(uint8_t bus,uint8_t dev);
void pci_probe_for_last_bus();
int pci_probe(int preference);

extern uint32_t (*pci_read_cfg_array[PCI_CFG_MAX])(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint8_t size);
extern void (*pci_write_cfg_array[PCI_CFG_MAX])(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint32_t data,uint8_t size);


