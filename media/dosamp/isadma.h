
#if defined(HAS_DMA)
/* ISA DMA buffer */
extern struct dma_8237_allocation*                  isa_dma;
#endif

#if defined(HAS_DMA)
void free_dma_buffer();
int alloc_dma_buffer(uint32_t choice,int8_t ch);
uint32_t soundcard_isa_dma_recommended_buffer_size(soundcard_t sc,uint32_t limit);
int realloc_isa_dma_buffer();
int check_dma_buffer(void);
#endif

