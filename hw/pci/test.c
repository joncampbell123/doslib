
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/pci/pci.h>
#include <hw/cpu/cpu.h>

static void help() {
	printf("test [options]\n");
	printf("Test PCI access library\n");
	printf("   /?                this help\n");
	printf("   /b                Prefer BIOS access\n");
	printf("   /b1               Prefer BIOS 1.x access\n");
	printf("   /t1               Prefer Type 1 (0xCF8-0xCFF) access\n");
	printf("   /t2               Prefer Type 2 (0xCxxx) access\n");
}

int main(int argc,char **argv) {
	int pref = -1,i;

	for (i=1;i < argc;) {
		char *a = argv[i++];

		if (*a == '-' || *a == '/') {
			do { a++; } while (*a == '-' || *a == '/');

			if (!strcmp(a,"h") || !strcmp(a,"help") || !strcmp(a,"?")) {
				help();
				return 1;
			}
			else if (!strcmp(a,"b")) {
				pref = PCI_CFG_BIOS;
			}
			else if (!strcmp(a,"b1")) {
				pref = PCI_CFG_BIOS1;
			}
			else if (!strcmp(a,"t1")) {
				pref = PCI_CFG_TYPE1;
			}
			else if (!strcmp(a,"t2")) {
				pref = PCI_CFG_TYPE2;
			}
			else {
				printf("Unknown switch '%s'\n",a);
				help();
				return 1;
			}
		}
		else {
			printf("Unknown arg '%s'\n",a);
			return 1;
		}
	}

	printf("PCI bus test code\n");

	cpu_probe();
	assert(pref == -1 || (pref >= 0 && pref < PCI_CFG_MAX)); /* CHECK: Bugfix for register corruption because cpu_probe() didn't save registers */
	if (cpu_basic_level < CPU_386) {
		printf("PCI programming requires a 386 or higher\n");
		return 1;
	}

	if (pci_probe(pref) == PCI_CFG_NONE) {
		printf("PCI bus not found\n");
		return 1;
	}
	printf("Found PCI BUS, code %d\n",pci_cfg);
	if (pci_cfg == PCI_CFG_BIOS) {
		printf("  32-bit entry point:       0x%08lx\n",(unsigned long)pci_bios_protmode_entry_point);
		printf("  Hardware charactistics:   0x%02x\n",(unsigned int)pci_bios_hw_characteristics);
		printf("  Interface level:          %x.%x\n",pci_bios_interface_level>>8,pci_bios_interface_level&0xFF);
	}
	if (pci_cfg == PCI_CFG_BIOS1) {
		printf("  PCI BIOS 1.xx interface\n");
	}
	if (pci_bios_last_bus == -1) {
		printf("  Autodetecting PCI bus count...\n");
		pci_probe_for_last_bus();
	}
	printf("  Last bus:                 %d\n",pci_bios_last_bus);
	printf("  Bus decode bits:          %d\n",pci_bus_decode_bits);

	{ /* prove readability by dumping 0,0,0 config space */
		unsigned int reg;
		uint32_t val;

		printf("PCI BUS/DEV/FUNC 0/0/0 config space: [DWORD]\n");
		for (reg=0;reg < (8*4);reg += 4) {
			val = pci_read_cfgl(0,0,0,reg);
			printf("%08lX ",(unsigned long)val);
		}
		printf("\n");

		for (reg=0;reg < (15*2);reg += 2) {
			val = pci_read_cfgw(0,0,0,reg);
			printf("%04X ",(unsigned int)val);
		}
		printf("\n");

		for (reg=0;reg < 25;reg++) {
			val = pci_read_cfgb(0,0,0,reg);
			printf("%02X ",(unsigned int)val & 0xFF);
		}
		printf("\n");
		while (getch() != 13);
	}

	/* then enumerate the bus */
	{
		int line=0;
		uint8_t bus,dev,func;
		uint32_t t32a,t32b,t32c;
		for (bus=0;bus <= pci_bios_last_bus;bus++) {
			for (dev=0;dev < 32;dev++) {
				uint8_t functions = pci_probe_device_functions(bus,dev);
				for (func=0;func < functions;func++) {
					/* make sure something is there before announcing it */
					uint16_t vendor,device,subsystem,subvendor_id;
					uint32_t class_code;
					uint8_t revision_id;
					vendor = pci_read_cfgw(bus,dev,func,0x00); if (vendor == 0xFFFF) continue;
					device = pci_read_cfgw(bus,dev,func,0x02); if (device == 0xFFFF) continue;
					subvendor_id = pci_read_cfgw(bus,dev,func,0x2C);
					subsystem = pci_read_cfgw(bus,dev,func,0x2E);

					class_code = pci_read_cfgl(bus,dev,func,0x08);
					revision_id = class_code & 0xFF;
					class_code >>= 8UL;

					/* show it Linux sysfs style */
					printf(" %04x:%02x:%02x.%x: Vendor %04x Device %04x Sub %04x Vnd %04x Class %06lx rev %02x\n",
						0,bus,dev,func,
						vendor,device,subsystem,subvendor_id,
						class_code,revision_id);

					/* any interrupt? */
					{
						uint8_t l = pci_read_cfgb(bus,dev,func,0x3C);
						uint8_t p = pci_read_cfgb(bus,dev,func,0x3D);
						if (p != 0)
							printf("   IRQ: %u (pin %c (%02Xh))\n",l,p-1+'A',p);
					}

					/* show resources too. note that to figure out how large the BARs are we have to
					 * write to them and see which bits toggle. finally, remember that changing BARs
					 * affects any resources that TSRs or other functions might be trying to use at
					 * the same time, so we must disable interrupts while doing this */
					{
						uint8_t bar;

						for (bar=0;bar < 6;bar++) {
							uint8_t io=0;
							uint32_t lower=0,higher=0;
							uint8_t reg = 0x10+(bar*4);

							_cli();
							t32a = pci_read_cfgl(bus,dev,func,reg);
							if (t32a == 0xFFFFFFFFUL) continue;
							io = t32a & 1;
							if (io) lower = t32a & 0xFFFFFFFCUL;
							else    lower = t32a & 0xFFFFFFF0UL;
							pci_write_cfgl(bus,dev,func,reg,0);
							t32b = pci_read_cfgl(bus,dev,func,reg);
							pci_write_cfgl(bus,dev,func,reg,~0UL);
							t32c = pci_read_cfgl(bus,dev,func,reg);
							pci_write_cfgl(bus,dev,func,reg,t32a); /* restore prior contents */
							if (t32a == t32b && t32b == t32c) {
								/* hm, can't change it? */
							}
							else {
								uint32_t size = ~(t32c & ~(io ? 3UL : 15UL));
								if (io) size &= 0xFFFFUL;
								if ((size+1UL) == 0UL) continue;
								higher = lower + size;
							}

							if (higher == lower) continue;

							_sti();
							line++;
							if (io) printf("    IO: 0x%04lx-0x%04lx\n",lower,higher);
							else    printf("   MEM: 0x%08lx-0x%08lx %s\n",lower,higher,
									t32a & 8 ? "Prefetch" : "Non-prefetch");
						}
						_sti();
					}

					if (++line >= 20) {
						while (getch() != 13);
						line -= 20;
					}
				}
			}
		}
	}

	return 0;
}

