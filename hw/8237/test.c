
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/8237/8237.h>

int main() {
    unsigned int i;

    if (!probe_8237()) {
        printf("Chip not present. Your computer might be 2010-era hardware that dropped support for it.\n");
        return 1;
    }

    printf("DMA flags:\n");
    if (d8237_flags & D8237_DMA_PRIMARY)
        printf("   DMA_PRIMARY\n");
    if (d8237_flags & D8237_DMA_SECONDARY)
        printf("   DMA_SECONDARY\n");
    if (d8237_flags & D8237_DMA_EISA_EXT)
        printf("   DMA_EISA_EXT\n");
    if (d8237_flags & D8237_DMA_82374)
        printf("   DMA_82374\n");
    if (d8237_flags & D8237_DMA_82357)
        printf("   DMA_82357\n");
    if (d8237_flags & D8237_DMA_8BIT_PAGE)
        printf("   DMA_8BIT_PAGE\n");
    if (d8237_flags & D8237_DMA_16BIT_CANDO_128K)
        printf("   DMA_16BIT_CANDO_128K\n");

    printf("DMA address bits: %u\n",d8237_dma_address_bits);
    printf("DMA counter bits: %u\n",d8237_dma_counter_bits);
    printf("DMA address mask: 0x%08lx\n",d8237_dma_address_mask);
    printf("DMA counter mask: 0x%08lx\n",d8237_dma_counter_mask);
    printf("DMA 16-bit shift on channels: ");
    for (i=0;i < d8237_channels;i++) {
        if (d8237_16bit_ashift & (1 << i))
            printf("%u ",i);
    }
    printf("\n");
    printf("DMA channels: %u\n",d8237_channels);
    printf("DMA channel page I/O port list: ");
    for (i=0;i < d8237_channels;i++) printf("0x%02x ",d8237_page_ioport_map[i]);
    printf("\n");
    return 0;
}

/* vim: set tabstop=4 softtabstop=4 shiftwidth=4 expandtab */

