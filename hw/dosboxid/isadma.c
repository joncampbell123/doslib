
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/8237/8237.h>		/* 8237 timer */
#include <hw/8254/8254.h>		/* 8254 timer */

#include <hw/dosboxid/iglib.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

static struct dma_8237_allocation *sb_dma = NULL; /* DMA buffer */

int main(int argc,char **argv,char **envp) {
	unsigned short tmpr[8];
	unsigned int ch;
	unsigned int i;

	(void)argc;
	(void)argv;
	(void)envp;

	if (argc < 2) {
		printf("ISADMA <channel>\n");
		return 1;
	}
	ch = (unsigned int)strtoul(argv[1],NULL,0);
	if (ch > 7) return 1;

	if (!probe_8237()) {
		printf("Chip not present. Your computer might be 2010-era hardware that dropped support for it.\n");
		return 1;
	}

	probe_dos();
	detect_windows();

	if (windows_mode == WINDOWS_NT) {
		printf("This program is not compatible with Windows NT\n");
		return 1;
	}

	if (!probe_dosbox_id()) {
		printf("DOSBox integration device not found\n");
		return 1;
	}
	printf("DOSBox integration device found at I/O port %xh\n",dosbox_id_baseio);

	if (ch >= 4)
		sb_dma = dma_8237_alloc_buffer_dw(4096,16);
	else
		sb_dma = dma_8237_alloc_buffer_dw(4096,8);

	if (sb_dma == NULL) {
		printf("Unable to allocate DMA buffer\n");
		return 1;
	}

#if TARGET_MSDOS == 32
	printf("DMA buffer %p\n",sb_dma->lin);
#else
	printf("DMA buffer %Fp\n",sb_dma->lin);
#endif

	/*=====================================================================================*/
	/* write test to memory */
	outp(d8237_ioport(ch,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(ch) | D8237_MASK_SET); /* mask */

	outp(d8237_ioport(ch,D8237_REG_W_WRITE_MODE),
			D8237_MODER_CHANNEL(ch) |
			D8237_MODER_TRANSFER(D8237_MODER_XFER_WRITE) |
			D8237_MODER_MODESEL(D8237_MODER_MODESEL_SINGLE));

	d8237_write_count(ch,4096);
	d8237_write_base(ch,sb_dma->phys);
	sb_dma->lin[0] = 0;
	sb_dma->lin[1] = 0;
	sb_dma->lin[2] = 0;
	sb_dma->lin[3] = 0;
	sb_dma->lin[4] = 0;
	sb_dma->lin[5] = 0;
	sb_dma->lin[6] = 0;
	sb_dma->lin[7] = 0;

	outp(d8237_ioport(ch,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(ch)); /* unmask */

	dosbox_id_reset_latch();
	dosbox_id_write_regsel(DOSBOX_ID_REG_8237_INJECT_WRITE);
	dosbox_id_write_data(((uint32_t)ch << (uint32_t)16) | 0x9911);
	dosbox_id_write_data(((uint32_t)ch << (uint32_t)16) | 0xAA22);
	dosbox_id_write_data(((uint32_t)ch << (uint32_t)16) | 0xBB33);
	dosbox_id_write_data(((uint32_t)ch << (uint32_t)16) | 0xCC44);

	/* If 8-bit DMA, the memory pattern should be 4 bytes 0x11 0x22 0x33 0x44 */
	/* If 16-bit DMA, the memory pattern should be 8 bytes 0x11 0x99 0x22 0xAA 0x33 0xBB 0x44 0xCC */
	if (ch >= 4) {
		if (sb_dma->lin[0] == 0x11 && sb_dma->lin[1] == 0x99 &&
				sb_dma->lin[2] == 0x22 && sb_dma->lin[3] == 0xAA &&
				sb_dma->lin[4] == 0x33 && sb_dma->lin[5] == 0xBB &&
				sb_dma->lin[6] == 0x44 && sb_dma->lin[7] == 0xCC) {
			printf("ISA DMA write to memory test passed\n");
		}
		else {
			printf("ISA DMA write to memory test failed\n");
		}
	}
	else {
		if (sb_dma->lin[0] == 0x11 &&
				sb_dma->lin[1] == 0x22 &&
				sb_dma->lin[2] == 0x33 &&
				sb_dma->lin[3] == 0x44) {
			printf("ISA DMA write to memory test passed\n");
		}
		else {
			printf("ISA DMA write to memory test failed\n");
		}
	}

	for (i=0;i < 8;i++) printf("%02x ",sb_dma->lin[i]);
	printf("\n");
	/*=====================================================================================*/


	/*=====================================================================================*/
	/* read test from memory */
	outp(d8237_ioport(ch,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(ch) | D8237_MASK_SET); /* mask */

	outp(d8237_ioport(ch,D8237_REG_W_WRITE_MODE),
			D8237_MODER_CHANNEL(ch) |
			D8237_MODER_TRANSFER(D8237_MODER_XFER_READ) |
			D8237_MODER_MODESEL(D8237_MODER_MODESEL_SINGLE));

	d8237_write_count(ch,4096);
	d8237_write_base(ch,sb_dma->phys);
	if (ch >= 4) {
		sb_dma->lin[0] = 0x44;
		sb_dma->lin[1] = 0xCC;
		sb_dma->lin[2] = 0x55;
		sb_dma->lin[3] = 0xDD;
		sb_dma->lin[4] = 0x66;
		sb_dma->lin[5] = 0xEE;
		sb_dma->lin[6] = 0x77;
		sb_dma->lin[7] = 0xFF;
	}
	else {
		sb_dma->lin[0] = 0x44;
		sb_dma->lin[1] = 0x55;
		sb_dma->lin[2] = 0x66;
		sb_dma->lin[3] = 0x77;
	}

	outp(d8237_ioport(ch,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(ch)); /* unmask */

	for (i=0;i < 4;i++) {
		dosbox_id_reset_latch();
		dosbox_id_write_regsel(DOSBOX_ID_REG_8237_INJECT_READ);
		dosbox_id_write_data(((uint32_t)ch << (uint32_t)16));
		tmpr[i] = (unsigned short)(dosbox_id_read_data() & 0xFFFFu);
	}

	/* If 8-bit DMA, the memory pattern read should be 0x44 0x55 0x66 0x77 */
	/* If 16-bit DMA, the memory pattern read should be 0xCC44 0xDD55 0xEE66 0xFF77 */
	if (ch >= 4) {
		if (tmpr[0] == 0xCC44u &&
				tmpr[1] == 0xDD55u &&
				tmpr[2] == 0xEE66u &&
				tmpr[3] == 0xFF77u) {
			printf("ISA DMA read from memory test passed\n");
		}
		else {
			printf("ISA DMA read from memory test failed\n");
		}
	}
	else {
		if (tmpr[0] == 0x44 &&
				tmpr[1] == 0x55 &&
				tmpr[2] == 0x66 &&
				tmpr[3] == 0x77) {
			printf("ISA DMA read from memory test passed\n");
		}
		else {
			printf("ISA DMA read from memory test failed\n");
		}
	}

	for (i=0;i < 4;i++) printf("%04x ",tmpr[i]);
	printf("\n");
	/*=====================================================================================*/

	dma_8237_free_buffer(sb_dma);
	return 0;
}

