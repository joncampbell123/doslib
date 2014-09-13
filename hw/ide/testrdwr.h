
enum {
	DRIVE_RW_MODE_CHS=0,
	DRIVE_RW_MODE_CHSMULTIPLE,
	DRIVE_RW_MODE_LBA,
	DRIVE_RW_MODE_LBAMULTIPLE,
	DRIVE_RW_MODE_LBA48,
	DRIVE_RW_MODE_LBA48_MULTIPLE,

	DRIVE_RW_MODE_MAX
};

struct drive_rw_test_info {
	unsigned short int	num_cylinder;		/* likely 0-16383 */
	unsigned short int	num_head,num_sector;	/* likely 0-15 and 0-63 */
	unsigned short int	cylinder;
	unsigned short int	head,sector;
	unsigned short int	multiple_sectors;	/* multiple mode sector count */
	unsigned short int	max_multiple_sectors;
	unsigned short int	read_sectors;		/* how many to read per command */
	uint64_t		max_lba;
	uint64_t		lba;
	unsigned char		mode;			/* DRIVE_RW_MODE_* */
	unsigned int		can_do_lba:1;
	unsigned int		can_do_lba48:1;
	unsigned int		can_do_multiple:1;
};

extern const char *drive_readwrite_test_modes[DRIVE_RW_MODE_MAX];

extern struct drive_rw_test_info drive_rw_test_nfo;

extern char drive_readwrite_test_geo[128];
extern char drive_readwrite_test_chs[128];
extern char drive_readwrite_test_numsec[128];
extern char drive_readwrite_test_mult[128];

int drive_rw_test_mode_supported(struct drive_rw_test_info *nfo);
void do_drive_readwrite_edit_chslba(struct ide_controller *ide,unsigned char which,struct drive_rw_test_info *nfo,unsigned char editgeo);
void do_drive_readwrite_test_choose_mode(struct ide_controller *ide,unsigned char which,struct drive_rw_test_info *nfo);
void do_drive_readwrite_tests(struct ide_controller *ide,unsigned char which);

