/* test.c
 *
 * Test program: Detecting and displaying CPU information
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/cpu/cpup3sn.h>
#include <hw/cpu/cpuidext.h>
#include <hw/dos/doswin.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

static char cpu_cpuid_ext_cache_tbl_to_str__cache_tbl_info[32];
const char *cpu_cpuid_ext_cache_tbl_to_str(unsigned char c) {
	if (c == 1) {
		return "Direct";
	}
	else if (c == 0xFF) {
		return "Full assoc";
	}
	else if (c > 1) {
		sprintf(cpu_cpuid_ext_cache_tbl_to_str__cache_tbl_info,"%u-way assoc",c);
		return cpu_cpuid_ext_cache_tbl_to_str__cache_tbl_info;
	}
	return "Reserved";
}

const char *cpu_cpuid_ext_cache_tbl_l2_to_str(unsigned char c) {
	switch (c) {
		case 0x00: return "Disabled";
		case 0x01: return "Direct";
		case 0x02: return "2-way assoc";
		case 0x04: return "4-way assoc";
		case 0x06: return "8-way assoc";
		case 0x08: return "16-way assoc";
		case 0x0A: return "32-way assoc";
		case 0x0B: return "48-way assoc";
		case 0x0C: return "64-way assoc";
		case 0x0D: return "96-way assoc";
		case 0x0E: return "128-way assoc";
		case 0x0F: return "Fully assoc";
	};

	return "??";
}

static void pause_if_tty() {
	unsigned char c;
	if (isatty(1)) { do { read(0,&c,1); } while (c != 13); }
}

int main() {
	unsigned char c;
	unsigned short w;

	cpu_probe();
	printf("Your CPU is basically a %s or higher\n",cpu_basic_level_to_string(cpu_basic_level));

	if (cpu_v86_active)
		printf(" - Your CPU is currently running me in virtual 8086 mode\n");
	if (cpu_flags & CPU_FLAG_PROTECTED_MODE)
		printf(" - Your CPU is currently running in protected mode\n");
	if (cpu_flags & CPU_FLAG_PROTECTED_MODE_32)
		printf(" - Your CPU is currently running in 32-bit protected mode\n");

	if (cpu_flags & CPU_FLAG_CPUID) {
		printf(" - With CPUID (Vendor='%s' MaxID=0x%lX)\n",cpu_cpuid_vendor,cpu_cpuid_max);
		printf(" - Features A=0x%08lX,B=0x%08lX,D=0x%08lX,C=0x%08lX\n",
			cpu_cpuid_features.a.raw[0],
			cpu_cpuid_features.a.raw[1],
			cpu_cpuid_features.a.raw[2],
			cpu_cpuid_features.a.raw[3]);

		printf(" - Step %lu Model %lu Family %lu ProcType %lu ExtModel %lu ExtFamily %lu\n",
			cpu_cpuid_features.a.raw[0]&0xFUL,
			(cpu_cpuid_features.a.raw[0]>>4UL)&0xFUL,
			(cpu_cpuid_features.a.raw[0]>>8UL)&0xFUL,
			(cpu_cpuid_features.a.raw[0]>>12UL)&0x3UL,
			(cpu_cpuid_features.a.raw[0]>>16UL)&0xFUL,
			(cpu_cpuid_features.a.raw[0]>>20UL)&0xFFUL);

		printf(" - ");
#define X(b,t) if (cpu_cpuid_features.a.raw[2] & (1UL << ((unsigned long)(b)))) printf("%s ",t);
		X(0,"FPU");
		X(1,"VME");
		X(2,"DE");
		X(3,"PSE");
		X(4,"TSC");
		X(5,"MSR");
		X(6,"PAE");
		X(7,"MCE");
		X(8,"CX8");
		X(9,"APIC");
		X(11,"SEP");
		X(12,"MTRR");
		X(13,"PGE");
		X(14,"MCA");
		X(15,"CMOV");
		X(16,"PAT");
		X(17,"PSE36");
		X(18,"PSN");
		X(19,"CLFLUSH");
		X(21,"DTS");
		X(22,"ACPI");
		X(23,"MMX");
		X(24,"FXSR");
		X(25,"SSE");
		X(26,"SSE2");
		X(27,"SS");
		X(28,"HT");
		X(29,"TM");
		X(30,"IA64");
		X(31,"PBE");
#undef X
#define X(b,t) if (cpu_cpuid_features.a.raw[3] & (1UL << ((unsigned long)(b)))) printf("%s ",t);
		X(0,"PNI");
		X(1,"PCLMULQDQ");
		X(2,"DTES64");
		X(3,"MONITOR");
		X(4,"DS_CPL");
		X(5,"VMX");
		X(6,"SMX");
		X(7,"EST");
		X(8,"TM2");
		X(9,"SSSE3");
		X(10,"CID");
		X(12,"FMA");
		X(13,"CX16");
		X(14,"XTPR");
		X(15,"PDCM");
		X(17,"PCID");
		X(18,"DCA");
		X(19,"SSE4_1");
		X(20,"SSE4_2");
		X(21,"X2APIC");
		X(22,"MOVBE");
		X(23,"POPCNT");
		X(24,"TSCDEADLINE");
		X(25,"AES");
		X(26,"XSAVE");
		X(27,"OXSAVE");
		X(28,"AVX");
		X(29,"F16C");
		X(30,"RDRND");
		X(31,"HYPERVISOR");
#undef X
		printf("\n");
	}
	if (cpu_flags & CPU_FLAG_FPU) printf(" - With FPU\n");

	if (cpu_cpuid_features.a.raw[2] & (1UL << 18UL)) {
		cpu_ask_serial();
		printf(" - Processor serial 0x%08lX,0x%08lX,0x%08lX,0x%08lX\n",
			cpu_serial.raw[0],cpu_serial.raw[1],
			cpu_serial.raw[2],cpu_serial.raw[3]);
	}

	if (cpu_extended_cpuid_probe()) {
		assert(cpu_cpuid_ext_info != NULL);
		printf(" - With extended CPUID (Brand='%s' MaxID=0x%lX)\n",cpu_cpuid_ext_info->brand,cpu_cpuid_ext_info->cpuid_max);
		printf(" - Extended Features A=0x%08lX,B=0x%08lX,C=0x%08lX,D=0x%08lX\n",
			cpu_cpuid_ext_info->features.a.raw[0],
			cpu_cpuid_ext_info->features.a.raw[1],
			cpu_cpuid_ext_info->features.a.raw[2],
			cpu_cpuid_ext_info->features.a.raw[3]);
#define X(b,t) if (cpu_cpuid_ext_info->features.a.raw[2] & (1UL << ((unsigned long)(b)))) printf("%s ",t);
		/* TODO: What about Intel processors? */
		X(0,"LAHF/SAHF");
		X(1,"CmpLegacy");
		X(2,"SVM");
		X(3,"ExtAPICSpc");
		X(4,"AltMovCr8");	/* apparently "lock mov cr0" -> "mov cr8" ? Ick... */
		X(5,"ABM");
		X(6,"SSE4A");
		X(7,"MisalignSSE");
		X(8,"PREFETCH");
		X(9,"OSVW");
		X(10,"IBS");
		X(11,"XOP");
		X(12,"SKINIT");
		X(13,"WDT");
		X(15,"LWP");
		X(16,"FMA4");
		X(19,"NodeID");
		X(21,"TBM");
		X(22,"TopoExt");
#undef X
#define X(b,t) if (cpu_cpuid_ext_info->features.a.raw[3] & (1UL << ((unsigned long)(b)))) printf("%s ",t);
		/* TODO: What about Intel processors? */
		X(0,"FPU");
		X(1,"VME");
		X(2,"DE");
		X(3,"PSE");
		X(4,"TSC");
		X(5,"MSR");
		X(6,"PAE");
		X(7,"MCE");
		X(8,"CMPXCHG8B");
		X(9,"APIC");
		X(11,"SYSCALL");
		X(12,"MTRR");
		X(13,"PGE");
		X(14,"MCA");
		X(15,"CMOV");
		X(16,"PAT");
		X(17,"PSE36");
		X(20,"NX");
		X(22,"MMXExt");
		X(23,"MMX");
		X(24,"FXSR");
		X(25,"FFXSR");
		X(26,"PAGE1GB");
		X(27,"RDTSCP");
		X(29,"LONGMODE");
		X(30,"3DNOWEXT");
		X(31,"3DNOW");
#undef X
		printf("\n");

		if (cpu_cpuid_ext_info_has_cache_tlb_l1) {
			pause_if_tty();

			printf(" - L1 Cache & TLB IDs\n");

			/*-----------------------------------------------------*/
			printf("    2MB & 4MB:\n");

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb.a.raw[0] >> 24UL);
				printf("      cache data: %s ",cpu_cpuid_ext_cache_tbl_to_str(c));

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb.a.raw[0] >> 16UL);
				printf("(%u entries)\n",c);

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb.a.raw[0] >> 8UL);
				printf("      cache code: %s ",cpu_cpuid_ext_cache_tbl_to_str(c));

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb.a.raw[0] >> 0UL);
				printf("(%u entries)\n",c);

			/*-----------------------------------------------------*/
			printf("    4KB:\n");

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb.a.raw[1] >> 24UL);
				printf("      cache data: %s ",cpu_cpuid_ext_cache_tbl_to_str(c));

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb.a.raw[1] >> 16UL);
				printf("(%u entries)\n",c);

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb.a.raw[1] >> 8UL);
				printf("      cache code: %s ",cpu_cpuid_ext_cache_tbl_to_str(c));

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb.a.raw[1] >> 0UL);
				printf("(%u entries)\n",c);

			/*-----------------------------------------------------*/
			printf("    Data cache:\n");

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb.a.raw[2] >> 24UL);
				printf("      %uKB",(unsigned int)c);

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb.a.raw[2] >> 16UL);
				printf(" %s",cpu_cpuid_ext_cache_tbl_to_str(c));

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb.a.raw[2] >> 8UL);
				printf(" %u lines/tag",c);

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb.a.raw[2] >> 0UL);
				printf(" %u bytes/cache line\n",c);

			/*-----------------------------------------------------*/
			printf("    Code cache:\n");

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb.a.raw[3] >> 24UL);
				printf("      %uKB",(unsigned int)c);

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb.a.raw[3] >> 16UL);
				printf(" %s",cpu_cpuid_ext_cache_tbl_to_str(c));

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb.a.raw[3] >> 8UL);
				printf(" %u lines/tag",c);

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb.a.raw[3] >> 0UL);
				printf(" %u bytes/cache line\n",c);
		}

		if (cpu_cpuid_ext_info_has_cache_tlb_l2) {
			printf(" - L2 Cache & TLB IDs\n");

			/*-----------------------------------------------------*/
			printf("    2MB & 4MB:\n");

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb_l2.a.raw[0] >> 28UL) & 0xF;
				printf("      cache data: %s ",cpu_cpuid_ext_cache_tbl_l2_to_str(c));

				w = (unsigned short)(cpu_cpuid_ext_info->cache_tlb_l2.a.raw[0] >> 16UL) & 0xFFFU;
				printf("(%u entries)\n",c);

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb_l2.a.raw[0] >> 12UL) & 0xF;
				printf("      cache code: %s ",cpu_cpuid_ext_cache_tbl_l2_to_str(c));

				w = (unsigned short)(cpu_cpuid_ext_info->cache_tlb_l2.a.raw[0] >> 0UL) & 0xFFFU;
				printf("(%u entries)\n",c);

			/*-----------------------------------------------------*/
			printf("    4KB:\n");

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb_l2.a.raw[1] >> 28UL) & 0xF;
				printf("      cache data: %s ",cpu_cpuid_ext_cache_tbl_l2_to_str(c));

				w = (unsigned short)(cpu_cpuid_ext_info->cache_tlb_l2.a.raw[1] >> 16UL) & 0xFFFU;
				printf("(%u entries)\n",c);

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb_l2.a.raw[1] >> 12UL) & 0xF;
				printf("      cache code: %s ",cpu_cpuid_ext_cache_tbl_l2_to_str(c));

				w = (unsigned short)(cpu_cpuid_ext_info->cache_tlb_l2.a.raw[1] >> 0UL) & 0xFFFU;
				printf("(%u entries)\n",c);

			/*-----------------------------------------------------*/
			printf("    Cache:\n");

				w = (unsigned short)(cpu_cpuid_ext_info->cache_tlb_l2.a.raw[2] >> 16UL);
				printf("      %luKB",(unsigned long)w);

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb_l2.a.raw[2] >> 12UL) & 0xF;
				printf(" %s",cpu_cpuid_ext_cache_tbl_l2_to_str(c));

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb_l2.a.raw[2] >> 8UL) & 0xF;
				printf(" %u lines/tag",c);

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb_l2.a.raw[2] >> 0UL);
				printf(" %u bytes/cache line\n",c);
		}

		if (cpu_cpuid_ext_info_has_cache_tlb_l2) {
			printf(" - L3 Cache & TLB IDs\n");

			/*-----------------------------------------------------*/
			printf("    Cache:\n");

				w = (unsigned short)(cpu_cpuid_ext_info->cache_tlb_l2.a.raw[3] >> 18UL);
				printf("      %luKB",(unsigned long)w * 512UL); /* <- REALLY?!? */

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb_l2.a.raw[3] >> 12UL) & 0xF;
				printf(" %s",cpu_cpuid_ext_cache_tbl_l2_to_str(c));

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb_l2.a.raw[3] >> 8UL) & 0xF;
				printf(" %u lines/tag",c);

				c = (unsigned char)(cpu_cpuid_ext_info->cache_tlb_l2.a.raw[3] >> 0UL);
				printf(" %u bytes/cache line\n",c);
		}

		if (cpu_cpuid_ext_info_has_apm && cpu_cpuid_ext_info->apm.a.raw[0] != 0UL) {
			pause_if_tty();

			printf(" - Advanced Power Management\n");
#define X(b,t) if (cpu_cpuid_ext_info->apm.a.raw[0] & (1UL << ((unsigned long)(b)))) printf("%s ",t);
		/* TODO: What about Intel processors? */
			X(0,"TempSensor");
			X(1,"FID");
			X(2,"VID");
			X(3,"ThermalTrip");
			X(4,"HwThermalControl");
			X(6,"100MHzMultiplierControl");
			X(7,"HwPStateControl");
			X(8,"InvariantTSC");
			X(9,"CorePerfBoost");
			X(10,"ROEffectiveFreqIf");
#undef X
			printf("\n");
		}

		if (cpu_cpuid_ext_info_has_longmode) {
			pause_if_tty();

			printf(" - Long Mode\n");

			printf("   Address space: ");

				c = (unsigned char)(cpu_cpuid_ext_info->longmode.a.raw[0] >> 16UL);
				if (c != 0) printf("Guest: %u bits ",(unsigned int)c);

				c = (unsigned char)(cpu_cpuid_ext_info->longmode.a.raw[0] >> 8UL);
				printf("Linear: %u bits ",(unsigned int)c);

				c = (unsigned char)(cpu_cpuid_ext_info->longmode.a.raw[0] >> 0UL);
				printf("Physical: %u bits ",(unsigned int)c);

				printf("\n");

			{
				unsigned int max,num;

				max = num = (unsigned int)((cpu_cpuid_ext_info->longmode.a.raw[2] >> 0UL) & 0xFFUL) + 1U;
				if (((cpu_cpuid_ext_info->longmode.a.raw[2] >> 12) & 0xF) != 0)
					max = 2 << ((cpu_cpuid_ext_info->longmode.a.raw[2] >> 12) & 0xF);

				printf("   Cores: %u (out of possible %u)\n",num,max);
			}
		}
	}

	cpu_extended_cpuid_info_free();
	return 0;
}

