
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

static inline unsigned char pci_powerof2(uint32_t x) {
	if (x == 0) return 1;
	return (x & (x - 1)) == 0 ? 1 : 0;
}

static void help() {
	printf("test [options]\n");
	printf("Test PCI access library\n");
	printf("   /?                this help\n");
	printf("   /b                Prefer BIOS access\n");
	printf("   /b1               Prefer BIOS 1.x access\n");
	printf("   /t1               Prefer Type 1 (0xCF8-0xCFF) access\n");
	printf("   /t2               Prefer Type 2 (0xCxxx) access\n");
	printf("   /d                Dump config space too\n");
}

int main(int argc,char **argv) {
	int pref = -1,i;
	char dumpraw = 0;

	for (i=1;i < argc;) {
		char *a = argv[i++];

		if (*a == '-' || *a == '/') {
			do { a++; } while (*a == '-' || *a == '/');

			if (!strcmp(a,"h") || !strcmp(a,"help") || !strcmp(a,"?")) {
				help();
				return 1;
			}
			else if (!strcmp(a,"d")) {
				dumpraw = 1;
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
					uint8_t header_type;
					uint32_t class_code;
					uint8_t revision_id;
					vendor = pci_read_cfgw(bus,dev,func,0x00); if (vendor == 0xFFFF) continue;
					device = pci_read_cfgw(bus,dev,func,0x02); if (device == 0xFFFF) continue;
					subvendor_id = pci_read_cfgw(bus,dev,func,0x2C);
					subsystem = pci_read_cfgw(bus,dev,func,0x2E);

					header_type = (uint8_t)pci_read_cfgw(bus,dev,func,0x0E);

					class_code = pci_read_cfgl(bus,dev,func,0x08);
					revision_id = class_code & 0xFF;
					class_code >>= 8UL;

					/* show it Linux sysfs style */
					printf(" %02x:%02x.%x: Vendor %04x Device %04x Sub %04x Vnd %04x Class %06lx rev %02x htyp %02x\n",
						bus,dev,func,
						vendor,device,subsystem,subvendor_id,
						class_code,revision_id,header_type);

					/* any interrupt? */
					if ((header_type&0x7F) == 0 || (header_type&0x7F) == 1) {
						uint8_t l,p;
						uint16_t t;

						// read the two in one cycle
						t = pci_read_cfgw(bus,dev,func,0x3C);
						l = (uint8_t)(t&0xFF); // 0x3C interrupt line
						p = (uint8_t)(t>>8); // 0x3D interrupt pin
						if (p != 0)
							printf("   IRQ: %u (pin %c (%02Xh))\n",l,p-1+'A',p);
					}

					/* show resources too. note that to figure out how large the BARs are we have to
					 * write to them and see which bits toggle. finally, remember that changing BARs
					 * affects any resources that TSRs or other functions might be trying to use at
					 * the same time, so we must disable interrupts while doing this */
					{
						uint8_t bar,bmax;

						if ((header_type&0x7F) == 0)
							bmax = 6;
						else if ((header_type&0x7F) == 1)
							bmax = 2;
						else
							bmax = 0;

						for (bar=0;bar < bmax;bar++) {
							uint8_t io=0;
							uint32_t lower=0,higher=0;
							uint8_t reg = 0x10+(bar*4);
							uint32_t sigbits,addrbits;
							uint16_t command;

							_cli();
							t32a = pci_read_cfgl(bus,dev,func,reg);
							if (t32a == 0xFFFFFFFFUL) continue;

							io = t32a & 1;
							if (io) {
								sigbits = 0x0000FFFCUL;
								addrbits = 0x0000FFFFUL;
							}
							else {
								sigbits = 0xFFFFFFF0UL;
								addrbits = 0xFFFFFFFFUL;
							}
							lower = t32a & sigbits;

							command = pci_read_cfgw(bus,dev,func,0x04);
							pci_write_cfgw(bus,dev,func,0x04,0);//disconnect

							pci_write_cfgl(bus,dev,func,reg,0);
							t32b = pci_read_cfgl(bus,dev,func,reg);
							pci_write_cfgl(bus,dev,func,reg,~0UL);
							t32c = pci_read_cfgl(bus,dev,func,reg);
							pci_write_cfgl(bus,dev,func,reg,t32a); /* restore prior contents */

							pci_write_cfgw(bus,dev,func,0x04,command);//restore command

							if (t32a == t32b && t32b == t32c) {
								/* hm, can't change it? */
								if (io && (t32a & 0xFFFF) == 0) continue;
								if (!io && t32a == 0) continue;

								_sti();
								line++;
								if (io) printf("    IO: 0x%04lx [FIXED]\n",lower);
								else    printf("   MEM: 0x%08lx [FIXED] %s\n",lower,
									t32a & 8 ? "Prefetch" : "Non-prefetch");
							}
							else {
								uint32_t mask,nsize;

								mask = (~t32b) & t32c & sigbits; // any bits that were 0 after writing 0 | any bits that were 1 after writing 1
								nsize = mask ^ addrbits;

								/* use the mask to determine size. the mask should be ~(2^N - 1)
								 * for example 0xFFFF0000, 0xFF000000, etc... we obtain size like this:
								 *
								 * mask              nsize        size = (nsize + 1)
								 * --------------------------------
								 * 0xFFFFFF00        0x000000FF   0x00000100 (256KB)
								 * 0xFFFF0000        0x0000FFFF   0x00010000 (64KB)
								 * 0xFF000000        0x00FFFFFF   0x01000000 (16MB)
								 *
								 * mask         nsize         size = (nsize + 1)
								 * --------------------------------
								 * 0xFFFC       0x0003        0x00000004 (4)
								 * 0xFFF0       0x000F        0x00000010 (16)
								 * 0xFF00       0x00FF        0x00000100 (256) */
								if (pci_powerof2(nsize + (uint32_t)1UL)) {
									lower = t32a & (~nsize);
									higher = t32a | nsize;
								}
								else {
									line += 2;
									printf(" * caution: probing PCI configuration gave inconsistent results.\n");
									printf("     mask=0x%08lx nsize=0x%08lx orig=0x%08lx b=0x%08lx c=0x%08lx\n",mask,nsize,t32a,t32b,t32c);
								}

								_sti();
								line++;
								if (io) printf("    IO: 0x%04lx-0x%04lx\n",lower,higher);
								else    printf("   MEM: 0x%08lx-0x%08lx %s\n",lower,higher,
									t32a & 8 ? "Prefetch" : "Non-prefetch");
							}
						}
						_sti();
					}

					if (++line >= 16) {
						while (getch() != 13);
						line -= 16;
					}

					if (dumpraw) { /* prove readability by dumping 0,0,0 config space */
						unsigned int reg;
						uint32_t val;

						for (reg=0;reg < (4*8*4);reg += 4) {
							val = pci_read_cfgl(bus,dev,func,reg);
							printf("%08lX",(unsigned long)val);
							if ((reg&((4*8)-1)) == ((4*8)-4)) printf("\n");
							else printf(" ");
						}

						for (reg=0;reg < (2*16*4);reg += 2) {
							val = pci_read_cfgw(bus,dev,func,reg);
							printf("%04X",(unsigned int)val);
							if ((reg&((2*16)-1)) == ((2*16)-2)) printf("\n");
							else printf(" ");
						}

						for (reg=0;reg < (16*4);reg++) {
							val = pci_read_cfgb(bus,dev,func,reg);
							printf("%02X",(unsigned int)val & 0xFF);
							if ((reg&((1*16)-1)) == ((1*16)-1)) printf("\n");
							else printf(" ");
						}

						while (getch() != 13);
					}

					/* single function device? stop scanning. */
					if (func == 0 && !(header_type & 0x80))
						break;
				}
			}
		}
	}

	return 0;
}

