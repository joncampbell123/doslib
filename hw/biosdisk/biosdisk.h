/* biosdisk.h
 *
 * INT 13h BIOS disk library.
 * (C) 2010-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box] */
 
#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <stdint.h>

struct biosdisk_drive {
	uint8_t		index;
	uint8_t		extended:1;		/* use INT 13h extensions */
	uint8_t		ext_packet_access:1;	/* can use packet read */
	uint8_t		drive_locking_eject:1;
	uint8_t		edd_support:1;
	uint8_t		write_enable:1;		/* enable write functions (sanity checking!!!) */
	uint8_t		dma_crossed:1;		/* set by CHS read/write functions if your IO crosses DMA boundaries */
	uint8_t		reserved:2;
	uint16_t	bytes_per_sector;
	int16_t		heads,cylinders,sectors_per_track;
	uint64_t	total_sectors;
};

#define BIOSDISK_EXTENDED 1

void biosdisk_free_resources();
int biosdisk_edd_get_geometry(struct biosdisk_drive *d,uint8_t index);
int biosdisk_check_extensions(struct biosdisk_drive *d,uint8_t index);
int biosdisk_classic_get_geometry(struct biosdisk_drive *d,uint8_t index);
int biosdisk_get_info(struct biosdisk_drive *d,uint8_t index,uint8_t flags);
int biosdisk_read(struct biosdisk_drive *d,
#if TARGET_MSDOS == 32
void *buffer,
#else
void far *buffer,
#endif
uint64_t sector,int num);
int biosdisk_write(struct biosdisk_drive *d,
#if TARGET_MSDOS == 32
void *buffer,
#else
void far *buffer,
#endif
uint64_t sector,int num);

