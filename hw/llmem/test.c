/* NOTES:
 *    This code works perfectly on most test systems I have.
 *    Some systems do provide interesting insight though when they do fail.
 *
 *    Test: Oracle VirtualBox 4.1.8 with 64-bit guest and AMD VT-X acceleration:
 *    Result: VirtualBox reports PAE and PSE, and 32-bit test program works perfectly.
 *            The 16-bit real mode builds however, cause the virtual machine to hang
 *            when attempting to use the PSE method. This hang does not occur when
 *            AMD VT-x is disabled.
 *
 *    Test: Dell netbook with Intel Atom processor
 *    Result: Processor reports via CPUID that it has 32-bit linear and 32-bit physical
 *            address space. And it means it. The minute this program steps beyond 4GB,
 *            the CPU faults and the system resets. This is true regardless of whether
 *            the 16-bit real mode or the 32-bit protected mode version is used. This is
 *            true whether you use PSE-36 or PAE.
 *
 *            So apparently, they don't do what 386/486/pentium systems USED to do and just
 *            silently wrap the addresses, eh?
 *
 *            That means this code should make use of the "extended CPUID" to read those
 *            limits and then llmemcpy() should enforce them. */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/llmem/llmem.h>
#include <hw/cpu/cpuidext.h>

static unsigned char temp1[256],temp2[256];

static void print_contents(unsigned char *t,unsigned int len) {
	unsigned int x;

	len = (len + 15U) >> 4U;
	while (len-- > 0) {
		printf("    ");
		for (x=0;x < 16;x++) printf("%02X ",t[x]);
		t += 16;
		printf("\n");
	}
}

static void help() {
	fprintf(stderr,"Long-Long memory copy test program\n");
	fprintf(stderr,"llmem [options]\n");
	fprintf(stderr,"  /PAE        Prefer PAE if both are available\n");
	fprintf(stderr,"  /PSE        Prefer PSE if both are available\n");
}

int main(int argc,char **argv) {
	int i,pref_pse=0,pref_pae=0;
	char *a;

	for (i=1;i < argc;) {
		a = argv[i++];

		if (*a == '/' || *a == '-') {
			do { a++; } while (*a == '/' || *a == '-');

			if (!strcmp(a,"h") || !strcmp(a,"help")) {
				help();
				return 1;
			}
			else if (!strcmp(a,"pse")) {
				pref_pse=1;
			}
			else if (!strcmp(a,"pae")) {
				pref_pae=1;
			}
			else {
				help();
				return 1;
			}
		}
		else {
			fprintf(stderr,"Unknown arg '%s'\n",a);
			return 1;
		}
	}

	if (!llmem_init()) {
		printf("Your system is not suitable to use with the Long-Long memory access library\n");
		printf("Reason: %s\n",llmem_reason);
		return 1;
	}

	if (pref_pse && llmem_meth_pse)
		llmem_meth_pae = 0;
	if (pref_pae && llmem_meth_pae)
		llmem_meth_pse = 0;

	printf("Long-Long memory access is usable on this machine.\n");
	printf("  - PSE method: %d\n",llmem_meth_pse);
	printf("  - PAE method: %d\n",llmem_meth_pae);

	memset(temp1,0xAA,sizeof(temp1));
	if (llmemcpy(llmem_ptr2ofs(temp1),0x000000000UL,256UL)) { /* At lowest address */
		printf("Memory @ 0MB:\n");
		print_contents(temp1,256);
	}
	else {
		printf("! Cannot read memory @ 0MB: '%s'\n",llmem_reason);
	}
	while (getch() != 13);

	memset(temp1,0xAA,sizeof(temp1));
	if (llmemcpy(llmem_ptr2ofs(temp1),0xFFFFFF00UL,256UL)) { /* Just below the 4GB boundary */
		printf("Memory @ 4096MB-256b:\n");
		print_contents(temp1,256);
	}
	else {
		printf("! Cannot read memory @ 4096MB-256B: '%s'\n",llmem_reason);
	}
	while (getch() != 13);

	memset(temp2,0xAA,sizeof(temp2));
	if (llmemcpy(llmem_ptr2ofs(temp2),0x100000000UL,256UL)) { /* At the 4GB boundary */
		printf("Memory @ 4096MB:\n");
		print_contents(temp2,256);
	}
	else {
		printf("! Cannot read memory @ 4096MB: '%s'\n",llmem_reason);
	}
	while (getch() != 13);

	memset(temp2,0xAA,sizeof(temp2));
	if (llmemcpy(llmem_ptr2ofs(temp2),0x200000000UL,256UL)) { /* At the 8GB boundary */
		printf("Memory @ 8192MB:\n");
		print_contents(temp2,256);
	}
	else {
		printf("! Cannot read memory @ 8192MB: '%s'\n",llmem_reason);
	}
	while (getch() != 13);

	llmemcpy_free();
	cpu_extended_cpuid_info_free();
	return 0;
}

