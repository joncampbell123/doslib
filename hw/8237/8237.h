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
#define D8237_DMA_PRIMARY           0x01
/* secondary DMA controller present (0xC0-0xDF) */
#define D8237_DMA_SECONDARY         0x02
/* EISA/PCI controller extended registers present (0x400-0x4FF) */
#define D8237_DMA_EISA_EXT          0x04
/* Intel 82374 EISA DMA controller */
#define D8237_DMA_82374             0x08
/* Intel 82357 EISA DMA controller */
#define D8237_DMA_82357             0x10
/* page registers are 8 bits wide */
#define D8237_DMA_8BIT_PAGE         0x20
/* 16-bit DMA channels can span a 128K region */
/* NTS: I found the IBM PS/1 technical reference where IBM specifically documents
 *      that for DMA channels 5-7, bits [7:1] become A23-A17 and the WORD counter
 *      becomes bits [16:1] thus 16-bit DMA can span 128KB (65536 16-bit WORDs) */
#define D8237_DMA_16BIT_CANDO_128K  0x40

/* NTS: There is generally no way to detect if the DMA controller can do 16-bit DMA across 128K,
 *      so for safety reason we assume that's not supported. We leave that up to the calling program
 *      to determine by other means (user options, motherboard detect, ISA Plug & Play, etc.).
 *
 *      The reason for this uncertainty is that, while it's known 16-bit DMA shifts the address bits
 *      to the left by 1, it's not as understood across chipsets how the chipset deals with the MSB
 *      (bit 15) or how it allows 128KB to work. There are two theoretical possibilities:
 *
 *      1) The DMA controller shifts the address bits to the left by 1, discarding bit 15, then latches
 *         the remaining bits to bits 1-15 of the ISA bus along with all 8 bits of the page register.
 *
 *         This is simpler, but limits the DMA to a 64KB range.
 *
 *         This is what Intel's PCI-based motherboard chipsets do according to some datasheets.
 *
 *      2) The DMA controller shifts the address bits to the left by 1, latches all 16 as bits 1-16 to
 *         the ISA bus, then passes the upper 7 bits of the page register as bits 17-23 to the ISA bus.
 *         Bit 0 of the page register is ignored as part of the process.
 *
 *         This is also fairly simple, and it allows the DMA to cover a 128KB range. However it can
 *         also confuse novice programmers who expect all 8 bits of the page register to be used.
 *
 *         This is not confirmed at this time, but supposedly many legacy ISA systems (486 and earlier)
 *         operate this way.
 *
 *      Note that DOSLIB in it's default state will operate with either one because it assigns the page
 *      and address as follows:
 *
 *      page = (physical address >> 16) & 0xFF
 *      addr = (physical address >> 1) & 0xFFFF
 *
 *      note that this puts the 16th bit of the address in both bit 0 of the page register AND in the
 *      15th bit of the address. thus, whether the DMA controller ignores bit 15 of the address register,
 *      or bit 0 of the page register, is irrelevent, it works regardless.
 *
 *      DOSLIB can be asked by the program to mask either bit when writing to help test what the
 *      motherboard chipset does. */

/* registers _R_ = read _W_ = write _RW = r/w */
#define D8237_REG_R_STATUS          0x08
#define D8237_REG_W_COMMAND         0x08
#define D8237_REG_W_REQUEST         0x09
#define D8237_REG_W_SINGLE_MASK     0x0A
#define D8237_REG_W_WRITE_MODE      0x0B
#define D8237_REG_W_CLEAR_FLIPFLOP  0x0C
#define D8237_REG_R_TEMPORARY       0x0D
#define D8237_REG_W_MASTER_CLEAR    0x0D
#define D8237_REG_W_CLEAR_MASK      0x0E /* Does not appear in original 8237/8237-2 datasheet */
#define D8237_REG_W_WRITE_ALL_MASK  0x0F

/* command register bits */
#define D8237_CMDR_MEM_TO_MEM_ENABLE    0x01
#define D8237_CMDR_CH0_ADDR_HOLD_ENABLE 0x02
#define D8237_CMDR_DISABLE_CONTROLLER   0x04
#define D8237_CMDR_COMPRESSED_TIMING    0x08
#define D8237_CMDR_ROTATING_PRIORITY    0x10
#define D8237_CMDR_EXT_WRITE_SELECTION  0x20
#define D8237_CMDR_DREQ_SENSE_ACTIVE_LOW 0x40
#define D8237_CMDR_DACK_SENSE_ACTIVE_HIGH 0x80

/* mask register */
/* bits 0-1 = which channel to set
 * bit 2 = 1=mask channel 0=unmask channel */
#define D8237_MASK_CHANNEL(x)       ((x)&3)
#define D8237_MASK_SET              0x04

/* mode register */
#define D8237_MODER_CHANNEL(x)      ((x)&3)
#define D8237_MODER_TRANSFER(x)     (((x)&3) << 2)
#define D8237_MODER_AUTOINIT        0x10
#define D8237_MODER_ADDR_DEC        0x20
#define D8237_MODER_MODESEL(x)      (((x)&3) << 6)

#define D8237_MODER_XFER_VERIFY     0
#define D8237_MODER_XFER_WRITE      1
#define D8237_MODER_XFER_READ       2

#define D8237_MODER_MODESEL_DEMAND  0
#define D8237_MODER_MODESEL_SINGLE  1
#define D8237_MODER_MODESEL_BLOCK   2
#define D8237_MODER_MODESEL_CASCADE 3

/* request register */
#define D8237_REQR_CHANNEL(x)       ((x)&3)
#define D8237_REQR_SET              0x04

/* status register bitmasks */
/* NOTE: Reading the status register clears the TC bits. Read once to a var and process each bit from there. */
#define D8237_STATUS_TC(x)          (0x01 << ((x)&3))
#define D8237_STATUS_REQ(x)         (0x10 << ((x)&3))

// this library can be asked to mask out the 16th bit of 16-bit DMA addresses to help test
// which bit the motherboard's DMA controller uses. all address are masked by these variables.
extern unsigned char d8237_16bit_pagemask;
extern unsigned short d8237_16bit_addrmask;

extern unsigned char d8237_flags;
extern unsigned char d8237_dma_address_bits;
extern unsigned char d8237_dma_counter_bits;
extern uint32_t d8237_dma_address_mask;
extern uint32_t d8237_dma_counter_mask;
extern unsigned char *d8237_page_ioport_map;
extern unsigned char d8237_16bit_ashift;
extern unsigned char d8237_channels;

#ifdef TARGET_PC98
extern unsigned char d8237_page_ioport_map_pc98[8];
#else
extern unsigned char d8237_page_ioport_map_xt[8];
extern unsigned char d8237_page_ioport_map_at[8];
#endif

static inline unsigned char d8237_ioport(unsigned char ch,unsigned char reg) {
    (void)ch;
#ifdef TARGET_PC98
/* 0x01-0x1F odd ports */
    return 0x01+(reg<<1);
#else
/* 0x00-0x0F, 0xC0-0xDF even ports */
    if (ch & 4) return 0xC0+(reg<<1);
    else        return 0x00+reg;
#endif
}

static inline unsigned char d8237_can_do_16bit_128k() {
    return (d8237_flags & D8237_DMA_16BIT_CANDO_128K);
}

static inline uint32_t d8237_8bit_limit_mask() {
    return 0xFFFFUL;
}

uint32_t d8237_16bit_limit_mask();

static inline void d8237_16bit_set_unknown_mask() {
    // set: unknown DMA controller mapping, therefore, write all bits to addr and mask.
    //      the 16th bit of the address will end up in both bit 15 of addr and bit 0 of page.
    d8237_16bit_addrmask = 0xFFFFU;
    d8237_16bit_pagemask = 0xFFU;
}

static inline void d8237_16bit_set_addr_mask() {
    // set: address masking. DMA controller (according to calling program) handles 16-bit
    //      DMA by shifting addr to left by 1, and discarding bit 15. therefore, zero out
    //      bit 15 of addr when we write the base.
    d8237_16bit_addrmask = 0x7FFFU; // discard 15th bit of addr
    d8237_16bit_pagemask = 0xFFU; // keep bit 0 of page
}

static inline void d8237_16bit_set_page_mask() {
    // set: page masking. DMA controller (according to calling program) handles 16-bit
    //      DMA by shifting addr to left by 1, making bits 0-15 become bits 1-16.
    //      bit 0 of the page register is ignored.
    d8237_16bit_addrmask = 0xFFFFU; // keep bit 15 of addr
    d8237_16bit_pagemask = 0xFEU; // discard bit 0 of page
}

static inline void d8237_enable_16bit_128kb_dma() {
    if (d8237_flags & D8237_DMA_SECONDARY)
        d8237_flags |= D8237_DMA_16BIT_CANDO_128K;
}

static inline void d8237_disable_16bit_128kb_dma() {
    d8237_flags &= ~D8237_DMA_16BIT_CANDO_128K;
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
    SAVE_CPUFLAGS( _cli() ) {
        d8237_reset_flipflop_ncli(ch);
    } RESTORE_CPUFLAGS();
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
    uint16_t x;

    SAVE_CPUFLAGS( _cli() ) {
        x = d8237_read_base_lo16_ncli(ch);
    } RESTORE_CPUFLAGS();

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
    uint16_t x;

    SAVE_CPUFLAGS( _cli() ) {
        x = d8237_read_count_lo16_direct_ncli(ch);
    } RESTORE_CPUFLAGS();

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
    SAVE_CPUFLAGS( _cli() ) {
        d8237_write_base_lo16_ncli(ch,o);
    } RESTORE_CPUFLAGS();
}

/*============*/
static inline void d8237_write_count_lo16_ncli(unsigned char ch,uint16_t o) {
    unsigned char iop = d8237_ioport(ch,((ch&3)*2) + 1);
    d8237_reset_flipflop_ncli(ch);
    outp(iop,o);
    outp(iop,o >> 8);
}

static inline void d8237_write_count_lo16(unsigned char ch,uint16_t o) {
    SAVE_CPUFLAGS( _cli() ) {
        d8237_write_count_lo16_ncli(ch,o);
    } RESTORE_CPUFLAGS();
}

/*============*/
int probe_8237();
uint32_t d8237_read_base(unsigned char ch);
void d8237_write_base(unsigned char ch,uint32_t o);
uint32_t d8237_read_count(unsigned char ch);
void d8237_write_count(unsigned char ch,uint32_t o);
uint16_t d8237_read_count_lo16(unsigned char ch);

#pragma pack(push,2) // word align, regardless of host program's -zp option, to keep struct consistent
struct dma_8237_allocation {
    unsigned char FAR*        lin;          /* linear (program accessible pointer) */
                                            /*   16-bit real mode: _fmalloc()'d buffer */
                                            /*   32-bit prot mode: dpmi_alloc_dos() in DOS memory, or else allocated linear memory that has been locked and verified sequential in memory */
    uint32_t            phys;               /* physical memory address */
    uint32_t            length;             /* length of the buffer */
#if TARGET_MSDOS == 32
    uint32_t            lin_handle;         /* if allocated by alloc linear handle, memory handle */
    uint16_t            dos_selector;       /* if allocated by dpmi_alloc_dos() (nonzero) this is the selector */
#endif
    uint8_t             dma_width;
};
#pragma pack(pop)

void dma_8237_free_buffer(struct dma_8237_allocation *a);
struct dma_8237_allocation *dma_8237_alloc_buffer(uint32_t len);
struct dma_8237_allocation *dma_8237_alloc_buffer_dw(uint32_t len,uint8_t dma_width);

/* vim: set tabstop=4 softtabstop=4 shiftwidth=4 expandtab */

