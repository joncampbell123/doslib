
struct convert_rdbuf_t {
    unsigned char dosamp_FAR *          buffer; // pointer to base
    unsigned int                        size;   // size in bytes (16-bit builds will not exceed 64KN)
    unsigned int                        len;    // length of actual data
    unsigned int                        pos;    // read position of data
};

extern struct convert_rdbuf_t           convert_rdbuf;

unsigned char dosamp_FAR * convert_rdbuf_get(uint32_t *sz);
void convert_rdbuf_clear(void);
void convert_rdbuf_free(void);

