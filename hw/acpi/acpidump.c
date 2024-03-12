/* test.c
 *
 * ACPI BIOS interface test program.
 * (C) 2011-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box]
 */

#include <stdio.h>
#ifdef LINUX
#include <stdint.h>
#else
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <dos.h>
#endif
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <ctype.h>
#include <fcntl.h>

#ifdef LINUX
// NONE
#else
#include <hw/dos/dos.h>
#include <hw/cpu/cpu.h>
#include <hw/8254/8254.h>       /* 8254 timer */
#include <hw/8259/8259.h>       /* 8259 PIC */
#include <hw/flatreal/flatreal.h>
#include <hw/dos/doswin.h>
#endif
#include <hw/acpi/acpi.h>

static void help() {
    fprintf(stderr,"Test [options]\n");
    fprintf(stderr,"  /32      Use 32-bit RSDT\n");
}

static void acpidump_block(unsigned long long addr,unsigned long tmplen) {
    /* The output here is purposefully formatted to match that of the acpica acpidump tool
     * so that our output can then be picked apart and parsed by those tools. The purpose
     * of this tool is to be a portable tool for older machines that can run on MS-DOS and
     * snapshot the data, then the snapshotted data can be transferred to a newer machine
     * with the acpica tools. Except that this program does not sort the tables in memory
     * address order. */
    unsigned long p=0;
    unsigned int c=0;

    while (p < tmplen) {
        printf("  %04lx: ",(unsigned long)p);
        for (c=0;c < 16;c++) {
            if ((p+(unsigned long)c) < (unsigned long)tmplen)
                printf("%02x ",(unsigned int)acpi_mem_readb(addr+p+(unsigned long)c));
            else
                printf("   ");
        }
        printf(" ");
        for (c=0;c < 16;c++) {
            if ((p+(unsigned long)c) < (unsigned long)tmplen) {
                uint8_t b = acpi_mem_readb(addr+p+(unsigned long)c);
                if (b >= 0x20 && b <= 0x7E) printf("%c",(char)b);
                else printf(".");
            }
            else {
                printf(" ");
            }
        }
        printf("\n");
        p += 16;
    }

    printf("\n");
}

#define MAX_TABLES 512
typedef struct acpi_table_entry {
    uint64_t            addr;
    uint32_t            len;
    uint32_t            fourcc;
} acpi_table_entry;

static acpi_table_entry acpi_tables[MAX_TABLES];
static unsigned int acpi_table_count = 0;

static void acpi_table_add(const uint64_t addr,const uint32_t len,const uint32_t fourcc) {
    unsigned int i=0;

    while (i < acpi_table_count) {
        if (acpi_tables[i].addr == addr)
            return;

        i++;
    }

    if (acpi_table_count >= MAX_TABLES)
        return;

    acpi_tables[acpi_table_count].fourcc = fourcc;
    acpi_tables[acpi_table_count].addr = addr;
    acpi_tables[acpi_table_count].len = len;
    acpi_table_count++;
}

static void acpi_add_facs_from_facp(const unsigned long long addr,const unsigned long len) {
    if (len >= 40) {
        const uint32_t facs = acpi_mem_readd(addr+36);/*FIRMWARE_CTL*/

        if (facs != (uint32_t)0 && acpi_mem_readd(facs) == (uint32_t)0x53434146/*FACS*/) { /* NTS: "FACS" does not have a checksum field! */
            const uint32_t tmplen = acpi_mem_readd(facs+4);
            if (tmplen >= 64 && tmplen <= 4096)
                acpi_table_add((uint64_t)facs,(uint32_t)tmplen,(uint32_t)0x53434146/*FACS*/);
        }
    }
}

static void acpi_add_dsdt_from_facp(const unsigned long long addr,const unsigned long len) {
    if (len >= 44) {
        const uint32_t dsdt = acpi_mem_readd(addr+40);/*DSDT*/

        if (dsdt != (uint32_t)0 && acpi_mem_readd(dsdt) == (uint32_t)0x54445344/*DSDT*/) { /* NTS: "DSDT" does not have a checksum field! */
            const uint32_t tmplen = acpi_mem_readd(dsdt+4);
            if (tmplen >= 64 && tmplen <= 0x100000/*THESE TABLES ARE HUGE*/)
                acpi_table_add((uint64_t)dsdt,(uint32_t)tmplen,(uint32_t)0x54445344/*DSDT*/);
        }
    }
}

int main(int argc,char **argv) {
    acpi_memaddr_t addr;
    unsigned long i,max;
    uint32_t tmp32,tmplen;
    char tmp[32];

    for (i=1;i < (unsigned long)argc;) {
        const char *a = argv[(unsigned int)(i++)];

        if (*a == '-' || *a == '/') {
            do { a++; } while (*a == '-' || *a == '/');

            if (!strcmp(a,"?") || !strcmp(a,"h") || !strcmp(a,"help")) {
                help();
                return 1;
            }
            else if (!strcmp(a,"32")) {
                acpi_use_rsdt_32 = 1;
            }
            else {
                fprintf(stderr,"Unknown switch '%s'\n",a);
                help();
                return 1;
            }
        }
        else {
            fprintf(stderr,"Unknown arg '%s'\n",a);
            help();
            return 1;
        }
    }

#ifdef LINUX
    if (!acpi_probe_dev_mem()) {
        printf("Cannot init dev mem interface\n");
        return 1;
    }
#else
    if (!probe_8254()) {
        printf("Cannot init 8254 timer\n");
        return 1;
    }
    if (!probe_8259()) {
        printf("Cannot init 8259 PIC\n");
        return 1;
    }
    cpu_probe();
    probe_dos();
    detect_windows();
# if TARGET_MSDOS == 32
    probe_dpmi();
    dos_ltp_probe();
# endif
#endif

#if TARGET_MSDOS == 16
    if (!flatrealmode_setup(FLATREALMODE_4GB)) {
        printf("Unable to set up flat real mode (needed for 16-bit builds)\n");
        printf("Most ACPI functions require access to the full 4GB range.\n");
        return 1;
    }
#endif

    if (!acpi_probe()) {
        printf("ACPI BIOS not found\n");
        return 1;
    }
    assert(acpi_rsdp != NULL);
    fprintf(stderr,"ACPI %u.0 structure at 0x%05lX\n",acpi_rsdp->revision+1,(unsigned long)acpi_rsdp_location);

    memcpy(tmp,(char*)(&(acpi_rsdp->OEM_id)),6); tmp[6]=0;
    fprintf(stderr,"ACPI OEM ID '%s', RSDT address (32-bit) 0x%08lX Length %lu\n",tmp,
        (unsigned long)(acpi_rsdp->rsdt_address),
        (unsigned long)(acpi_rsdp->length));
    if (acpi_rsdp->revision != 0)
        fprintf(stderr,"   XSDT address (64-bit) 0x%016llX\n",
            (unsigned long long)(acpi_rsdp->xsdt_address));

    fprintf(stderr,"RSDT=0x%lx(len=0x%lx) XSDT=0x%llx(len=0x%0lx)\n",(unsigned long)acpi_rsdt_table_location,(unsigned long)acpi_rsdt_table_length,(unsigned long long)acpi_xsdt_table_location,(unsigned long)acpi_xsdt_table_length);
    fprintf(stderr,"Chosen RSDT/XSDT at 0x%08llX\n",(unsigned long long)acpi_rsdt_location);

    if (acpi_rsdt != NULL) {
        memcpy(tmp,(void*)(acpi_rsdt->signature),4); tmp[4] = 0;
        fprintf(stderr,"  '%s': len=%lu rev=%u\n",tmp,(unsigned long)acpi_rsdt->length,
            acpi_rsdt->revision);

        memcpy(tmp,(void*)(acpi_rsdt->OEM_id),6); tmp[6] = 0;
        fprintf(stderr,"  OEM id: '%s'\n",tmp);

        memcpy(tmp,(void*)(acpi_rsdt->OEM_table_id),8); tmp[8] = 0;
        fprintf(stderr,"  OEM table id: '%s' rev %lu\n",tmp,
            (unsigned long)acpi_rsdt->OEM_revision);

        memcpy(tmp,(void*)(&(acpi_rsdt->creator_id)),4); tmp[4] = 0;
        fprintf(stderr,"  Creator: '%s' rev %lu\n",tmp,
            (unsigned long)acpi_rsdt->creator_revision);
    }

    if (acpi_xsdt_table_location != (uint64_t)0) {
	acpi_select_xsdt();
        max = acpi_rsdt_entries();
        for (i=0;i < max;i++) {
            addr = acpi_rsdt_entry(i);
            if (addr == 0) continue;

            tmp32 = acpi_mem_readd(addr);
            if (tmp32 == 0) continue;

            tmplen = 0;
            if (acpi_probe_rsdt_check(addr,tmp32,&tmplen) && tmplen != 0)
                acpi_table_add((uint64_t)addr,(uint32_t)tmplen,(uint32_t)tmp32);

            if (tmp32 == 0x50434146/*FACP*/) {
                acpi_add_facs_from_facp(addr,tmplen);
                acpi_add_dsdt_from_facp(addr,tmplen);
	    }
        }
    }

    if (acpi_rsdt_table_location != (uint64_t)0) {
	acpi_select_rsdt();
        max = acpi_rsdt_entries();
        for (i=0;i < max;i++) {
            addr = acpi_rsdt_entry(i);
            if (addr == 0) continue;

            tmp32 = acpi_mem_readd(addr);
            if (tmp32 == 0) continue;

            tmplen = 0;
            if (acpi_probe_rsdt_check(addr,tmp32,&tmplen) && tmplen != 0)
                acpi_table_add((uint64_t)addr,(uint32_t)tmplen,(uint32_t)tmp32);

            if (tmp32 == 0x50434146/*FACP*/) {
                acpi_add_facs_from_facp(addr,tmplen);
                acpi_add_dsdt_from_facp(addr,tmplen);
	    }
        }
    }

    /* assume the ACPI library has already validated the XSDT and RSDT tables */
    if (acpi_xsdt_table_location != (uint64_t)0 && acpi_xsdt_table_length != 0)
        acpi_table_add(acpi_xsdt_table_location,acpi_xsdt_table_length,0x54445358);

    if (acpi_rsdt_table_location != (uint64_t)0 && acpi_rsdt_table_length != 0)
        acpi_table_add(acpi_rsdt_table_location,acpi_rsdt_table_length,0x54445352);

    if (acpi_rsdp_location != (uint64_t)0 && acpi_rsdp_length != 0)
        acpi_table_add(acpi_rsdp_location,acpi_rsdp_length,0);

    for (i=0;i < acpi_table_count;i++) {
        if (acpi_tables[i].fourcc != 0) {
            memcpy(tmp,&acpi_tables[i].fourcc,4); tmp[4] = 0;
            printf("%s @ 0x%llx\n",tmp,(unsigned long long)acpi_tables[i].addr);
        }
        else {
            printf("RSD PTR @ 0x%llx\n",(unsigned long long)acpi_tables[i].addr);
        }

        acpidump_block(acpi_tables[i].addr,acpi_tables[i].len);
    }

    acpi_free();
#ifdef LINUX
    acpi_devmem_free();
#endif
    return 0;
}

