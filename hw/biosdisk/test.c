/* test.c
 *
 * INT 13h BIOS disk test program.
 * (C) 2010-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box]
 *
 * A test program for INT 13h functions.
 *
 * WARNING: If misued, this program CAN ERASE YOUR HARD DRIVE.
 * Please test this program only on a computer who's hard disk
 * contents you don't care for. */
 
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/biosdisk/biosdisk.h>

static char enable_extended = 1;
static unsigned char sector[4096*3];	/* NTS: To hold 512 byte sectors, 2048 byte CD-ROM sectors, or the 4KB sectors on the newest SATA drives */
static unsigned char sector2[4096*3];	/* NTS: To hold 512 byte sectors, 2048 byte CD-ROM sectors, or the 4KB sectors on the newest SATA drives */

static void print_ent(uint8_t hddcount,struct biosdisk_drive *tmp) {
	printf(	"[%u] %02X: E/D/P/R=%u/%u/%u/%u CHS %u/%u/%u ssz=%u tot=%llu (%lluKB)\n",
			hddcount,
			tmp->index,
			tmp->extended,
			tmp->edd_support,
			tmp->ext_packet_access,
			tmp->drive_locking_eject,
			tmp->cylinders,
			tmp->heads,
			tmp->sectors_per_track,
			tmp->bytes_per_sector,
			tmp->total_sectors,
			(tmp->total_sectors * ((uint64_t)tmp->bytes_per_sector)) >> 10ULL);
}

static int choose_drive(struct biosdisk_drive *drv) {
	struct biosdisk_drive hdd[17];
	int hddcount = 0,i;
	int c;

	/* NTS: There are some BIOS bugs that will show up quite visibly here!
	 *      
	 *      - Compaq Elite LTE 450C/X: The BIOS apparently only decodes the lower 4 bits of DL (drive index)
	 *        causing this loop to show the same drive(s) 8 times. But attempting to read/write the ghost
	 *        drives will fail. It does NOT support the extended INT 13H functions.
	 *                              (BIOS date: Approx 1994) */

	/* NTS: This loop will also reveal the hidden CD-ROM drive assignment on some machine and emulator configurations.
	 *      - Microsoft Virtual PC 2007: CD-ROM drive shows up on 0xF2. But extended functions to read it are denied
	 *        with the "controller failure" error code. The CD-ROM drive also shows up in the standard C/H/S based
	 *        INT 13h interface as well.
	 *
	 *      - VirtualBox 4.0.8: CD-ROM drive shows up on 0xE0. Reading works perfectly fine.
	 *
	 *      - Most PC BIOSes: CD-ROM drive is not assigned at all. The only way to test CD-ROM drive detection is to have
	 *        booted DOS from a CD to run this program (see "El Torito" specification). */

	/* stock entry for 1.44MB floppy */
	{
		struct biosdisk_drive *tmp = hdd+hddcount;
		memset(tmp,0,sizeof(*tmp));

		tmp->index = 0x00;
		tmp->extended = 0;
		tmp->edd_support = 0;
		tmp->ext_packet_access = 0;
		tmp->drive_locking_eject = 0;
		tmp->cylinders = 80;
		tmp->heads = 2;
		tmp->sectors_per_track = 18;
		tmp->bytes_per_sector = 512;
		tmp->total_sectors = tmp->cylinders * tmp->heads * tmp->sectors_per_track;
		print_ent(hddcount,tmp);
		hddcount++;
	}

	for (i=0;i < 0x80 && hddcount < 17;i++) {
		struct biosdisk_drive *tmp = hdd+hddcount;
		if (biosdisk_get_info(tmp,i+0x80,enable_extended ? BIOSDISK_EXTENDED : 0)) {
			print_ent(hddcount,tmp);
			hddcount++;
		}
	}

	printf("Choice? "); fflush(stdout);
	scanf("%d",&c);
	if (c < 0 || c >= hddcount) return 0;

	*drv = hdd[c];
	return 1;
}

static void chomp(char *s) {
	char *e = s+strlen(s)-1;
	while (e >= s && *e == '\n') *e-- = 0;
}

static void showsector(uint64_t sectn,struct biosdisk_drive *bdsk) {
	int row,col,rd;

	memset(sector,0xAA,512);
	if ((rd=biosdisk_read(bdsk,sector,sectn,1)) > 0) {
		if (rd != 1) printf("WARNING: BIOS drive returned more sectors than asked for\n");
		printf("Sector %llu contents:\n",sectn);
		for (row=0;row < 4;row++) {
			for (col=0;col < 16;col++)
				printf("%02X ",sector[(row*16)+col]);

			printf("  ");
			for (col=0;col < 16;col++) {
				char c = sector[(row*16)+col];
				if (c >= 32) printf("%c",c);
				else printf(".");
			}
			printf("\n");
		}
	}
}

static void showmsector(uint64_t sectn,struct biosdisk_drive *bdsk) {
	int sects = (int)(sizeof(sector) / bdsk->bytes_per_sector);
	int row,col,rd,rc;

	if ((rd=biosdisk_read(bdsk,sector,sectn,sects)) > 0) {
		if (rd > sects) printf("WARNING: BIOS drive returned more sectors than asked for\n");
		rc = 1;
		printf("Sector %llu-%llu contents:\n",sectn,sectn+rd-1);
		for (row=0;row < ((rd*bdsk->bytes_per_sector)/16);row++) {
			printf("%02x ",row);
			for (col=0;col < 16;col++)
				printf("%02X ",sector[(row*16)+col]);

			printf("  ");
			for (col=0;col < 16;col++) {
				char c = sector[(row*16)+col];
				if (c >= 32) printf("%c",c);
				else printf(".");
			}
			printf("\n");

			if (++rc >= 24) {
				while (getch() != 13);
				rc -= 24;
			}
		}
	}
}

static void mrv_test(struct biosdisk_drive *d) {
	int sects = (int)(sizeof(sector) / d->bytes_per_sector);
	unsigned int ui,dmacs=0;
	uint64_t sect,max;
	int do_sects;
	int rd,c;

	printf("Type 'Y' to begin read test.\n");
	c = getch();
	if (!(c == 'y' || c == 'Y')) return;
	printf("Okay, here we go!\n");

	if (d->total_sectors == 0)
		max = 0x7FFFFFFFUL;
	else
		max = d->total_sectors;

	for (sect=0;sect < max;) {
		int perc = (int)((sect * 100ULL) / max);
		printf("\x0D %%%u %llu/%llu [%u DMA crossing]   ",perc,sect,max,dmacs);

		do_sects = sects;
		if ((do_sects+sect) > max) do_sects = (int)(max - sect);

		/* multisector read */
		if ((rd=biosdisk_read(d,sector,sect,do_sects)) <= 0) {
			printf("failed [mrt]\n");
			return;
		}
		if (rd < do_sects) {
			printf("Got less than asked for (%d<%d) [mrt]\n",rd,do_sects);
			return;
		}
		if (d->dma_crossed) dmacs++;

		/* single-sector read */
		for (rd=0;rd < do_sects;rd++) {
			if (biosdisk_read(d,sector2+(rd*d->bytes_per_sector),sect+((uint64_t)rd),1) != 1) {
				printf("failed [sing]\n");
				return;
			}
			if (d->dma_crossed) dmacs++;
		}

		/* do they match? */
		for (ui=0;ui < ((unsigned int)do_sects * d->bytes_per_sector);ui++) {
			if (sector[ui] != sector2[ui]) {
				printf("Byte mismatch at %u into the read\n",ui);
				return;
			}
		}

		/* step */
		sect += do_sects;

		if (kbhit()) {
			if (getch() == 27)
				break;
		}
	}
	printf("\nTest complete\n");
}

static void multisector_read_test(struct biosdisk_drive *d) {
	int sects = (int)(sizeof(sector) / d->bytes_per_sector);
	uint64_t sect,max;
	int do_sects;
	int rd,c;

	printf("Type 'Y' to begin read test.\n");
	c = getch();
	if (!(c == 'y' || c == 'Y')) return;
	printf("Okay, here we go!\n");

	if (d->total_sectors == 0)
		max = 0x7FFFFFFFUL;
	else
		max = d->total_sectors;

	for (sect=0;sect < max;) {
		int perc = (int)((sect * 100ULL) / max);
		printf("\x0D %%%u %llu/%llu    ",perc,sect,max);

		do_sects = sects;
		if ((do_sects+sect) > max) do_sects = (int)(max - sect);
		if ((rd=biosdisk_read(d,sector,sect,do_sects)) <= 0) {
			printf("failed\n");
			return;
		}

		if (rd < do_sects)
			printf("Warning, got less than asked for (%d<%d)\n",rd,do_sects);

		sect += rd;

		if (kbhit()) {
			if (getch() == 27)
				break;
		}
	}
	printf("\nTest complete\n");
}

static void read_test(struct biosdisk_drive *d) {
	uint64_t sect,max;
	int rd,c;

	printf("Type 'Y' to begin read test.\n");
	c = getch();
	if (!(c == 'y' || c == 'Y')) return;
	printf("Okay, here we go!\n");

	if (d->total_sectors == 0)
		max = 0x7FFFFFFFUL;
	else
		max = d->total_sectors;

	for (sect=0;sect < max;sect++) {
		int perc = (int)((sect * 100ULL) / max);
		printf("\x0D %%%u %llu/%llu    ",perc,sect,max);

		if ((rd=biosdisk_read(d,sector,sect,1)) != 1) {
			printf("failed\n");
			return;
		}

		if (kbhit()) {
			if (getch() == 27)
				break;
		}
	}
	printf("\nTest complete\n");
}

static void chs_lba_test(struct biosdisk_drive *d) {
	struct biosdisk_drive d_chs={0};
	uint64_t sect;
	int rd,c;

	/* we need another biosdisk struct reflecting the CHS view of the drive */
	if (!d->ext_packet_access || d->index < 0x80) {
		printf("LBA extended read mode is not available for this drive\n");
		return;
	}

	if (!biosdisk_get_info(&d_chs,d->index,0)) {
		printf("Cannot detect drive in non-extended mode\n");
		return;
	}

	printf("Extended geo: %u/%u/%u %u bytes/sec\n",
		d->cylinders,
		d->heads,
		d->sectors_per_track,
		d->bytes_per_sector);

	printf("C/H/S non-extended geo: %u/%u/%u %u bytes/sec\n",
		d_chs.cylinders,
		d_chs.heads,
		d_chs.sectors_per_track,
		d_chs.bytes_per_sector);

	if (d_chs.bytes_per_sector != d->bytes_per_sector) {
		printf("Bytes per sector must match\n");
		return;
	}

	printf("Initial read test: "); fflush(stdout);
	if ((rd=biosdisk_read(d,sector,0,1)) != 1) {
		printf("LBA failed\n");
		return;
	}
	if ((rd=biosdisk_read(&d_chs,sector2,0,1)) != 1) {
		printf("CHS failed\n");
		return;
	}
	printf("OK\n");
	if (memcmp(sector,sector2,d->bytes_per_sector) != 0) {
		printf("...But the data read back does not match!\n");
		return;
	}

	printf("Type 'Y' to begin read test.\n");
	c = getch();
	if (!(c == 'y' || c == 'Y')) return;
	printf("Okay, here we go!\n");

	for (sect=0;sect < d_chs.total_sectors;sect++) {
		int perc = (int)((sect * 100ULL) / d_chs.total_sectors);
		printf("\x0D %%%u %llu/%llu    ",perc,sect,d_chs.total_sectors);

		if ((rd=biosdisk_read(d,sector,sect,1)) != 1) {
			printf("LBA failed\n");
			return;
		}
		if ((rd=biosdisk_read(&d_chs,sector2,sect,1)) != 1) {
			printf("CHS failed\n");
			return;
		}
		if (memcmp(sector,sector2,d->bytes_per_sector) != 0) {
			printf("Data mismatch\n");
			return;
		}

		if (kbhit()) {
			if (getch() == 27)
				break;
		}
	}
	printf("\nTest complete\n");
}

static void helpcmd() {
	printf("q: quit    g [number]: go to sector  z: last sector  b: back 1 sector\n");
	printf("w [msg]: write sector with message     c1: read test (LBA <-> CHS)\n");
	printf("rt: Read test     mrt: multisector read test    mr: multisector read\n");
	printf("mrv: Single + Multisector read test\n");
}

int main(int argc,char **argv) {
	struct biosdisk_drive bdsk = {0};
	uint64_t sectn = 0;
	char line[128],*pp;
	int die=0,ii,cc;

	for (ii=1;ii < argc;) {
		char *a = argv[ii++];

		if (*a == '-' || *a == '/') {
			do { a++; } while (*a == '-' || *a == '/');
			if (!strcmp(a,"nx")) {
				enable_extended = 0;
			}
			else if (!strcmp(a,"?") || !strcmp(a,"h") || !strcmp(a,"help")) {
				fprintf(stderr,"   /nx    Do not use INT 13h extensions\n");
				return 1;
			}
			else {
				fprintf(stderr,"I don't know what '%s' means\n",a);
			}
		}
	}

	cpu_probe();
	probe_dos();
	printf("DOS version %x.%02u\n",dos_version>>8,dos_version&0xFF);
	if (detect_windows()) {
		printf("I am running under Windows.\n");
		printf("    Mode: %s\n",windows_mode_str(windows_mode));
		printf("    Ver:  %x.%02u\n",windows_version>>8,windows_version&0xFF);
		printf("\n");
		printf("WARNING: Running this test program within Windows is NOT recommended.\n");
	}

	if (!choose_drive(&bdsk))
		return 1;

	helpcmd();
	while (!die) {
		printf("@ %llu\n",sectn);
		line[0]=0; fgets(line,sizeof(line)-1,stdin);
		pp = line; while (*pp == ' ') pp++;
		chomp(pp);

		if (*pp == 0) {
			showsector(sectn++,&bdsk);
		}
		else if (!strcmp(pp,"mrv")) {
			mrv_test(&bdsk);
		}
		else if (!strcmp(pp,"mr")) {
			showmsector(sectn,&bdsk);
		}
		else if (!strcmp(pp,"rt")) {
			read_test(&bdsk);
		}
		else if (!strcmp(pp,"mrt")) {
			multisector_read_test(&bdsk);
		}
		else if (!strcmp(pp,"c1")) {
			chs_lba_test(&bdsk);
		}
		else if (*pp == '?') {
			helpcmd();
		}
		else if (*pp == 'q') {
			die = 1;
		}
		else if (*pp == 'g') {
			pp++; while (*pp == ' ') pp++;
			sectn = (uint64_t)strtoll(pp,NULL,0);
			showsector(sectn,&bdsk);
		}
		else if (*pp == 'z') {
			if (bdsk.total_sectors == 0)
				sectn = 0x7FFFFFFFUL;
			else
				sectn = bdsk.total_sectors - 1;
			showsector(sectn,&bdsk);
		}
		else if (*pp == 'b') {
			if (sectn > 0) sectn--;
			showsector(sectn,&bdsk);
		}
		else if (*pp == 'w') {
			if (!bdsk.write_enable) {
				printf("WARNING: If you care about the data on the disk, abort now!\n");
				printf("Enable low-level writing to the disk?!? "); fflush(stdout);
				cc = getch();
				if (cc == 'y' || cc == 'Y') bdsk.write_enable = 1;
			}

			if (bdsk.write_enable) {
				int wd;
				pp++; while (*pp == ' ') pp++;
				for (ii=0;ii < bdsk.bytes_per_sector;ii++)
					sector[ii] = 0x30 + (ii & 0xF);
				for (ii=0;ii < bdsk.bytes_per_sector && pp[ii] != 0;ii++)
					sector[ii] = pp[ii];

				if ((wd=biosdisk_write(&bdsk,sector,sectn,1)) > 0) {
					if (wd != 1) printf("WARNING: BIOS drive wrote more sectors than asked for\n");
					printf("OK written\n");
					showsector(sectn,&bdsk);
				}
				else {
					printf("Failed\n");
				}
			}
		}
	}

	return 0;
}

