
void do_construct_atapi_scsi_mmc_read(unsigned char *buf/*must be 12 bytes*/,uint32_t sector,uint32_t tlen_sect,unsigned char read_mode);
unsigned char sanitizechar(unsigned char c);
int wait_for_enter_or_escape();

