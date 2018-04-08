
#ifndef __DOSLIB_HW_IDE_FLOPPYLIB_H
#define __DOSLIB_HW_IDE_FLOPPYLIB_H

#define MAX_FLOPPY_CONTROLLER 4

struct floppy_controller {
	uint16_t		base_io;
	int8_t			irq;
	int8_t			dma;

	/* known state */
	uint8_t			ps2_status[2];		/* 0x3F0-0x3F1 */
	uint8_t			st[4];			/* ST0...ST3 */
							/*  Status Register ST0 */
							/*     [7:6] IC (Interrupt code) */
							/*            00 = normal termination, no errors */
							/*            01 = abnormal termination */
							/*            10 = invalid command */
							/*            11 = abnormal termination by polling */
							/*     [5:5] Seek end */
							/*     [4:4] Unit check */
							/*     [3:3] Drive not ready */
							/*     [2:2] Head currently active */
							/*     [1:0] Currently selected drive */
							/*  Status Register ST1 */
							/*     [7:7] End of Cylinder */
							/*     [6:6] always 0 */
							/*     [5:5] Data error */
							/*     [4:4] Time out (data overrun) */
							/*     [3:3] always 0 */
							/*     [2:2] No data */
							/*     [1:1] Not writeable */
							/*     [0:0] No address mark */
							/*  Status Register ST2 */
							/*     [7:7] always 0 */
							/*     [6:6] Deleted Address Mark */
							/*     [5:5] CRC error in data field */
							/*     [4:4] Wrong Cylinder */
							/*     [3:3] Seek equal */
							/*     [2:2] Seek error */
							/*     [1:1] Bad cylinder */
							/*     [0:0] Not data address mark DAM */
							/*  Status Register ST3 */
							/*     [7:7] Error, drive error signal is active */
							/*     [6:6] Write protection */
							/*     [5:5] Ready, drive ready signal is active */
							/*     [4:4] Track 0 */
							/*     [3:3] Double sided drive */
							/*     [2:2] HDSEL (head select) signal from drive */
							/*     [1:0] Drive select */
	uint8_t			digital_out;		/* last value written to 0x3F2 */
							/*     [7:7] MOTD (Motor control, drive D) */
							/*     [6:6] MOTC (Motor control, drive C) */
							/*     [5:5] MOTB (Motor control, drive B) */
							/*     [4:4] MOTA (Motor control, drive A) */
							/*     [3:3] DMA (DMA+IRQ enable) */
							/*     [2:2] !REST (Controller reset) 1=enable 0=reset */
							/*     [1:0] Drive select (0=A 1=B 2=C 3=D) */
	uint8_t			main_status;		/* last value read from 0x3F4 */
							/*     [7:7] MRQ (Main request) 1=data register ready */
							/*     [6:6] DIO (Data input/output) 1=expecting CPU read 0=expecting CPU write */
							/*     [5:5] NDMA (non-DMA mode) 1=not in DMA mode */
							/*     [4:4] BUSY (instruction, device busy) 1=active */
							/*     [3:3] ACTD Drive D in positioning mode */
							/*     [2:2] ACTC Drive C in positioning mode */
							/*     [1:1] ACTB Drive B in positioning mode */
							/*     [0:0] ACTA Drive A in positioning mode */
	uint8_t			digital_in;		/* last value read from 0x3F7 */
							/*     [7:7] CHAN (Disk Change) 1=disk changed since last command  (PS/2 model 30 bit is inverted) */
							/* * PS/2 except Model 30 */
							/*     [6:3] all 1's */
							/* * PS/2 Model 30 */
							/*     [6:4] all 0's */
	uint8_t			control_cfg;		/* last value written to 0x3F7 */
							/*     [1:0] Data transfer rate */
							/*            11 = 1 mbit/sec */
							/*            10 = 250 kbit/sec */
							/*            01 = 300 kbit/sec */
							/*            00 = 500 kbit/sec */
	uint8_t			cylinder;		/* last known cylinder position */
	uint16_t		irq_fired;

	/* desired state */
	uint8_t			use_irq:1;		/* if set, use IRQ. else. ??? */
	uint8_t			use_dma:1;		/* if set, use DMA. else, use PIO data transfer */
	uint8_t			use_implied_seek:1;	/* if set, act like controller has Implied Seek enabled */

	/* what we know about the controller */
	uint8_t			version;		/* result of command 0x10 (determine controller version) or 0x00 if not yet queried */
	uint8_t			ps2_mode:1;		/* controller is in PS/2 mode (has status registers A and B I/O ports) */
	uint8_t			at_mode:1;		/* controller is in AT mode (has Digital Input Register and Control Config Reg) */
	uint8_t			digital_out_rw:1;	/* digital out (0x3F2) is readable */
	uint8_t			non_dma_mode:1;		/* "ND" bit in Dump Regs */
	uint8_t			implied_seek:1;		/* "EIS" bit in Dump Regs */
	uint8_t			current_srt:4;
	uint8_t			current_hut:4;
	uint8_t			current_hlt;
};

extern struct floppy_controller     floppy_standard_isa[2];
extern int8_t                       floppy_controllers_enable_2nd;

extern struct floppy_controller     floppy_controllers[MAX_FLOPPY_CONTROLLER];
extern int8_t                       floppy_controllers_init;

struct floppy_controller *floppy_get_standard_isa_port(int x);
struct floppy_controller *floppy_get_controller(int x);
struct floppy_controller *alloc_floppy_controller();
void floppy_controller_read_ps2_status(struct floppy_controller *i);

static inline uint8_t floppy_controller_read_status(struct floppy_controller *i) {
	i->main_status = inp(i->base_io+4); /* 0x3F4 main status */
	return i->main_status;
}

static inline uint8_t floppy_controller_read_DIR(struct floppy_controller *i) {
	if (i->at_mode || i->ps2_mode)
		i->digital_in = inp(i->base_io+7); /* 0x3F7 */
	else
		i->digital_in = 0xFF;

	return i->digital_in;
}

static inline void floppy_controller_write_DOR(struct floppy_controller *i,unsigned char c) {
	i->digital_out = c;
	outp(i->base_io+2,c);	/* 0x3F2 Digital Output Register */
}

static inline void floppy_controller_write_CCR(struct floppy_controller *i,unsigned char c) {
	i->control_cfg = c;
	outp(i->base_io+7,c);	/* 0x3F7 Control Configuration Register */
}

#endif /* __DOSLIB_HW_IDE_FLOPPYLIB_H */

