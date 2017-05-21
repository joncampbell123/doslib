
//#define MEM_DEBUG

#if defined(MEM_DEBUG)
#define realloc(x,y) debug_realloc(x,y)
#define calloc(n,s) debug_calloc(n,s)
#define malloc(x) debug_malloc(x)
#define free(x) debug_free(x)

void *debug_realloc(void *p,size_t sz);
void *debug_calloc(size_t n,size_t sz);
void *debug_malloc(size_t sz);
void debug_free(void *p);
#endif

