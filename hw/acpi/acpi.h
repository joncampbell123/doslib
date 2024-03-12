/* acpi.h
 *
 * ACPI BIOS interface library.
 * (C) 2011-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box]
 *
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

typedef uint64_t acpi_memaddr_t;

#pragma pack(push,1)
struct acpi_rsdp_descriptor_v1 { /* ACPI RSDP v1.0 descriptor */
    char                signature[8];
    uint8_t             checksum;
    char                OEM_id[6];
    uint8_t             revision;
    uint32_t            rsdt_address;
}; /* == 20 bytes */

struct acpi_rsdp_descriptor_v2 { /* ACPI RSDP v2.0 descriptor */
    /* v1.0 */
    char                signature[8];
    uint8_t             checksum;
    char                OEM_id[6];
    uint8_t             revision;
    uint32_t            rsdt_address;
    /* v2.0 */
    uint32_t            length;
    uint64_t            xsdt_address;
    uint8_t             extended_checksum;
    uint8_t             reserved[3];
}; /* == 36 bytes */

struct acpi_rsdt_header {
    char                signature[4];
    uint32_t            length;
    uint8_t             revision;
    uint8_t             checksum;
    char                OEM_id[6];
    char                OEM_table_id[8];
    uint32_t            OEM_revision;
    uint32_t            creator_id;
    uint32_t            creator_revision;
}; /* == 36 bytes */

struct acpi_mcfg_header { /* PCI Express MCFG table */
    char                signature[4];
    uint32_t            length;
    uint8_t             revision;
    uint8_t             checksum;
    char                OEM_id[6];
    char                OEM_table_id[8];
    uint32_t            OEM_revision;
    uint32_t            creator_id;
    uint32_t            creator_revision;
    /* MCFG specific */
    uint64_t            _reserved_;
    /* configuration entries follow, one per range of busses */
}; /* == 44 bytes */

struct acpi_mcfg_entry {
    uint64_t            base_address;
    uint16_t            pci_segment_group_number;
    unsigned char       start_pci_bus_number;
    unsigned char       end_pci_bus_number;
    uint32_t            _reserved_;
}; /* == 16 bytes */

#define acpi_rsdp_descriptor acpi_rsdp_descriptor_v2
#pragma pack(pop)

#define acpi_rsdt_is_xsdt() (acpi_rsdt != NULL && (*((uint32_t*)(acpi_rsdt->signature)) == 0x54445358UL))

/* rather than copypasta code, let's just map 32-bit reads and typecast down */
#define acpi_mem_readw(m) ((uint16_t)acpi_mem_readd(m))
#define acpi_mem_readb(m) ((uint8_t)acpi_mem_readd(m))

extern unsigned char                    acpi_use_rsdt_32;
extern uint32_t                         acpi_rsdp_location;
extern struct acpi_rsdp_descriptor*     acpi_rsdp;
extern uint32_t                         acpi_rsdt_location;
extern struct acpi_rsdt_header*         acpi_rsdt; /* RSDT or XSDT */

int acpi_probe();
void acpi_free();
int acpi_probe_ebda();
void acpi_probe_rsdt();
unsigned long acpi_rsdt_entries();
uint32_t acpi_mem_readd(acpi_memaddr_t m);
int acpi_probe_scan(uint32_t start,uint32_t end);
acpi_memaddr_t acpi_rsdt_entry(unsigned long idx);
void acpi_memcpy_from_phys(void *dst,acpi_memaddr_t src,uint32_t len);
int acpi_probe_rsdt_check(acpi_memaddr_t a,uint32_t expect,uint32_t *length);

#ifdef LINUX
int acpi_probe_dev_mem();
void acpi_devmem_free();
uint32_t acpi_devmem_readd(uint64_t m);
#endif

