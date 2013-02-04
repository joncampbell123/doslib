
#ifndef __ISAPNP_ISAPNP_H
#define __ISAPNP_ISAPNP_H

/* NOTES:
 *   
 *   Provides an API for BIOS PnP calls.
 *   Contrary to what you might expect this is apparently done by scanning the ROM area
 *   for a structure then making far calls to a memory address stored there. There is
 *   a real-mode and protected mode interface.
 *     - If compiled for 16-bit real mode, then only the real mode interface is
 *       supported by this code. The calls to the BIOS are provided by macros that
 *       make direct calls to the subroutine.
 *     - If compiled for 32-bit protected (flat) mode, then both real and protected
 *       mode interfaces are supported. Real mode calls are made via the DPMI server
 *       functions (thunking to real or v86 mode) and protected mode is dealt with
 *       through abuse of the DPMI selector functions and willingness to jump between
 *       16/32-bit code segments (since the protected mode entry point is 16-bit
 *       protected mode, and we are a 32-bit flat protected mode DOS program.... see
 *       the conflict?). While I've tested this code as best I can, I can't really
 *       guarantee that the code will work 100% reliably if your BIOS is funny or
 *       broken in the protected mode entry point.
 *
 *   The protected mode version of this code uses DPMI and DOS to create a "common
 *   area" below the 1MB boundary where parameters and stack are maintained to call
 *   the BIOS with (real or protected).
 *
 *   This also provides basic functions for direct ISA PnP I/O including management
 *   of address and data ports.
 *
 *   Requires the 8254 timer functions. Calling program is expected to init the 8254
 *   library. The timer routines are needed for configuration read timeouts.
 *
 *   DO NOT RUN THIS CODE UNDER MS WINDOWS!!! I'm not certain how well the Windows 9x
 *   and Windows 3.1 kernels would take us mucking around with ISA configuration
 *   registers but it would probably cause a hard crash! RUN THIS CODE FROM PURE DOS
 *   MODE!
 *
 *   WARNING: There are some worst case scenarios you should be aware of:
 *     - The BIOS subroutine may have serious bugs especially in protected mode
 *     - Calls to the BIOS can crash in really bad situations
 *     - If the motherboard, hardware, or overall computer implementation is
 *       cheap and crappy, this code might trigger some serious problems and
 *       hang the machine.
 *     - If for any reason something OTHER than the ROM BIOS exists at 0xF0000-0xFFFFF
 *       and reading triggers any hardware activity, this code will probably cause
 *       problems there as well. */

#include <hw/cpu/cpu.h>
#include <stdint.h>

enum {
	ISA_PNP_BIOS_GET_SYSDEV_NODE_CTRL_NOW=1,
	ISA_PNP_BIOS_GET_SYSDEV_NODE_CTRL_NEXT_BOOT=2
};

/* number of configuration "blocks" in a system device node.
 * each "block" is one after the other, terminated by an END tag.
 * first one is what is allocated, next is what is possible, third
 * is what is compatible. */
#define ISAPNP_CONFIG_BLOCKS	3

extern const char *isapnp_dma_xfer_preference_str[4];
extern const char *isapnp_fml32_miosize_str[4];
extern const char *isapnp_sdf_priority_strs[3];
extern const char *isapnp_config_block_str[3];
extern const char *isapnp_dma_speed_str[4];
extern const char *isapnp_tag_strs[];

#pragma pack(push,1)
struct isa_pnp_bios_struct {
	char		signature[4];		/* 0x00 */
	uint8_t		version;		/* 0x04 */
	uint8_t		length;			/* 0x05 */
	uint16_t	control;		/* 0x06 */
	uint8_t		checksum;		/* 0x08 */
	uint32_t	event_notify_flag_addr;	/* 0x09 */
	uint16_t	rm_ent_offset;		/* 0x0D */
	uint16_t	rm_ent_segment;		/* 0x0F */
	uint16_t	pm_ent_offset;		/* 0x11 */
	uint32_t	pm_ent_segment_base;	/* 0x13 */
	uint32_t	oem_device_id;		/* 0x17 */
	uint16_t	rm_ent_data_segment;	/* 0x1B */
	uint32_t	pm_ent_data_segment_base;/* 0x1D */
						/* 0x21 */
};

/* NTS: This actually represents the fixed portion of the struct.
 *      Variable length records follow */
struct isa_pnp_device_node {
	uint16_t	size;			/* 0x00 ...of entire node */
	uint8_t		handle;			/* 0x02 */
	uint32_t	product_id;		/* 0x03 */
	uint8_t		type_code[3];		/* 0x07 */
	uint16_t	attributes;		/* 0x0A */
#define ISAPNP_DEV_ATTR_CANT_DISABLE					(1 << 0)
#define ISAPNP_DEV_ATTR_CANT_CONFIGURE					(1 << 1)
#define ISAPNP_DEV_ATTR_CAN_BE_PRIMARY_OUTPUT				(1 << 2)
#define ISAPNP_DEV_ATTR_CAN_BE_PRIMARY_INPUT				(1 << 3)
#define ISAPNP_DEV_ATTR_CAN_BE_PRIMARY_IPL				(1 << 4)
#define ISAPNP_DEV_ATTR_DOCKING_STATION_DEVICE				(1 << 5)
#define ISAPNP_DEV_ATTR_REMOVEABLE_STATION_DEVICE			(1 << 6)

#define ISAPNP_DEV_ATTR_WHEN_CONFIGURABLE(x)				(((x) >> 7) & 3)

#define ISAPNP_DEV_ATTR_WHEN_CONFIGURABLE_ONLY_NEXT_BOOT		0
#define ISAPNP_DEV_ATTR_WHEN_CONFIGURABLE_AT_RUNTIME			1
#define ISAPNP_DEV_ATTR_WHEN_CONFIGURABLE_ONLY_RUNTIME			3

	/* variable length fields follow */
};
#pragma pack(pop)

/* small tags */
#define ISAPNP_TAG_PNP_VERSION			0x01
#define ISAPNP_TAG_LOGICAL_DEVICE_ID		0x02
#define ISAPNP_TAG_COMPATIBLE_DEVICE_ID		0x03
#define ISAPNP_TAG_IRQ_FORMAT			0x04
#define ISAPNP_TAG_DMA_FORMAT			0x05
#define ISAPNP_TAG_START_DEPENDENT_FUNCTION	0x06
#define ISAPNP_TAG_END_DEPENDENT_FUNCTION	0x07
#define ISAPNP_TAG_IO_PORT			0x08
#define ISAPNP_TAG_FIXED_IO_PORT		0x09
#define ISAPNP_TAG_VENDOR			0x0E
#define ISAPNP_TAG_END				0x0F
/* large tags */
#define ISAPNP_TAG_ANSI_ID_STRING		0x12
#define ISAPNP_TAG_FIXED_MEMORY_LOCATION_32	0x16

struct isapnp_tag {
	uint8_t		tag;	/* tag: 0x00+N = small tag N   0x10+N = large tag N */
	uint16_t	len;	/* length of data */
	uint8_t far*	data;	/* data */
};

#pragma pack(push,1)
struct isapnp_tag_compatible_device_id {
	uint32_t	id;
};

struct isapnp_tag_pnp_version { /* ISAPNP_TAG_PNP_VERSION len=2 */
	uint8_t		pnp;
	uint8_t		vendor;
};

struct isapnp_tag_logical_device_id {
	uint32_t	logical_device_id;
	uint8_t		flags;
	uint8_t		flags2;	/* if length >= 6 */
};

struct isapnp_tag_irq_format {
	uint16_t	irq_mask;
	/*------------------------*/
	uint8_t		hte:1;			/* bit 0 */
	uint8_t		lte:1;			/* bit 1 */
	uint8_t		htl:1;			/* bit 2 */
	uint8_t		ltl:1;			/* bit 3 */
	uint8_t		reserved:4;		/* bit 4-7 */
};

struct isapnp_tag_dma_format {
	uint8_t		dma_mask;
	uint8_t		xfer_preference:2;	/* bit 0-1 */
#define ISAPNP_TAG_DMA_XFER_PREF_8BIT_ONLY	0
#define ISAPNP_TAG_DMA_XFER_PREF_8_OR_16_BIT	1
#define ISAPNP_TAG_DMA_XFER_PREF_16BIT_ONLY	2
	uint8_t		bus_master:1;		/* bit 2 */
	uint8_t		byte_count:1;		/* bit 3 */
	uint8_t		word_count:1;		/* bit 4 */
	uint8_t		dma_speed:2;		/* bit 5-6 */
#define ISAPNP_TAG_DMA_XFER_SPEED_COMPATIBLE	0
#define ISAPNP_TAG_DMA_XFER_SPEED_TYPE_A	1
#define ISAPNP_TAG_DMA_XFER_SPEED_TYPE_B	2
#define ISAPNP_TAG_DMA_XFER_SPEED_TYPE_F	3
	uint8_t		reserved:1;		/* bit 7 */
};

struct isapnp_tag_start_dependent_function { /* WARNING: only typecast tag->data if tag->length > 0! */
	uint8_t		priority;
#define ISAPNP_TAG_SDF_PRIO_GOOD		0
#define ISAPNP_TAG_SDF_PRIO_ACCEPTABLE		1
#define ISAPNP_TAG_SDF_PRIO_SUB_OPTIMAL		2
};

struct isapnp_tag_io_port {
	uint8_t		decode_16bit:1;		/* bit 0: decodes 16-bit ISA addresses */
	uint8_t		reserved1:7;

	uint16_t	min_range,max_range;
	uint8_t		alignment;
	uint8_t		length;
};

struct isapnp_tag_fixed_io_port {
	uint16_t	base;
	uint8_t		length;
};

struct isapnp_tag_fixed_memory_location_32 {
	uint8_t		writeable:1;		/* bit 0 */
	uint8_t		cacheable:1;		/* bit 1 */
	uint8_t		support_hi_addr:1;	/* bit 2 */
	uint8_t		memory_io_size:2;	/* bit 3-4 */
#define ISAPNP_TAG_FML32_MIOSIZE_8BIT_ONLY		0
#define ISAPNP_TAG_FML32_MIOSIZE_16BIT_ONLY		1
#define ISAPNP_TAG_FML32_MIOSIZE_8_16_BIT		2
#define ISAPNP_TAG_FML32_MIOSIZE_32BIT_ONLY		3
	uint8_t		shadowable:1;		/* bit 5 */
	uint8_t		expansion_rom:1;	/* bit 6 */
	uint8_t		mio_reserved:1;		/* bit 7 */
	uint32_t	base,length;
};

struct isapnp_pnp_isa_cfg {
	uint8_t		revision;		/* +0 */
	uint8_t		total_csn;
	uint16_t	isa_pnp_port;
	uint16_t	reserved;
};
#pragma pack(pop)

extern struct isa_pnp_bios_struct	isa_pnp_info;
extern uint16_t				isa_pnp_bios_offset;
extern unsigned int			(__cdecl far *isa_pnp_rm_call)();
extern const unsigned char		isa_pnp_init_keystring[32];

#if TARGET_MSDOS == 32
# define ISA_PNP_RM_DOS_AREA_SIZE 8192
extern uint16_t				isa_pnp_pm_code_seg;	/* code segment to call protected mode BIOS if */
extern uint16_t				isa_pnp_pm_data_seg;	/* data segment to call protected mode BIOS if */
extern uint8_t				isa_pnp_pm_use;		/* 1=call protected mode interface  0=call real mode interface */
extern uint8_t				isa_pnp_pm_win95_mode;	/* 1=call protected mode interface with 32-bit stack (Windows 95 style) */
extern uint16_t				isa_pnp_rm_call_area_code_sel;
extern uint16_t				isa_pnp_rm_call_area_sel;
extern void*				isa_pnp_rm_call_area;
#endif

int init_isa_pnp_bios();
void free_isa_pnp_bios();
int find_isa_pnp_bios();

const char *isa_pnp_device_type(const uint8_t far *b,const char **subtype,const char **inttype);
int isapnp_read_tag(uint8_t far **pptr,uint8_t far *fence,struct isapnp_tag *tag);
void isa_pnp_device_description(char desc[255],uint32_t productid);
void isa_pnp_product_id_to_str(char *str,unsigned long id);
const char *isa_pnp_device_category(uint32_t productid);
const char *isapnp_sdf_priority_str(uint8_t x);
const char *isapnp_tag_str(uint8_t tag);

int isa_pnp_init_key_readback(unsigned char *data/*9 bytes*/);
void isa_pnp_set_read_data_port(uint16_t port);
void isa_pnp_wake_csn(unsigned char id);
unsigned char isa_pnp_read_config();
void isa_pnp_init_key();

/* these I/O ports are fixed */
#define ISAPNP_ADDRESS			0x279
#define ISAPNP_WRITE_DATA		0xA79
/* this port varies, can change at runtime. caller is expected to fill this variable in */
#define ISAPNP_READ_DATA		isapnp_read_data

#define isa_pnp_write_address(x)	outp(ISAPNP_ADDRESS,(uint8_t)(x))
#define isa_pnp_write_data(x)		outp(ISAPNP_WRITE_DATA,(uint8_t)(x))
#define isa_pnp_read_data(x)		inp(ISAPNP_READ_DATA)

extern uint16_t				isapnp_read_data;
extern uint8_t				isapnp_probe_next_csn;

int isa_pnp_read_irq(unsigned int i);
int isa_pnp_read_dma(unsigned int i);
int isa_pnp_read_io_resource(unsigned int i);
uint8_t isa_pnp_read_data_register(uint8_t x);
void isa_pnp_write_dma(unsigned int i,int dma);
void isa_pnp_write_irq(unsigned int i,int irq);
void isa_pnp_write_data_register(uint8_t x,uint8_t data);
void isa_pnp_write_io_resource(unsigned int i,uint16_t port);
void isa_pnp_write_irq_mode(unsigned int i,unsigned int im);

#if TARGET_MSDOS == 32
unsigned int isa_pnp_bios_number_of_sysdev_nodes(unsigned char far *a,unsigned int far *b);
unsigned int isa_pnp_bios_get_sysdev_node(unsigned char far *a,unsigned char far *b,unsigned int c);
unsigned int isa_pnp_bios_send_message(unsigned int msg);
unsigned int isa_pnp_bios_get_static_alloc_resinfo(unsigned char far *a);
unsigned int isa_pnp_bios_get_pnp_isa_cfg(unsigned char far *a);
unsigned int isa_pnp_bios_get_escd_info(unsigned int far *min_escd_write_size,unsigned int far *escd_size,unsigned long far *nv_storage_base);
#else
/* we can call the real-mode subroutine directly, no translation */
/* NTS: __cdecl follows the documented calling convention of this call i.e. Microsoft C++ style */
#  define isa_pnp_bios_number_of_sysdev_nodes(a,b)	((unsigned int (__cdecl far *)(int,unsigned char far *,unsigned int far *,unsigned int))isa_pnp_rm_call)(0,a,b,isa_pnp_info.rm_ent_data_segment)
#  define isa_pnp_bios_get_sysdev_node(a,b,c)		((unsigned int (__cdecl far *)(int,unsigned char far *,unsigned char far *,unsigned int,unsigned int))isa_pnp_rm_call)(1,a,b,c,isa_pnp_info.rm_ent_data_segment)
#  define isa_pnp_bios_send_message(a)			((unsigned int (__cdecl far *)(int,unsigned int,unsigned int))isa_pnp_rm_call)(4,a,isa_pnp_info.rm_ent_data_segment)
#  define isa_pnp_bios_get_static_alloc_resinfo(a)	((unsigned int (__cdecl far *)(int,unsigned char far *,unsigned int))isa_pnp_rm_call)(0x0A,a,isa_pnp_info.rm_ent_data_segment)
#  define isa_pnp_bios_get_pnp_isa_cfg(a)		((unsigned int (__cdecl far *)(int,unsigned char far *,unsigned int))isa_pnp_rm_call)(0x40,a,isa_pnp_info.rm_ent_data_segment)
#  define isa_pnp_bios_get_escd_info(a,b,c)		((unsigned int (__cdecl far *)(int,unsigned int far *,unsigned int far *,unsigned long far *,unsigned int))isa_pnp_rm_call)(0x41,a,b,c,isa_pnp_info.rm_ent_data_segment)
#endif

/* abc = ASCII letters of the alphabet
 * defg = hexadecimal digits */
#define ISAPNP_ID(a,b,c,d,e,f,g) ( \
	 (((a)&0x1FUL)      <<  2UL) | \
	 (((b)&0x1FUL)      >>  3UL) | \
	((((b)&0x1FUL)&7UL) << 13UL) | \
	 (((c)&0x1FUL)      <<  8UL) | \
	 (((d)&0x0FUL)      << 20UL) | \
	 (((e)&0x0FUL)      << 16UL) | \
	 (((f)&0x0FUL)      << 28UL) | \
	 (((g)&0x0FUL)      << 24UL) )

#define ISAPNP_ID_FMATCH(id,a,b,c)	(ISAPNP_ID(a,b,c,0,0,0,0) == ((id) & 0x0000FF7FUL))
#define ISAPNP_ID_LMATCH(id,h)		(ISAPNP_ID(0,0,0,(h>>12)&0xF,(h>>8)&0xF,(h>>4)&0xF,h&0xF) == ((id) & 0xFFFF0000UL))

#endif /* __ISAPNP_ISAPNP_H */

