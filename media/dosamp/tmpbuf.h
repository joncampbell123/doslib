
extern unsigned char dosamp_FAR *           tmpbuffer;
extern size_t                               tmpbuffer_sz;

void tmpbuffer_free(void);
unsigned char dosamp_FAR * tmpbuffer_get(uint32_t *sz);

