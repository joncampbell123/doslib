/* 8237.h
 *
 * Intel 8237 DMA controller library.
 * (C) 2008-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box] */

/* WARNING: As usual for performance reasons this library generally does not
 *          enable/disable interrupts (cli/sti). To avoid contention with
 *          interrupt handlers the calling program should do that. */

#include <hw/cpu/cpu.h>
#include <stdint.h>

/* primary DMA controller present (0x00-0x0F) */
#define D8237_DMA_PRIMARY		0x01
/* secondary DMA controller present (0xC0-0xDF) */
#define D8237_DMA_SECONDARY		0x02
/* EISA/PCI controller extended registers present (0x400-0x4FF) */
#define D8237_DMA_EISA_EXT		0x04
/* Intel 82374 EISA DMA controller */
#define D8237_DMA_82374			0x08
/* Intel 82357 EISA DMA controller */
#define D8237_DMA_82357			0x10
/* page registers are 8 bits wide */
#define D8237_DMA_8BIT_PAGE		0x20

/* registers _R_ = read _W_ = write _RW = r/w */
#define D8237_REG_R_STATUS		0x08
#define D8237_REG_W_COMMAND		0x08
#define D8237_REG_W_REQUEST		0x09
#define D8237_REG_W_SINGLE_MASK		0x0A
#define D8237_REG_W_WRITE_MODE		0x0B
#define D8237_REG_W_CLEAR_FLIPFLOP	0x0C
#define D8237_REG_R_TEMPORARY		0x0D
#define D8237_REG_W_MASTER_CLEAR	0x0D
#define D8237_REG_W_CLEAR_MASK		0x0E
#define D8237_REG_W_WRITE_ALL_MASK	0x0F

/* command register bits */
#define D8237_CMDR_MEM_TO_MEM_ENABLE	0x01
#define D8237_CMDR_CH0_ADDR_HOLD_ENABLE	0x02
#define D8237_CMDR_DISABLE_CONTROLLER	0x04
#define D8237_CMDR_COMPRESSED_TIMING	0x08
#define D8237_CMDR_ROTATING_PRIORITY	0x10
#define D8237_CMDR_EXT_WRITE_SELECTION	0x20
#define D8237_CMDR_DREQ_SENSE_ACTIVE_LOW 0x40
#define D8237_CMDR_DACK_SENSE_ACTIVE_HIGH 0x80

/* mask register */
/* bits 0-1 = which channel to set
 * bit 2 = 1=mask channel 0=unmask channel */
#define D8237_MASK_CHANNEL(x)		((x)&3)
#define D8237_MASK_SET			0x04

/* mode register */
#define D8237_MODER_CHANNEL(x)		((x)&3)
#define D8237_MODER_TRANSFER(x)		(((x)&3) << 2)
#define D8237_MODER_AUTOINIT		0x10
#define D8237_MODER_ADDR_DEC		0x20
#define D8237_MODER_MODESEL(x)		(((x)&3) << 6)

#define D8237_MODER_XFER_VERIFY		0
#define D8237_MODER_XFER_WRITE		1
#define D8237_MODER_XFER_READ		2

#define D8237_MODER_MODESEL_DEMAND	0
#define D8237_MODER_MODESEL_SINGLE	1
#define D8237_MODER_MODESEL_BLOCK	2
#define D8237_MODER_MODESEL_CASCADE	3

/* request register */
#define D8237_REQR_CHANNEL(x)		((x)&3)
#define D8237_REQR_SET			0x04

/* status register */
#define D8237_STATUS_TC(x)		(0x01 << ((x)&3))
#define D8237_STATUS_REQ(x)		(0x10 << ((x)&3))

extern unsigned char d8237_flags;
extern unsigned char d8237_dma_address_bits;
extern unsigned char d8237_dma_counter_bits;
extern uint32_t d8237_dma_address_mask;
extern uint32_t d8237_dma_counter_mask;
extern unsigned char *d8237_page_ioport_map;
extern unsigned char d8237_page_ioport_map_xt[8];
extern unsigned char d8237_page_ioport_map_at[8];
extern unsigned char d8237_16bit_ashift;
extern unsigned char d8237_channels;

static inline unsigned char d8237_ioport(unsigned char ch,unsigned char reg) {
	if (ch & 4)	return 0xC0+(reg<<1);
	else		return 0x00+reg;
}

static inline unsigned char d8237_page_ioport(unsigned char ch) {
	return d8237_page_ioport_map[ch];
}

/*============*/
static inline void d8237_reset_flipflop_ncli(unsigned char ch) {
	/* NOTE TO SELF: This code used to write the port, then read it. Based on some ancient 386SX laptop
	 *               I have who's DMA controller demands it, apparently. But then VirtualBox seems
	 *               to mis-emulate THAT and leave the flip-flop in the wrong state, causing erratic
	 *               DMA readings in the Sound Blaster program. Putting it in this order fixes it. */
	inp(d8237_ioport(ch,0xC));
	outp(d8237_ioport(ch,0xC),0x00); /* A3-0=1100 IOR=0 IOW=1 -> clear byte pointer flip/flop */
}

static inline void d8237_reset_flipflop(unsigned char ch) {
	unsigned int flags = get_cpu_flags();
	_cli();
	d8237_reset_flipflop_ncli(ch);
	_sti_if_flags(flags);
}

/*============*/
static inline uint16_t d8237_read_base_lo16_ncli(unsigned char ch) {
	unsigned char iop = d8237_ioport(ch,(ch&3)*2);
	uint16_t r;

	d8237_reset_flipflop_ncli(ch);
	r  = (uint16_t)inp(iop);
	r |= (uint16_t)inp(iop) << 8U;
	return r;
}

static inline uint16_t d8237_read_base_lo16(unsigned char ch) {
	unsigned int flags = get_cpu_flags();
	uint16_t x;
	_cli();
	x = d8237_read_base_lo16_ncli(ch);
	_sti_if_flags(flags);
	return x;
}

/*============*/
static inline uint16_t d8237_read_count_lo16_direct_ncli(unsigned char ch) {
	unsigned char iop = d8237_ioport(ch,((ch&3)*2) + 1);
	uint16_t r;

	d8237_reset_flipflop_ncli(ch);
	r  = (uint16_t)inp(iop);
	r |= (uint16_t)inp(iop) << 8U;
	return r;
}

static inline uint16_t d8237_read_count_lo16_direct(unsigned char ch) {
	unsigned int flags = get_cpu_flags();
	uint16_t x;
	_cli();
	x = d8237_read_count_lo16_direct_ncli(ch);
	_sti_if_flags(flags);
	return x;
}

/*============*/
static inline void d8237_write_base_lo16_ncli(unsigned char ch,uint16_t o) {
	unsigned char iop = d8237_ioport(ch,(ch&3)*2);
	d8237_reset_flipflop_ncli(ch);
	outp(iop,o);
	outp(iop,o >> 8);
}

static inline void d8237_write_base_lo16(unsigned char ch,uint16_t o) {
	unsigned int flags = get_cpu_flags();
	_cli();
	d8237_write_base_lo16_ncli(ch,o);
	_sti_if_flags(flags);
}

/*============*/
static inline void d8237_write_count_lo16_ncli(unsigned char ch,uint16_t o) {
	unsigned char iop = d8237_ioport(ch,((ch&3)*2) + 1);
	d8237_reset_flipflop_ncli(ch);
	outp(iop,o);
	outp(iop,o >> 8);
}

static inline void d8237_write_count_lo16(unsigned char ch,uint16_t o) {
	unsigned int flags = get_cpu_flags();
	_cli();
	d8237_write_count_lo16_ncli(ch,o);
	_sti_if_flags(flags);
}

/*============*/
int probe_8237();
uint32_t d8237_read_base(unsigned char ch);
void d8237_write_base(unsigned char ch,uint32_t o);
uint32_t d8237_read_count(unsigned char ch);
void d8237_write_count(unsigned char ch,uint32_t o);
uint16_t d8237_read_count_lo16(unsigned char ch);

struct dma_8237_allocation {
	unsigned char FAR*		lin;		/* linear (program accessible pointer) */
							/*   16-bit real mode: _fmalloc()'d buffer */
							/*   32-bit prot mode: dpmi_alloc_dos() in DOS memory, or else allocated linear memory that has been locked and verified sequential in memory */
	uint32_t			phys;		/* physical memory address */
	uint32_t			length;		/* length of the buffer */
#if TARGET_MSDOS == 32
	uint16_t			dos_selector;	/* if allocated by dpmi_alloc_dos() (nonzero) this is the selector */
	uint32_t			lin_handle;	/* if allocated by alloc linear handle, memory handle */
#endif
};

void dma_8237_free_buffer(struct dma_8237_allocation *a);
struct dma_8237_allocation *dma_8237_alloc_buffer(uint32_t len);

