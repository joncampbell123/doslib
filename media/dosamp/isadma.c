
#if defined(TARGET_WINDOWS)
# define HW_DOS_DONT_DEFINE_MMSYSTEM
# include <windows.h>
#endif

#include <stdio.h>
#include <stdint.h>
#ifndef LINUX
#include <conio.h>
#endif

#ifndef LINUX
#include <hw/8237/8237.h>       /* 8237 DMA */
#endif

#include "dosamp.h"
#include "sndcard.h"
#include "isadma.h"

extern soundcard_t                              soundcard;

#if defined(HAS_DMA)
/* ISA DMA buffer */
struct dma_8237_allocation*                     isa_dma = NULL;
#endif

#if defined(HAS_DMA)
void free_dma_buffer() {
    if (isa_dma != NULL) {
        soundcard->ioctl(soundcard,soundcard_ioctl_isa_dma_assign_buffer,NULL,NULL,0); /* disassociate DMA buffer from sound card */
        dma_8237_free_buffer(isa_dma);
        isa_dma = NULL;
    }
}
#endif

#if defined(HAS_DMA)
int alloc_dma_buffer(uint32_t choice,int8_t ch) {
    if (ch < 0)
        return 0;
    if (isa_dma != NULL)
        return 0;

    {
        const unsigned char isa_width = (ch >= 4 ? 16 : 8);

        do {
            isa_dma = dma_8237_alloc_buffer_dw(choice,isa_width);

            if (isa_dma != NULL) {
                break;
            }
            else {
                if (choice >= 8192UL)
                    choice -= 4096UL;
                else
                    return -1;
            }
        } while (1);
    }

    /* assume isa_dma != NULL */
    return 0;
}
#endif

#if defined(HAS_DMA)
uint32_t soundcard_isa_dma_recommended_buffer_size(soundcard_t sc,uint32_t limit) {
    unsigned int sz = sizeof(uint32_t);
    uint32_t p = 0;

    (void)limit;
    (void)sc;

    if (soundcard->ioctl(soundcard,soundcard_ioctl_isa_dma_recommended_buffer_size,&p,&sz,0) < 0)
        return 0;

    return p;
}
#endif

#if defined(HAS_DMA)
int realloc_isa_dma_buffer() {
    uint32_t choice;
    int8_t ch;

    free_dma_buffer();

    ch = soundcard->ioctl(soundcard,soundcard_ioctl_isa_dma_channel,NULL,NULL,0);
    if (ch < 0)
        return 0; /* nothing to do */

    choice = soundcard_isa_dma_recommended_buffer_size(soundcard,/*no limit*/0);
    if (choice == 0)
        return -1;

    if (alloc_dma_buffer(choice,ch) < 0)
        return -1;

    {
        unsigned int sz = sizeof(*isa_dma);

        if (soundcard->ioctl(soundcard,soundcard_ioctl_isa_dma_assign_buffer,(void dosamp_FAR*)isa_dma,&sz,0) < 0) {
            free_dma_buffer();
            return -1;
        }
    }

    return 0;
}
#endif

#if defined(HAS_DMA)
int check_dma_buffer(void) {
    if ((soundcard->requirements & soundcard_requirements_isa_dma) ||
        (soundcard->capabilities & soundcard_caps_isa_dma)) {
        int8_t ch;

        /* alloc DMA buffer.
         * if already allocated, then realloc if changing from 8-bit to 16-bit DMA */
        if (isa_dma == NULL)
            realloc_isa_dma_buffer();
        else {
            ch = soundcard->ioctl(soundcard,soundcard_ioctl_isa_dma_channel,NULL,NULL,0);
            if (ch >= 0 && isa_dma->dma_width != (ch >= 4 ? 16 : 8))
                realloc_isa_dma_buffer();
        }

        if (isa_dma == NULL)
            return -1;
    }

    return 0;
}
#endif

