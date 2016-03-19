
#ifndef __DOSLIB_HW_IDE_MPU401LIB_H
#define __DOSLIB_HW_IDE_MPU401LIB_H

#define MPU401_MAX_CARDS			4

/* MPU-401 I/O ports */
/* 0x3x0 + const = I/O port */
#define MPU401_IO_DATA				0x0
#define MPU401_IO_COMMAND			0x1
#define MPU401_IO_STATUS			0x1

/* MPU commands */
#define MPU401_MPUCMD_ENTER_UART		0x3F
#define MPU401_MPUCMD_RESET			0xFF

struct mpu401_ctx {
	uint16_t		baseio;
};

extern struct mpu401_ctx mpu401_card[MPU401_MAX_CARDS];

int init_mpu401();
void free_mpu401();
struct mpu401_ctx *mpu401_by_base(uint16_t x);

int mpu401_try_base(uint16_t iobase);
int mpu401_command(struct mpu401_ctx *cx,uint8_t d);
int mpu401_write(struct mpu401_ctx *cx,uint8_t d);
int mpu401_probe(struct mpu401_ctx *cx);
int mpu401_read(struct mpu401_ctx *cx);
struct mpu401_ctx *mpu401_alloc_card();
void mpu401_free_card(struct mpu401_ctx *c);

#endif /* __DOSLIB_HW_IDE_MPU401LIB_H */

