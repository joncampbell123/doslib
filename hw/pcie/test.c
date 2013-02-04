
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/flatreal/flatreal.h>
#include <hw/llmem/llmem.h>
#include <hw/pcie/pcie.h>
#include <hw/cpu/cpu.h>

static void help() {
	printf("test [options]\n");
	printf("Test PCI-Express access library\n");
	printf("   /?                this help\n");
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

	printf("PCI-Express bus test code\n");

	cpu_probe();
	if (cpu_basic_level < 5) {
		printf("PCI-Express programming requires a Pentium or higher\n");
		return 1;
	}

#if TARGET_MSDOS == 32
	probe_dpmi();
	dos_ltp_probe();
#endif

	if (!llmem_init()) {
		printf("Your system is not suitable to use with the Long-Long memory access library\n");
		printf("Reason: %s\n",llmem_reason);
	}

#if TARGET_MSDOS == 16
	if (!flatrealmode_setup(FLATREALMODE_4GB)) {
		printf("Unable to set up flat real mode (needed for 16-bit builds)\n");
		printf("Most ACPI functions require access to the full 4GB range.\n");
		return 1;
	}
#endif

	if (pcie_probe(pref) == PCIE_CFG_NONE) {
		printf("PCI-Express bus not found\n");
		return 1;
	}
	printf("Found PCI-Express BUS, code %d\n",pcie_cfg);
	printf("  Last bus:                 %d\n",pcie_bios_last_bus);
	printf("  Bus decode bits:          %d\n",pcie_bus_decode_bits);

	{ /* prove readability by dumping 0,0,0 config space */
		unsigned int reg;
		uint32_t val;
		uint64_t q;

		printf("PCI BUS/DEV/FUNC 0/0/0 config space: [DWORD]\n");
		for (reg=0;reg < (4*8);reg += 8) {
			q = pcie_read_cfgq(0,0,0,reg);
			printf("%016llX ",(unsigned long long)q);
		}
		printf("\n");

		for (reg=0;reg < (8*4);reg += 4) {
			val = pcie_read_cfgl(0,0,0,reg);
			printf("%08lX ",(unsigned long)val);
		}
		printf("\n");

		for (reg=0;reg < (15*2);reg += 2) {
			val = pcie_read_cfgw(0,0,0,reg);
			printf("%04X ",(unsigned int)val);
		}
		printf("\n");

		for (reg=0;reg < 25;reg++) {
			val = pcie_read_cfgb(0,0,0,reg);
			printf("%02X ",(unsigned int)val);
		}
		printf("\n");
		while (getch() != 13);
	}

	/* then enumerate the bus */
	{
		int line=0;
		uint16_t bus;
		uint8_t dev,func;
		uint32_t t32a,t32b,t32c;
		for (bus=0;bus <= pcie_bios_last_bus;bus++) {
			if (pcie_bus_mmio_base[bus] != 0ULL) {
				fprintf(stderr,"BUS %u at %08llX\n",bus,(unsigned long long)pcie_bus_mmio_base[bus]);
				line++;
			}

			for (dev=0;dev < 32;dev++) {
				uint8_t functions = pcie_probe_device_functions(bus,dev);
				for (func=0;func < functions;func++) {
					/* make sure something is there before announcing it */
					uint16_t vendor,device,subsystem,subvendor_id;
					uint32_t class_code;
					uint8_t revision_id;
					vendor = pcie_read_cfgw(bus,dev,func,0x00); if (vendor == 0xFFFF) continue;
					device = pcie_read_cfgw(bus,dev,func,0x02); if (device == 0xFFFF) continue;
					subvendor_id = pcie_read_cfgw(bus,dev,func,0x2C);
					subsystem = pcie_read_cfgw(bus,dev,func,0x2E);

					class_code = pcie_read_cfgl(bus,dev,func,0x08);
					revision_id = class_code & 0xFF;
					class_code >>= 8UL;

					/* show it Linux sysfs style */
					line++;
					printf(" %04x:%02x:%02x.%x: Vendor %04x Device %04x Sub %04x Vnd %04x Class %06lx rev %02x\n",
						0,bus,dev,func,
						vendor,device,subsystem,subvendor_id,
						class_code,revision_id);

					/* any interrupt? */
					{
						uint8_t l = pcie_read_cfgb(bus,dev,func,0x3C);
						uint8_t p = pcie_read_cfgb(bus,dev,func,0x3D);
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
							t32a = pcie_read_cfgl(bus,dev,func,reg);
							if (t32a == 0xFFFFFFFFUL) continue;
							io = t32a & 1;
							if (io) lower = t32a & 0xFFFFFFFCUL;
							else    lower = t32a & 0xFFFFFFF0UL;
							pcie_write_cfgl(bus,dev,func,reg,0);
							t32b = pcie_read_cfgl(bus,dev,func,reg);
							pcie_write_cfgl(bus,dev,func,reg,~0UL);
							t32c = pcie_read_cfgl(bus,dev,func,reg);
							pcie_write_cfgl(bus,dev,func,reg,t32a); /* restore prior contents */
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

					if (line >= 20) {
						while (getch() != 13);
						line -= 20;
					}
				}
			}

			if (line >= 20) {
				while (getch() != 13);
				line -= 20;
			}
		}
	}

	return 0;
}

