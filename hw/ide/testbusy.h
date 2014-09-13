
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/vga/vga.h>
#include <hw/vga/vgagui.h>
#include <hw/vga/vgatty.h>
#include <hw/ide/idelib.h>

#include "testutil.h"
#include "test.h"

int do_ide_controller_user_wait_busy_timeout_controller(struct ide_controller *ide,unsigned int timeout/*ms*/); /* returns 1 if timed out */
int do_ide_controller_user_wait_irq(struct ide_controller *ide,uint16_t count);
int do_ide_controller_user_wait_busy_controller(struct ide_controller *ide);
int do_ide_controller_user_wait_drive_ready(struct ide_controller *ide);
int do_ide_controller_user_wait_drive_drq(struct ide_controller *ide);

