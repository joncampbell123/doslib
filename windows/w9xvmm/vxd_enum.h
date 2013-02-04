
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>
#include <conio.h>
#include <fcntl.h>
#include <dos.h>
#include <i86.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/dosbox.h>

#pragma pack(push,1)
struct w9x_vmm_DDB
{
	uint32_t	next;
	uint16_t	ver;
	uint16_t	no;
	uint8_t		Mver;
	uint8_t		minorver;
	uint16_t	flags;
	unsigned char	name[8];
	uint32_t	Init;
	uint32_t	ControlProc;
	uint32_t	v86_proc;
	uint32_t	Pm_Proc;
	uint32_t	v86;	/* void (*v86)(); */
	uint32_t	PM;	/* void (*PM)(); */
	uint32_t	data;
	uint32_t	service_size;
	uint32_t	win32_ptr;
};
#pragma pack(pop)

struct w9x_vmm_DDB_scan {
	struct w9x_vmm_DDB	ddb;
	uint32_t		found_at;
	uint32_t		lowest,highest;
#if TARGET_MSDOS == 32 && defined(TARGET_WINDOWS)
	uint32_t		linear_bias;
#elif TARGET_MSDOS == 16 && defined(TARGET_WINDOWS)
	uint16_t		flat_sel;	/* will be alloc'd as 16-bit */
	uint32_t		flat_sel_base;
#elif TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS)
	uint16_t		flat_sel;	/* will be alloc'd as 32-bit */
#endif
};

#if TARGET_MSDOS == 16 && defined(TARGET_WINDOWS)
unsigned char far *w9x_vmm_vxd_ddb_scan_ptr(struct w9x_vmm_DDB_scan *vxdscan,uint32_t addr);
#elif TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS)
unsigned char far *w9x_vmm_vxd_ddb_scan_ptr(struct w9x_vmm_DDB_scan *vxdscan,uint32_t addr);
#endif

void w9x_vmm_last_vxd(struct w9x_vmm_DDB_scan *vxdscan);
int w9x_vmm_next_vxd(struct w9x_vmm_DDB_scan *vxdscan);
int w9x_vmm_first_vxd(struct w9x_vmm_DDB_scan *vxdscan);

