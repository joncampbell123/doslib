
#include <hw/cpu/cpu.h>
#include <stdint.h>

/* PCI Express configuration space is memory-mapped I/O, so the options
   cover the various ways 16-bit and 32-bit code can reach it */
enum {
	PCIE_CFG_NONE=0,
	PCIE_CFG_FLATREAL,		/* 16-bit: 386 flat real mode */
	PCIE_CFG_LLMEM,			/* 16/32:  llmem library */
	PCIE_CFG_PTR,			/* 32-bit: DOS extender put us in flat (nonpaged) 32-bit protected mode, just typecast a ptr */
	PCIE_CFG_MAX
};

/* Take PCI express bus/device/function/reg/offset pair and convert to offset within MMIO region */
static inline uint32_t pci_express_bdfero_to_offset(unsigned char bus,unsigned char device,unsigned char function,unsigned short reg,unsigned char offset) {
	return ((uint32_t)bus << 20UL) + ((uint32_t)device << 15UL) + ((uint32_t)function << 12UL) + ((uint32_t)reg << 2UL) + ((uint32_t)offset);
}

#define pcie_read_cfgq(b,c,f,r) pcie_read_cfg_arrayq[pcie_cfg](b,c,f,r)
#define pcie_read_cfgl(b,c,f,r) pcie_read_cfg_array[pcie_cfg](b,c,f,r,2)
#define pcie_read_cfgw(b,c,f,r) pcie_read_cfg_array[pcie_cfg](b,c,f,r,1)
#define pcie_read_cfgb(b,c,f,r) pcie_read_cfg_array[pcie_cfg](b,c,f,r,0)

#define pcie_write_cfgq(b,c,f,r,d) pcie_write_cfg_arrayq[pcie_cfg](b,c,f,r,d)
#define pcie_write_cfgl(b,c,f,r,d) pcie_write_cfg_array[pcie_cfg](b,c,f,r,d,2)
#define pcie_write_cfgw(b,c,f,r,d) pcie_write_cfg_array[pcie_cfg](b,c,f,r,d,1)
#define pcie_write_cfgb(b,c,f,r,d) pcie_write_cfg_array[pcie_cfg](b,c,f,r,d,0)

extern unsigned char			pcie_cfg;
extern unsigned char			pcie_cfg_probed;
extern uint8_t				pcie_bus_decode_bits;
extern uint8_t				pcie_bios_last_bus;
extern uint64_t				pcie_acpi_mcfg_table;
extern uint64_t				pcie_bus_mmio_base[256];

uint8_t pcie_probe_device_functions(uint8_t bus,uint8_t dev);
void pcie_probe_for_last_bus();
int pcie_probe(int preference);

extern uint32_t (*pcie_read_cfg_array[PCIE_CFG_MAX])(uint8_t bus,uint8_t card,uint8_t func,uint16_t reg,uint8_t size);
extern void (*pcie_write_cfg_array[PCIE_CFG_MAX])(uint8_t bus,uint8_t card,uint8_t func,uint16_t reg,uint32_t data,uint8_t size);

extern uint64_t (*pcie_read_cfg_arrayq[PCIE_CFG_MAX])(uint8_t bus,uint8_t card,uint8_t func,uint16_t reg);
extern void (*pcie_write_cfg_arrayq[PCIE_CFG_MAX])(uint8_t bus,uint8_t card,uint8_t func,uint16_t reg,uint64_t data);

