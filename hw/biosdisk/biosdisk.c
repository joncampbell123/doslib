/* biosdisk.c
 *
 * INT 13h BIOS disk library.
 * (C) 2010-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box]
 *
 * A library to deal with and use the INT 13h BIOS interface. It
 * wraps some functions to help deal with geometry translation,
 * DMA boundary issues, switching to/from protected mode, etc. */
 
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/biosdisk/biosdisk.h>

#if TARGET_MSDOS == 32
/* the protected mode version of this code needs a real-mode segment to transfer data */
static uint16_t			biosdisk_bufseg = 0;
static void*			biosdisk_buf = NULL;
#define biosdisk_bufsize	16384

static int biosdisk_buf_init() {
	uint32_t ofs;

	if (biosdisk_bufseg == 0) {
		biosdisk_buf = dpmi_alloc_dos(biosdisk_bufsize,&biosdisk_bufseg);
		if (biosdisk_buf == NULL) return 0;

		/* if that buffer crosses a 64KB boundary then we need to alloc another */
		ofs = (uint32_t)biosdisk_buf;
		if ((ofs & 0xFFFF0000UL) != ((ofs + biosdisk_bufsize - 1) & 0xFFFF0000UL)) {
			uint16_t p2seg=0;
			void *p2;

			p2 = dpmi_alloc_dos(biosdisk_bufsize,&p2seg);
			if (p2 == NULL) {
				dpmi_free_dos(biosdisk_bufseg);
				biosdisk_bufseg = 0;
				biosdisk_buf = NULL;
				return 0;
			}

			/* if that buffer crosses a 64KB boundary then give up */
			ofs = (uint32_t)p2;
			if ((ofs & 0xFFFF0000UL) != ((ofs + biosdisk_bufsize - 1) & 0xFFFF0000UL)) {
				dpmi_free_dos(biosdisk_bufseg);
				dpmi_free_dos(p2seg);
				biosdisk_bufseg = 0;
				biosdisk_buf = NULL;
				return 0;
			}

			dpmi_free_dos(biosdisk_bufseg);
			biosdisk_bufseg = p2seg;
			biosdisk_buf = p2;
		}
	}

	return (biosdisk_bufseg != 0);
}

static void biosdisk_buf_free() {
	if (biosdisk_bufseg != 0) {
		dpmi_free_dos(biosdisk_bufseg);
		biosdisk_bufseg = 0;
		biosdisk_buf = NULL;
	}
}
#else
/* 16-bit real mode: if the calling program attempts a read spanning a DMA boundary we have a "bounce buffer" for that */
static void far*		biosdisk_bounce = NULL;
#define biosdisk_bounce_size	4096

static int biosdisk_bounce_init() {
	uint32_t ofs;

	if (biosdisk_bounce == NULL) {
		biosdisk_bounce = _fmalloc(biosdisk_bounce_size);
		if (biosdisk_bounce == NULL) return 0;

		/* but wait--we need to ensure the buffer does not cross a 64KB boundary */
		ofs = ((uint32_t)FP_SEG(biosdisk_bounce) << 4UL) + ((uint32_t)FP_OFF(biosdisk_bounce));
		if ((ofs & 0xFFFF0000UL) != ((ofs + 0xFFFUL) & 0xFFFF0000UL)) {
			void far *p2 = _fmalloc(biosdisk_bounce_size);
			if (p2 == NULL) {
				_ffree(biosdisk_bounce);
				biosdisk_bounce = NULL;
				return 0;
			}

			/* and this new buffer doesn't cross either, right? */
			ofs = ((uint32_t)FP_SEG(p2) << 4UL) + ((uint32_t)FP_OFF(p2));
			if ((ofs & 0xFFFF0000UL) != ((ofs + 0xFFFUL) & 0xFFFF0000UL)) {
				_ffree(biosdisk_bounce);
				biosdisk_bounce = NULL;
				_ffree(p2);
				return 0;
			}

			_ffree(biosdisk_bounce);
			biosdisk_bounce = p2;
		}
	}

	return (biosdisk_bounce != NULL);
}
	
static void biosdisk_bounce_free() {
	if (biosdisk_bounce != NULL) _ffree(biosdisk_bounce);
	biosdisk_bounce = NULL;
}
#endif

void biosdisk_free_resources() {
#if TARGET_MSDOS == 32
	biosdisk_buf_free();
#else
	biosdisk_bounce_free();
#endif
}

#if TARGET_MSDOS == 32
static void biosdisk_realmode_13_call(struct dpmi_realmode_call *rc) {
	__asm {
		mov	ax,0x0300
		mov	bx,0x0013
		xor	cx,cx
		mov	edi,rc		; we trust Watcom has left ES == DS
		int	0x31		; call DPMI
	}
}
#endif

int biosdisk_classic_get_geometry(struct biosdisk_drive *d,uint8_t index) {
#if TARGET_MSDOS == 32
	struct dpmi_realmode_call rc={0};
	rc.eax = 0x0800;
	rc.edx = index;
	rc.edi = 0;
	rc.es = 0;
	biosdisk_realmode_13_call(&rc);
	if (rc.flags & 1) return 0;	/* if CF=1 */
	if (((rc.eax >> 8)&0xFF) != 0) return 0; /* if AH != 0 */

	d->index = index;
	d->sectors_per_track = (rc.ecx & 63);
	d->cylinders = (((rc.ecx >> 8) & 0xFF) | (((rc.ecx >> 6) & 3) << 8)) + 1;
	d->heads = ((rc.edx >> 8) & 0xFF) + 1;
	d->bytes_per_sector = 512;
	d->total_sectors = (uint64_t)d->sectors_per_track * (uint64_t)d->cylinders * (uint64_t)d->heads;
	return 1;
#else
	uint8_t last_head=0;
	uint16_t trksect=0;
	uint8_t retc=0xFF;

	__asm {
		mov	ah,8
		mov	dl,index
		push	es
		push	di
		xor	di,di
		mov	es,di
		int	0x13
		pop	di
		pop	es
		jc	call1
		mov	retc,ah
		mov	last_head,dh
		mov	trksect,cx
call1:
	}

	if (retc != 0)
		return 0;

	d->index = index;
	d->sectors_per_track = (trksect & 63);
	d->cylinders = ((trksect >> 8) | (((trksect >> 6) & 3) << 8)) + 1;
	d->heads = last_head + 1;
	d->bytes_per_sector = 512;
	d->total_sectors = (uint64_t)d->sectors_per_track * (uint64_t)d->cylinders * (uint64_t)d->heads;
	return 1;
#endif
}

int biosdisk_check_extensions(struct biosdisk_drive *d,uint8_t index) {
#if TARGET_MSDOS == 32
	struct dpmi_realmode_call rc={0};
	rc.eax = 0x4100;
	rc.edx = index;
	rc.ebx = 0x55AA;
	biosdisk_realmode_13_call(&rc);
	if (rc.flags & 1) return 0;	/* if CF=1 */
	if ((rc.ebx & 0xFFFF) != 0xAA55) return 0;
	d->extended = 1;
	d->edd_support = (rc.ecx & 4) ? 1 : 0;
	d->ext_packet_access = (rc.ecx & 1) ? 1 : 0;
	d->drive_locking_eject = (rc.ecx & 2) ? 1 : 0;
	return 1;
#else
	uint8_t retc=0xFF;
	uint16_t flags=0;

	__asm {
		mov	ah,0x41
		mov	dl,index
		mov	bx,0x55AA
		int	0x13
		jc	call1
		cmp	bx,0xAA55
		jnz	call1
		mov	retc,ah
		mov	flags,cx
call1:
	}

	if (retc == 0xFF)
		return 0;

	d->extended = 1;
	d->edd_support = (flags & 4) ? 1 : 0;
	d->ext_packet_access = (flags & 1) ? 1 : 0;
	d->drive_locking_eject = (flags & 2) ? 1 : 0;
	return 1;
#endif
}

int biosdisk_edd_get_geometry(struct biosdisk_drive *d,uint8_t index) {
#if TARGET_MSDOS == 32
	unsigned char *tmp;

	if (!biosdisk_buf_init())
		return 0;

	tmp = biosdisk_buf;
	memset(tmp,0,sizeof(tmp));
	*((uint16_t*)(tmp+0x00)) = 0x1E;

	{
		struct dpmi_realmode_call rc={0};
		rc.eax = 0x4800;
		rc.edx = index;
		rc.esi = ((uint32_t)biosdisk_buf) & 0xF;
		rc.ds = ((uint32_t)biosdisk_buf) >> 4;
		biosdisk_realmode_13_call(&rc);
		if (rc.flags & 1) return 0;	/* if CF=1 */
		if (((rc.eax >> 8) & 0xFF) != 0) return 0;
	}
#else
	unsigned char tmp[0x40],retc=0;
	void *p_tmp = (void*)tmp; /* this bullshit would be unnecessary if Watcom's inline assembler could just resolve the address of a stack variable */

	memset(tmp,0,sizeof(tmp));
	*((uint16_t*)(tmp+0x00)) = 0x1E;

	__asm {
		mov	ah,0x48
		mov	dl,index
		mov	si,word ptr p_tmp
#ifdef __LARGE__
		push	ds
		mov	ds,word ptr p_tmp+2
#endif
		int	0x13
		jnc	call1
#ifdef __LARGE__
		pop	ds
#endif
		mov	retc,ah
call1:
	}

	if (retc != 0)
		return 0;
#endif

	d->index = index;
	d->sectors_per_track = *((uint32_t*)(tmp+0x0C));
	d->cylinders = *((uint32_t*)(tmp+0x04));
	d->heads = *((uint32_t*)(tmp+0x08));
	d->bytes_per_sector = *((uint16_t*)(tmp+0x18));
	d->total_sectors = *((uint64_t*)(tmp+0x10)); /* NTS: This is often the TRUE geometry. Most BIOSes intentionally max out the C/H/S from this ioctl at 16383/16/63 */

	/* sanity checking: For CD-ROM drives (yes, they do show up through this interface!) the BIOS
	 * will often return CHS 0/0/0 and 0 total sectors (Virtual PC) or 0xFFFF/0xFFFF/0xFFFF CHS and
	 * 0xFFFFFFFFFFFFFFFF total sectors (VirtualBox). We have to filter our return values a bit
	 * to make disk I/O work regardless */
	if (d->bytes_per_sector == 2048) {
		if (d->cylinders == 0 || d->cylinders == (int16_t)0xFFFF) d->cylinders = 16383;
		if (d->sectors_per_track == 0 || d->sectors_per_track == (int16_t)0xFFFF) d->sectors_per_track = 63;
		if (d->heads == 0 || d->heads == (int16_t)0xFFFF) d->heads = 16;
		if (d->total_sectors == 0 || d->total_sectors == 0xFFFFFFFFFFFFFFFFULL)
			d->total_sectors = 0;
	}

	return 1;
}

int biosdisk_get_info(struct biosdisk_drive *d,uint8_t index,uint8_t flags) {
	memset(d,0,sizeof(*d));
	d->heads = d->cylinders = d->sectors_per_track = -1;
	if (index & 0x80) {
		if ((flags & BIOSDISK_EXTENDED) && biosdisk_check_extensions(d,index) && d->edd_support && biosdisk_edd_get_geometry(d,index))
			return 1;
		else {
			/* if I can't get the geometry through extensions then I might as well not use them */
			d->ext_packet_access = 0;
			if (biosdisk_classic_get_geometry(d,index))
				return 1;
		}
	}

	/* TODO: floppy interfaces */

	return 0;
}

/* INT 13H disk read, C/H/S style */
static int biosdisk_std_read(struct biosdisk_drive *d,
#if TARGET_MSDOS == 32
void *buffer,
#else
void far *buffer,
#endif
uint64_t sector,int num) {
	unsigned int C,H,S,srd,segv,offv,segrem,pair;
	unsigned char drv = d->index;
	unsigned char err,erc;
	uint32_t sec32,physo;
#if TARGET_MSDOS == 16
	int crossdma = 0;
#endif
	int ret = 0;

#if TARGET_MSDOS == 32
	if (!biosdisk_buf_init())
		return 0;
#else
	/* normalize pointer */
	segv = FP_SEG(buffer);
	offv = FP_OFF(buffer);
	physo = ((uint32_t)segv << 4UL) + ((uint32_t)offv);
	buffer = MK_FP(physo>>4,physo&0xF);
#endif

	d->dma_crossed = 0;
	while (num > 0) {
		if (d->total_sectors != 0ULL && sector >= d->total_sectors)
			return 0;
		if (sector >= (1024ULL*255ULL*63ULL)) /* if this goes beyond the max C/H/S 8GB limit... */
			return 0;

		sec32 = (uint32_t)sector;

		S = (unsigned int)(sec32 % ((uint32_t)d->sectors_per_track));
		sec32 /= (uint32_t)d->sectors_per_track;

		H = (unsigned int)(sec32 % ((uint32_t)d->heads));
		sec32 /= (uint32_t)d->heads;

		C = (unsigned int)sec32;

		srd = d->sectors_per_track - S;
		if (srd > ((unsigned int)num)) srd = (unsigned int)num;

#if TARGET_MSDOS == 32
		segv = (unsigned int)((size_t)biosdisk_buf >> 4);
		offv = (unsigned int)((size_t)biosdisk_buf & 0xF);
#else
		segv = FP_SEG(buffer);
		offv = FP_OFF(buffer);
#endif

		/* TODO: If the request crosses a 64KB boundary, then use an alternative buffer and memcpy it back */
		physo = ((uint32_t)segv << 4UL) + ((uint32_t)offv);
		if ((physo & 0xFFFF0000ULL) != ((physo + ((uint32_t)d->bytes_per_sector) - (uint32_t)1) & 0xFFFF0000ULL)) {
#if TARGET_MSDOS == 32
			fprintf(stderr,"DMA boundary crossing in 32-bit protected mode! This shouldn't happen!\n");
			fprintf(stderr,"The bounce buffer is not supposed to have been straddling a DMA boundary\n");
			abort();
			break;
#else
			if (!biosdisk_bounce_init()) {
				fprintf(stderr,"Error: cannot alloc bios disk bounce for DMA boundary read\n");
				break;
			}

			srd = 1;
			crossdma = 1;
			d->dma_crossed = 1;
			segv = FP_SEG(biosdisk_bounce);
			offv = FP_OFF(biosdisk_bounce);
			physo = ((uint32_t)segv << 4UL) + ((uint32_t)offv);
			if ((physo & 0xFFFF0000ULL) != ((physo + ((uint32_t)d->bytes_per_sector) - (uint32_t)1) & 0xFFFF0000ULL)) {
				fprintf(stderr,"Error: bounce buffer also crosses DMA boundary---what's the point?\n");
				break;
			}
#endif
		}

		/* how many can we read without crossing 64KB? */
		segrem = (0x10000 - (physo & 0xFFFFUL)) / d->bytes_per_sector;
		assert(segrem != 0); /* because the above code should have caught this! */
		if (srd > segrem) srd = segrem;

		assert(S < 63);
		assert(H < 256);
		assert(C < 1024);
		pair = (S + 1) | (((C >> 8) & 3) << 6) | ((C & 0xFF) << 8);

		/* normalize the segment offset */
		segv = physo >> 4;
		offv = physo & 0xF;
		err = erc = 0;

		/* carry out the read */
#if TARGET_MSDOS == 32
		{
			struct dpmi_realmode_call rc={0};
			rc.eax = 0x0200 | srd;
			rc.ebx = offv;
			rc.ecx = pair;
			rc.edx = drv | (H << 8);
			rc.es = segv;
			biosdisk_realmode_13_call(&rc);
			erc = (rc.eax >> 8) & 0xFF;
			if ((rc.flags & 1) == 0) err = rc.eax & 0xFF;
		}
#else
		__asm {
			push	es
			mov	ah,0x02
			mov	al,byte ptr srd
			mov	dh,byte ptr H
			mov	dl,drv
			mov	cx,pair
			mov	bx,segv
			mov	es,bx
			mov	bx,offv
			int	0x13
			pop	es
			mov	erc,ah
			jc	call1
			mov	err,al
call1:
		}
#endif
		if (erc != 0)
			break;
		if (err == 0 || err > srd) /* filter out BIOSes that return inane responses when AL should be sector count */
			err = srd; /* Compaq Elite laptop BIOS returns AX=0x0050 when asked to read one sector, for example */

		num -= err;
		ret += err;
		sector += (uint64_t)err;
#if TARGET_MSDOS == 32
		memcpy(buffer,biosdisk_buf,err * d->bytes_per_sector);
		buffer = (void*)((char*)buffer + (err * d->bytes_per_sector));
#else
		/* need to copy from bounce buffer */
		if (crossdma) _fmemcpy(buffer,biosdisk_bounce,d->bytes_per_sector);
		segv = FP_SEG(buffer);
		offv = FP_OFF(buffer);
		physo = ((uint32_t)segv << 4UL) + ((uint32_t)offv);
		physo += (err * d->bytes_per_sector);
		buffer = MK_FP(physo>>4,physo&0xF);
#endif
	}

	return ret;
}

/* NOTES:
 *   Microsoft Virtual PC 2007: For some reason the BIOS stubbornly returns 0x20 (controller failure) and does not allow us to read
 *   the CD-ROM drive. */
static int biosdisk_ext_read(struct biosdisk_drive *d,
#if TARGET_MSDOS == 32
void *buffer,
#else
void far *buffer,
#endif
uint64_t sector,int num) {
	unsigned char drv = d->index;
	unsigned int srd,segv,offv;
	unsigned char pkt[0x10];
	unsigned char err;
	uint32_t physo;
	int ret = 0;

#if TARGET_MSDOS == 32
	if (!biosdisk_buf_init())
		return 0;
#endif

	while (num > 0) {
		if (d->total_sectors != 0ULL && sector >= d->total_sectors)
			return 0;

		srd = (unsigned int)num;
#if TARGET_MSDOS == 32
		if (srd > ((biosdisk_bufsize - 0x20) / d->bytes_per_sector)) srd = ((biosdisk_bufsize - 0x20) / d->bytes_per_sector);
#else
		if (srd > ((0xFF00 - 0x20) / d->bytes_per_sector)) srd = ((0xFF00 - 0x20) / d->bytes_per_sector);
#endif
		err = 0x00;

#if TARGET_MSDOS == 32
		segv = (unsigned int)((size_t)((char*)biosdisk_buf + 0x20) >> 4);
		offv = (unsigned int)((size_t)((char*)biosdisk_buf + 0x20) & 0xF);
#else
		segv = FP_SEG(buffer);
		offv = FP_OFF(buffer);
#endif
		physo = ((uint32_t)segv << 4UL) + ((uint32_t)offv);
		pkt[0] = 0x10;
		pkt[1] = 0;
		*((uint16_t*)(pkt+2)) = srd;
		*((uint16_t*)(pkt+4)) = physo & 0xF;
		*((uint16_t*)(pkt+6)) = physo >> 4;
		*((uint64_t*)(pkt+8)) = sector;

		/* carry out the read */
#if TARGET_MSDOS == 32
		{
			struct dpmi_realmode_call rc={0};
			rc.eax = 0x4200;
			rc.edx = drv;
			rc.esi = ((uint32_t)biosdisk_buf) & 0xF;
			rc.ds = ((uint32_t)biosdisk_buf) >> 4;
			memcpy(biosdisk_buf,pkt,0x10);
			biosdisk_realmode_13_call(&rc);
			if (rc.flags & 1) err = (rc.eax >> 8) & 0xFF;
			else err = 0;
		}
#else
		segv = FP_SEG(pkt);
		offv = FP_OFF(pkt);
		__asm {
			push	ds
			mov	ah,0x42
			mov	dl,drv
			mov	ds,segv
			mov	si,offv
			int	0x13
			pop	ds
			jnc	call1
			mov	err,ah
call1:
		}
#endif
		if (err != 0)
			break;

		num -= srd;
		ret += srd;
		sector += (uint64_t)srd;
#if TARGET_MSDOS == 32
		memcpy(buffer,(char*)biosdisk_buf + 0x20,srd * d->bytes_per_sector);
		buffer = (void*)((char*)buffer + (srd * d->bytes_per_sector));
#else
		segv = FP_SEG(buffer);
		offv = FP_OFF(buffer);
		physo = ((uint32_t)segv << 4UL) + ((uint32_t)offv);
		physo += (srd * d->bytes_per_sector);
		buffer = MK_FP(physo>>4,physo&0xF);
#endif
	}

	return ret;
}

int biosdisk_read(struct biosdisk_drive *d,
#if TARGET_MSDOS == 32
void *buffer,
#else
void far *buffer,
#endif
uint64_t sector,int num) {
	if (num <= 0)
		return 0;

	if (d->ext_packet_access)
		return biosdisk_ext_read(d,buffer,sector,num);
	else if (d->sectors_per_track > 0)
		return biosdisk_std_read(d,buffer,sector,num);
	return 0;
}

/* INT 13H disk write, C/H/S style */
static int biosdisk_std_write(struct biosdisk_drive *d,
#if TARGET_MSDOS == 32
void *buffer,
#else
void far *buffer,
#endif
uint64_t sector,int num) {
	unsigned int C,H,S,srd,segv,offv,segrem,pair;
	unsigned char drv = d->index;
	unsigned char err,erc;
	uint32_t sec32,physo;
#if TARGET_MSDOS == 16
	int crossdma = 0;
#endif
	int ret = 0;

#if TARGET_MSDOS == 32
	if (!biosdisk_buf_init())
		return 0;
#else
	/* normalize pointer */
	segv = FP_SEG(buffer);
	offv = FP_OFF(buffer);
	physo = ((uint32_t)segv << 4UL) + ((uint32_t)offv);
	buffer = MK_FP(physo>>4,physo&0xF);
#endif

	d->dma_crossed = 0;
	while (num > 0) {
		if (d->total_sectors != 0ULL && sector >= d->total_sectors)
			return 0;
		if (sector >= (1024ULL*255ULL*63ULL)) /* if this goes beyond the max C/H/S 8GB limit... */
			return 0;

		sec32 = (uint32_t)sector;

		S = (unsigned int)(sec32 % ((uint32_t)d->sectors_per_track));
		sec32 /= (uint32_t)d->sectors_per_track;

		H = (unsigned int)(sec32 % ((uint32_t)d->heads));
		sec32 /= (uint32_t)d->heads;

		C = (unsigned int)sec32;

		srd = d->sectors_per_track - S;
		if (srd > ((unsigned int)num)) srd = (unsigned int)num;

#if TARGET_MSDOS == 32
		segv = (unsigned int)((size_t)biosdisk_buf >> 4);
		offv = (unsigned int)((size_t)biosdisk_buf & 0xF);
		memcpy(biosdisk_buf,buffer,srd * d->bytes_per_sector);
#else
		segv = FP_SEG(buffer);
		offv = FP_OFF(buffer);
#endif

		/* TODO: If the request crosses a 64KB boundary, then use an alternative buffer and memcpy it back */
		physo = ((uint32_t)segv << 4UL) + ((uint32_t)offv);
		if ((physo & 0xFFFF0000ULL) != ((physo + ((uint32_t)d->bytes_per_sector) - (uint32_t)1) & 0xFFFF0000ULL)) {
#if TARGET_MSDOS == 32
			fprintf(stderr,"DMA boundary crossing in 32-bit protected mode! This shouldn't happen!\n");
			fprintf(stderr,"The bounce buffer is not supposed to have been straddling a DMA boundary\n");
			abort();
			break;
#else
			if (!biosdisk_bounce_init()) {
				fprintf(stderr,"Error: cannot alloc bios disk bounce for DMA boundary read\n");
				break;
			}

			srd = 1;
			crossdma = 1;
			d->dma_crossed = 1;
			segv = FP_SEG(biosdisk_bounce);
			offv = FP_OFF(biosdisk_bounce);
			physo = ((uint32_t)segv << 4UL) + ((uint32_t)offv);
			if ((physo & 0xFFFF0000ULL) != ((physo + ((uint32_t)d->bytes_per_sector) - (uint32_t)1) & 0xFFFF0000ULL)) {
				fprintf(stderr,"Error: bounce buffer also crosses DMA boundary---what's the point?\n");
				break;
			}

			_fmemcpy(biosdisk_bounce,buffer,d->bytes_per_sector);
#endif
		}

		/* how many can we read without crossing 64KB? */
		segrem = (0x10000 - (physo & 0xFFFFUL)) / d->bytes_per_sector;
		assert(segrem != 0); /* because the above code should have caught this! */
		if (srd > segrem) srd = segrem;

		assert(S < 63);
		assert(H < 256);
		assert(C < 1024);
		pair = (S + 1) | (((C >> 8) & 3) << 6) | ((C & 0xFF) << 8);

		/* normalize the segment offset */
		segv = physo >> 4;
		offv = physo & 0xF;
		err = erc = 0;

		/* carry out the read */
#if TARGET_MSDOS == 32
		{
			struct dpmi_realmode_call rc={0};
			rc.eax = 0x0300 | srd;
			rc.ebx = offv;
			rc.ecx = pair;
			rc.edx = drv | (H << 8);
			rc.es = segv;
			biosdisk_realmode_13_call(&rc);
			erc = (rc.eax >> 8) & 0xFF;
			if ((rc.flags & 1) == 0) err = rc.eax & 0xFF;
		}
#else
		__asm {
			push	es
			mov	ah,0x03
			mov	al,byte ptr srd
			mov	dh,byte ptr H
			mov	dl,drv
			mov	cx,pair
			mov	bx,segv
			mov	es,bx
			mov	bx,offv
			int	0x13
			pop	es
			mov	erc,ah
			jc	call1
			mov	err,al
call1:
		}
#endif
		if (erc != 0)
			break;
		if (err == 0 || err > srd) /* filter out BIOSes that return inane responses when AL should be sector count */
			err = srd; /* Compaq Elite laptop BIOS returns AX=0x0050 when asked to write one sector, for example */

		num -= err;
		ret += err;
		sector += (uint64_t)err;
#if TARGET_MSDOS == 32
		buffer = (void*)((char*)buffer + (err * d->bytes_per_sector));
#else
		/* need to copy from bounce buffer */
		segv = FP_SEG(buffer);
		offv = FP_OFF(buffer);
		physo = ((uint32_t)segv << 4UL) + ((uint32_t)offv);
		physo += (err * d->bytes_per_sector);
		buffer = MK_FP(physo>>4,physo&0xF);
#endif
	}

	return ret;
}

static int biosdisk_ext_write(struct biosdisk_drive *d,
#if TARGET_MSDOS == 32
void *buffer,
#else
void far *buffer,
#endif
uint64_t sector,int num) {
	unsigned char drv = d->index;
	unsigned int srd,segv,offv;
	unsigned char pkt[0x10];
	unsigned char err;
	uint32_t physo;
	int ret = 0;

#if TARGET_MSDOS == 32
	if (!biosdisk_buf_init())
		return 0;
#endif

	while (num > 0) {
		if (d->total_sectors != 0ULL && sector >= d->total_sectors)
			return 0;

		srd = (unsigned int)num;
#if TARGET_MSDOS == 32
		if (srd > ((biosdisk_bufsize - 0x20) / d->bytes_per_sector)) srd = ((biosdisk_bufsize - 0x20) / d->bytes_per_sector);
#else
		if (srd > ((0xFF00 - 0x20) / d->bytes_per_sector)) srd = ((0xFF00 - 0x20) / d->bytes_per_sector);
#endif
		err = 0x00;

#if TARGET_MSDOS == 32
		segv = (unsigned int)((size_t)((char*)biosdisk_buf + 0x20) >> 4);
		offv = (unsigned int)((size_t)((char*)biosdisk_buf + 0x20) & 0xF);
#else
		segv = FP_SEG(buffer);
		offv = FP_OFF(buffer);
#endif
		physo = ((uint32_t)segv << 4UL) + ((uint32_t)offv);
		pkt[0] = 0x10;
		pkt[1] = 0;
		*((uint16_t*)(pkt+2)) = srd;
		*((uint16_t*)(pkt+4)) = physo & 0xF;
		*((uint16_t*)(pkt+6)) = physo >> 4;
		*((uint64_t*)(pkt+8)) = sector;

		/* carry out the write */
#if TARGET_MSDOS == 32
		memcpy((char*)biosdisk_buf + 0x20,buffer,srd * d->bytes_per_sector);
		{
			struct dpmi_realmode_call rc={0};
			rc.eax = 0x4300;
			rc.edx = drv;
			rc.esi = ((uint32_t)biosdisk_buf) & 0xF;
			rc.ds = ((uint32_t)biosdisk_buf) >> 4;
			memcpy(biosdisk_buf,pkt,0x10);
			biosdisk_realmode_13_call(&rc);
			if (rc.flags & 1) err = (rc.eax >> 8) & 0xFF;
			else err = 0;
		}
#else
		segv = FP_SEG(pkt);
		offv = FP_OFF(pkt);
		__asm {
			push	ds
			mov	ah,0x43
			mov	dl,drv
			mov	ds,segv
			mov	si,offv
			int	0x13
			pop	ds
			jnc	call1
			mov	err,ah
call1:
		}
#endif
		if (err != 0)
			break;

		num -= srd;
		ret += srd;
		sector += (uint64_t)srd;
#if TARGET_MSDOS == 32
		buffer = (void*)((char*)buffer + (srd * d->bytes_per_sector));
#else
		segv = FP_SEG(buffer);
		offv = FP_OFF(buffer);
		physo = ((uint32_t)segv << 4UL) + ((uint32_t)offv);
		physo += (srd * d->bytes_per_sector);
		buffer = MK_FP(physo>>4,physo&0xF);
#endif
	}

	return ret;
}

int biosdisk_write(struct biosdisk_drive *d,
#if TARGET_MSDOS == 32
void *buffer,
#else
void far *buffer,
#endif
uint64_t sector,int num) {
	if (num <= 0)
		return 0;

	if (!d->write_enable)
		return 0;

	if (d->ext_packet_access)
		return biosdisk_ext_write(d,buffer,sector,num);
	else if (d->sectors_per_track > 0)
		return biosdisk_std_write(d,buffer,sector,num);
	return 0;
}

