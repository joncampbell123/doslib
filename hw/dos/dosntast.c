/* dosntast.c
 *
 * Windows NT VDD driver, dynamically loaded by code that needs it
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * This driver when loaded allows the DOS program to call Windows NT APIs
 * such as version information or to use the WINMM WAVE API instead of
 * NTVDM.EXE's shitty Sound Blaster emulation.
 */

/* This is a Windows NT VDD driver */
#define NTVDM_VDD_DRIVER

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
#include <windows/ntvdm/ntvdmlib.h>
#include <windows/ntvdm/ntvdmvdd.h>

#define DEBUG

/* this is a DLL for Win32 */
#if TARGET_MSDOS == 16 || !defined(TARGET_WINDOWS)
# error this is a DLL for Win32 only
#endif

static HMODULE				this_vdd = 0;

/* If the DOS portion of this code calls NTVDM.EXE to load the same DLL again, NTVDM.EXE
 * will do it, and allocate another handle. We'd rather not waste handles, so we leave
 * a "mark" in the BIOS data area for the DOS program to find and know whether we're
 * already loaded, and what handle to use. */
static unsigned int			vm_marked = 0;

/* since 32-bit DOS builds must thunk to 16-bit mode to make calls, we use "fake" I/O
 * ports to control other aspects such as sound output to make it easier for the DOS
 * portion's interrupt controller to do it's job. But we never assume a fixed I/O port
 * base because, who knows, the user's NTVDM environment may have special VDD drivers
 * loaded. */
static uint16_t				vm_io_base = 0;
#define VM_IO_PORTS			0x20

/* +0x00 (WORD)  W/O   Function select. */
/* +0x01 (WORD)  W/O   Sub-function select. */

/* Interrupt handlers may use these, to save the current state and use the interface themselves
 * before restoring the interface for the code they interrupted.
 *
 * The idea being the DOS program should not have to CLI/STI all the time to avoid conflicts with their
 * own interrupt handlers. */

/* +0x00 (INSB)  R/O   Read current function/subfunction selection [24 bytes] */
/* +0x00 (OUTSB) W/O   Write current function/subfunction selection [24 bytes] */

#pragma pack(push,1)
typedef struct {
	uint16_t			function;		/* +0x00 */
	uint16_t			subfunction;		/* +0x02 */
	uint16_t			__reserved__1;		/* +0x04 */
	uint16_t			__reserved__2;		/* +0x06 */
	uint16_t			__reserved__3;		/* +0x08 */
	uint16_t			__reserved__4;		/* +0x0A */
	uint16_t			__reserved__5;		/* +0x0C */
	uint16_t			__reserved__6;		/* +0x0E */
	uint16_t			__reserved__7;		/* +0x10 */
	uint16_t			__reserved__8;		/* +0x12 */
	uint16_t			__reserved__9;		/* +0x14 */
	uint16_t			__reserved__A;		/* +0x16 */
} IOIF_STATE;

typedef struct {
	WORD				(*iop)(WORD param0);
	void				(*insb)(BYTE *data,WORD count);
	void				(*insw)(WORD *data,WORD count);
	void				(*outsb)(BYTE *data,WORD count);
	void				(*outsw)(WORD *data,WORD count);
} IOIF_COMMAND;
#pragma pack(pop)

/* IO interface state */
IOIF_STATE				io_if={0};
IOIF_COMMAND*				io_if_command=NULL; /* what to do on command */

/* What the fucking hell Watcom?
 * This function obviously never gets called, yet if I don't have it here
 * your linker suddenly decides the standard C libray doesn't exist? What the fuck? */
BOOL WINAPI DllMain(HINSTANCE hInstance,DWORD fdwReason,LPVOID lpvReserved) {
	/* FIXME: If Watcom demands this function, then why the fuck isn't it getting called on DLL load?
	 * What the fuck Open Watcom?!? */
	if (fdwReason == DLL_PROCESS_ATTACH) {
		this_vdd = hInstance;
	}

	return TRUE;
}

/* DOSNTAST_FUNCTION_GENERAL------------------------------------------------------------ */
static void ioif_function_general_messagebox_outsb(BYTE *data,WORD count) {
	if (io_if.subfunction == DOSNTAST_FUN_GEN_SUB_MESSAGEBOX) {
		/* whatever ASCIIZ string DS:SI points to is passed by NTVDM.EXE to us */
		MessageBoxA(NULL,data,"DOSNTAST.VDD",MB_OK);
	}
}

static IOIF_COMMAND ioif_command_general_messagebox = {
	/* If only Watcom C/C++ supported GCC's .name = value structure definition style,
	 * like what they do in the Linux kernel, we could make this a lot more obvious */
	NULL,					/* iop */
	NULL,					/* insb */
	NULL,					/* insw */
	ioif_function_general_messagebox_outsb,	/* outsb */
	NULL					/* outsw */
};

/* this is called when Function == general and selection changes */
static void ioif_function_general__SELECT() {
	switch (io_if.subfunction) {
		case DOSNTAST_FUN_GEN_SUB_MESSAGEBOX:
			io_if_command = &ioif_command_general_messagebox;
			break;
		default:
			io_if_command = NULL;
			break;
	}
}
/* DOSNTAST_FUNCTION_WINMM-------------------------------------------------------------- */
static WORD ioif_function_winmm_waveOutGetNumDevs_iop(WORD param0) {
	return (WORD)waveOutGetNumDevs();
}

static IOIF_COMMAND ioif_command_winmm_waveOutGetNumDevs = {
	/* If only Watcom C/C++ supported GCC's .name = value structure definition style,
	 * like what they do in the Linux kernel, we could make this a lot more obvious */
	ioif_function_winmm_waveOutGetNumDevs_iop,/* iop */
	NULL,					/* insb */
	NULL,					/* insw */
	NULL,					/* outsb */
	NULL					/* outsw */
};

static void ioif_function_winmm_waveOutOpen_outsb(BYTE *p,WORD count) {
	/* in: EAX = uDeviceID
	 *     EBX = dwCallbackInstance
	 *  DS:ESI = pwfx (WAVEFORMATEX*)
	 * out: EAX = handle (or 0xFFFFFFFF if error)
	 *      EBX = error */
	/* TODO */
	setEAX(0xFFFFFFFF);
	setEBX(0xFFFFFFFF);
}

static IOIF_COMMAND ioif_command_winmm_waveOutOpen = {
	NULL,					/* iop */
	NULL,					/* insb */
	NULL,					/* insw */
	ioif_function_winmm_waveOutOpen_outsb,	/* outsb */
	NULL					/* outsw */
};

static void ioif_function_winmm_waveOutGetDevCaps_insb(BYTE *p,WORD count) {
	/* EAX = uDeviceID
	 * EBX = cbwoc (sizeof of WAVEOUTCAPS) */
	setEAX(waveOutGetDevCaps(getEAX(),(WAVEOUTCAPS*)p,getEBX()));
}

static IOIF_COMMAND ioif_command_winmm_waveOutGetDevCaps = {
	NULL,					/* iop */
	ioif_function_winmm_waveOutGetDevCaps_insb,/* insb */
	NULL,					/* insw */
	NULL,					/* outsb */
	NULL					/* outsw */
};

/* this is called when Function == general and selection changes */
static void ioif_function_winmm__SELECT() {
	switch (io_if.subfunction) {
		case DOSNTAST_FUN_WINMM_SUB_waveOutGetNumDevs:
			io_if_command = &ioif_command_winmm_waveOutGetNumDevs;
			break;
		case DOSNTAST_FUN_WINMM_SUB_waveOutGetDevCaps:
			io_if_command = &ioif_command_winmm_waveOutGetDevCaps;
			break;
		case DOSNTAST_FUN_WINMM_SUB_waveOutOpen:
			io_if_command = &ioif_command_winmm_waveOutOpen;
			break;
		default:
			io_if_command = NULL;
			break;
	}
}
/* ------------------------------------------------------------------------------------- */
void ioif_function__SELECT() {
	switch (io_if.function) {
		case DOSNTAST_FUNCTION_GENERAL:
			ioif_function_general__SELECT();
			break;
		case DOSNTAST_FUNCTION_WINMM:
			ioif_function_winmm__SELECT();
			break;
		default:
			io_if_command = NULL;
	}
}

WORD ioif_command(WORD param0) {
	if (io_if_command != NULL && io_if_command->iop != NULL)
		return io_if_command->iop(param0);

	return 0xFFFFU;
}

void ioif_command_insb(BYTE *data,WORD count) {
	if (io_if_command != NULL && io_if_command->insb != NULL)
		io_if_command->insb(data,count);
}

void ioif_command_insw(WORD *data,WORD count) {
	if (io_if_command != NULL && io_if_command->insw != NULL)
		io_if_command->insw(data,count);
}

void ioif_command_outsb(BYTE *data,WORD count) {
	if (io_if_command != NULL && io_if_command->outsb != NULL)
		io_if_command->outsb(data,count);
}

void ioif_command_outsw(WORD *data,WORD count) {
	if (io_if_command != NULL && io_if_command->outsw != NULL)
		io_if_command->outsw(data,count);
}

void save_ioif_state(IOIF_STATE *data,WORD count) {
	if (count < sizeof(IOIF_STATE)) return;
	*data = io_if;
}

void restore_ioif_state(IOIF_STATE *data,WORD count) {
	if (count < sizeof(IOIF_STATE)) return;
	io_if = *data;
	ioif_function__SELECT();
}

void ioif_function_select(WORD f) {
	/* setting the function resets subfunction to zero */
	io_if.function = f;
	io_if.subfunction = 0;
	ioif_function__SELECT();
}

void ioif_subfunction_select(WORD f) {
	io_if.subfunction = f;
	ioif_function__SELECT();
}

/* when a DOS program does REP OUTSW, NTVDM.EXE provides the translated DS:SI for us.
 * Nice. Except... when done from a 32-bit DOS program, it only translates DS:SI for us.
 * Not very good when our DOS code uses *DS:ESI* as a flat 32-bit program!
 *
 * Speaking of which Microsoft why the hell isn't there a function to tell if the DS
 * segment is 16-bit or 32-bit?!?
 *
 * NTS: This code assumes x86 32-bit NTVDM.EXE behavior where DOS memory is mapped to
 *      the 0x00000-0xFFFFF area within the NTVDM process. */
BYTE *NTVDM_DS_ESI_correct(BYTE *p) {
	/* if protected mode, replace the pointer given with a proper pointer to DS:ESI */
	if (getMSW() & 1) {
		/* NTS: x86 behavior: VdmMapFlat() just returns a pointer. There's an
		 *      "unmap" function but it's a stub. We take advantage of that.
		 *      No punishment for "mapping" without "unmapping" */
		return (BYTE*)VdmMapFlat(getDS(),getESI(),VDM_PM);
	}

	return (BYTE*)p;
}

BYTE *NTVDM_ES_EDI_correct(BYTE *p) {
	/* if protected mode, replace the pointer given with a proper pointer to DS:ESI */
	if (getMSW() & 1) {
		/* NTS: x86 behavior: VdmMapFlat() just returns a pointer. There's an
		 *      "unmap" function but it's a stub. We take advantage of that.
		 *      No punishment for "mapping" without "unmapping" */
		return (BYTE*)VdmMapFlat(getES(),getEDI(),VDM_PM);
	}

	return (BYTE*)p;
}

/* IO handler */
VOID WINAPI io_inw(WORD iport,WORD *data) {
	if (iport == (vm_io_base+0x00)) /* function select */
		*data = io_if.function;
	else if (iport == (vm_io_base+0x01)) /* subfunction select */
		*data = io_if.subfunction;
	else if (iport == (vm_io_base+0x02)) /* command (param 0 in data) */
		*data = ioif_command(0xFFFFU);
	else
		*data = 0xFFFF; /* default */
}

VOID WINAPI io_inb(WORD iport,BYTE *data) {
	{
		WORD w;
		io_inw(iport,&w);
		*data = (BYTE)w; /* default: do whatever our word-size version would and return lower bits */
	}
}

VOID WINAPI io_insw(WORD iport,WORD *data,WORD count) {
	data = (WORD*)NTVDM_ES_EDI_correct((BYTE*)data);

	if (iport == (vm_io_base+0x00))
		save_ioif_state((IOIF_STATE*)data,count*2U); /* FIXME: Microsoft isn't clear: is "count" the count of WORDs? or BYTEs? */
	else if (iport == (vm_io_base+0x02)) /* command (param 0 in data) */
		ioif_command_insw(data,count);
}

VOID WINAPI io_insb(WORD iport,BYTE *data,WORD count) {
	data = NTVDM_ES_EDI_correct(data);

	if (iport == (vm_io_base+0x00))
		save_ioif_state((IOIF_STATE*)data,count);
	else if (iport == (vm_io_base+0x02)) /* command (param 0 in data) */
		ioif_command_insb(data,count);
}

VOID WINAPI io_outw(WORD iport,WORD data) {
	if (iport == (vm_io_base+0x00)) /* function select */
		ioif_function_select(data);
	else if (iport == (vm_io_base+0x01)) /* subfunction select */
		ioif_subfunction_select(data);
	else if (iport == (vm_io_base+0x02)) /* command (param 0 in data) */
		ioif_command(data);
}

VOID WINAPI io_outb(WORD iport,BYTE data) {
	io_outw(iport,data); /* default: pass the byte value up to the word-size callback */
}

VOID WINAPI io_outsw(WORD iport,WORD *data,WORD count) {
	data = (WORD*)NTVDM_DS_ESI_correct((BYTE*)data);

	if (iport == (vm_io_base+0x00))
		restore_ioif_state((IOIF_STATE*)data,count*2U); /* FIXME: Microsoft isn't clear: is "count" the count of WORDs? or BYTEs? */
	else if (iport == (vm_io_base+0x02)) /* command (param 0 in data) */
		ioif_command_outsw(data,count);
}

VOID WINAPI io_outsb(WORD iport,BYTE *data,WORD count) {
	data = NTVDM_DS_ESI_correct(data);

	if (iport == (vm_io_base+0x00))
		restore_ioif_state((IOIF_STATE*)data,count);
	else if (iport == (vm_io_base+0x02)) /* command (param 0 in data) */
		ioif_command_outsb(data,count);
}

static void mark_vm() {
	unsigned char *ptr;
	unsigned int i=0xF0/*start at 0x40:0xF0*/,max=0x200;

	if (vm_marked) return;

	ptr = VdmMapFlat(0x40,0x00,VDM_V86);
	if (ptr == NULL) return;

	/* find an empty spot to place our signature. the client is expected
	 * to write the handle value next to it */
	while (i < max) {
		if (!memcmp(ptr+i,"\0\0\0\0" "\0\0\0\0" "\0\0\0\0" "\0\0\0\0" "\0\0\0\0",20))
			break;

		i++;
	}

	if (i < max)
		memcpy(ptr+i,"DOSNTAST.VDD\xFF\xFF\xFF\xFF",16);

	VdmUnmapFlat(0x40,0x00,(DWORD)ptr,VDM_V86);
	vm_marked = i+0x400;
}

static void remove_vm_mark() {
	unsigned char *ptr;

	if (vm_marked >= 0x400 && vm_marked <= 0x600) {
		ptr = VdmMapFlat(0x40,0x00,VDM_V86);
		if (ptr == NULL) return;

		memset(ptr+vm_marked-0x400,0,16);

		VdmUnmapFlat(0x40,0x00,(DWORD)ptr,VDM_V86);
		vm_marked = 0;
	}
}

static void choose_io_port() {
	VDD_IO_HANDLERS h;
	VDD_IO_PORTRANGE pr;

	if (vm_io_base != 0) return;

	/* FIXME: Remove this when Watcom C/C++ actually calls up to DllMain on entry point */
	if (this_vdd == NULL)
		this_vdd = GetModuleHandle("DOSNTAST.VDD");
	if (this_vdd == NULL) {
		MessageBox(NULL,"NO!","",MB_OK);
		return;
	}

	h.inb_handler = io_inb;
	h.inw_handler = io_inw;
	h.insb_handler = io_insb;
	h.insw_handler = io_insw;
	h.outb_handler = io_outb;
	h.outw_handler = io_outw;
	h.outsb_handler = io_outsb;
	h.outsw_handler = io_outsw;

	/* choose an I/O port */
	for (vm_io_base=0x1000;vm_io_base <= 0xF000;vm_io_base += 0x80) {
		pr.First = vm_io_base;
		pr.Last = vm_io_base + VM_IO_PORTS - 1;

		if (VDDInstallIOHook(this_vdd,1/*cPortRange*/,&pr,&h)) {
			/* got it */
			break;
		}
	}

	if (vm_io_base > 0xF000) {
		/* didn't find any */
		MessageBox(NULL,"Failed","",MB_OK);
		vm_io_base = 0;
	}
}

static void remove_io_port() {
	VDD_IO_PORTRANGE r;

	if (vm_io_base == 0) return;
	r.First = vm_io_base;
	r.Last = vm_io_base + VM_IO_PORTS - 1;
	VDDDeInstallIOHook(this_vdd,1,&r);
	vm_io_base = 0;
}

/* Microsoft documentation on this "init" routine is lacking */
__declspec(dllexport) void WINAPI Init() {
	if (!vm_marked) mark_vm();
	if (vm_io_base == 0) choose_io_port();
}

__declspec(dllexport) void WINAPI Dispatch() {
	uint32_t command;
	char tmp[64];

	command = getEBX();
	if (command == DOSNTAST_INIT_REPORT_HANDLE) {
		setEBX(0x55AA55AA);
		setECX(vm_marked);
	}
	else if (command == DOSNTAST_GETVERSIONEX) {
		unsigned char *ptr = NULL;
		uint16_t seg;
		uint32_t ofs;
		uint8_t mode;

		seg = getDS();
		ofs = getESI();
		mode = (getCX() == 1) ? VDM_PM : VDM_V86;

		ptr = VdmMapFlat(seg,ofs,mode);
		if (ptr != NULL) {
			setEBX(GetVersionEx((OSVERSIONINFO*)ptr));
			VdmUnmapFlat(seg,ofs,(DWORD)ptr,mode);
		}
	}
	else if (command == DOSNTAST_GET_TICK_COUNT) {
		setEBX(GetTickCount());
	}
	/* the DOS program sends this when it's about to unload us.
	 * I originally wanted DllMain to do this on PROCESS_DETACH but,
	 * for some reason, that function isn't getting called. */
	else if (command == DOSNTAST_GET_IO_PORT) {
		setEBX(0x55AA55AA);
		setEDX(vm_io_base);
	}
	else if (command == DOSNTAST_NOTIFY_UNLOAD) {
		if (vm_io_base) remove_io_port();
		if (vm_marked) remove_vm_mark();
		setEBX(0x55AA55AA);
	}
	else {
		sprintf(tmp,"0x%08lX\n",(unsigned long)command);
		MessageBox(NULL,tmp,"Unknown command",MB_OK);
	}
}

