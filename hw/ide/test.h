
extern unsigned char		cdrom_read_mode;
extern unsigned char		pio_width_warning;
extern unsigned char		big_scary_write_test_warning;
extern unsigned char		opt_ignore_smartdrv;
extern unsigned char		opt_no_irq;
extern unsigned char		opt_no_pci;
extern unsigned char		opt_no_isapnp;
extern unsigned char		opt_no_isa_probe;
extern unsigned char		opt_irq_mask;
extern unsigned char		opt_irq_chain;

extern char			tmp[1024];
extern uint16_t			ide_info[256];

#if TARGET_MSDOS == 32
extern unsigned char		cdrom_sector[512U*256U];/* ~128KB, enough for 64 CD-ROM sector or 256 512-byte sectors */
#else
# if defined(__LARGE__) || defined(__COMPACT__)
extern unsigned char		cdrom_sector[512U*16U];	/* ~8KB, enough for 4 CD-ROM sector or 16 512-byte sectors */
# else
extern unsigned char		cdrom_sector[512U*8U];	/* ~4KB, enough for 2 CD-ROM sector or 8 512-byte sectors */
# endif
#endif

void do_ide_controller_hook_irq(struct ide_controller *ide);
void do_ide_controller_unhook_irq(struct ide_controller *ide);
void do_ide_controller_enable_irq(struct ide_controller *ide,unsigned char en);
void do_ide_controller_emergency_halt_irq(struct ide_controller *ide);

#if TARGET_MSDOS == 16 && (defined(__COMPACT__) || defined(__SMALL__) || defined(__MEDIUM__))
  /* chop features out of the Compact memory model build to ensure all code fits inside 64KB */
#else
# define ISAPNP
#endif

#if TARGET_MSDOS == 16 && (defined(__COMPACT__) || defined(__SMALL__))
  /* chop features out of the Compact memory model build to ensure all code fits inside 64KB */
#else
# define PCI_SCAN
# define MORE_TEXT
# define ATAPI_ZIP
# define NOP_TEST
# define MISC_TEST
# define MULTIPLE_MODE_MENU
# define PIO_MODE_MENU
# define POWER_MENU
# define TWEAK_MENU
# define PIO_AUTODETECT
# define READ_VERIFY
#endif

