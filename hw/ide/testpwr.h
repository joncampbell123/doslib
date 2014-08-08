
void do_drive_idle_test(struct ide_controller *ide,unsigned char which);
void do_drive_sleep_test(struct ide_controller *ide,unsigned char which);
void do_drive_standby_test(struct ide_controller *ide,unsigned char which);
void do_drive_check_power_mode(struct ide_controller *ide,unsigned char which);
void do_drive_device_reset_test(struct ide_controller *ide,unsigned char which);

void do_drive_power_states_test(struct ide_controller *ide,unsigned char which);

