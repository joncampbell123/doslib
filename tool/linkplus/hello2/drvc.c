
static const char hello_world[] = "Hello world!\r\n";

unsigned int _dos_write_small(short fd,const void *buffer,unsigned int count);
#if defined(__LARGE__) || defined(__COMPACT__)
#pragma aux _dos_write_small = \
    "mov    ah,0x40" \
    "int    21h" \
    modify [ax] \
    parm [bx] [ds dx] [cx] \
    value [ax]
#else
#pragma aux _dos_write_small = \
    "mov    ah,0x40" \
    "int    21h" \
    modify [ax] \
    parm [bx] [dx] [cx] \
    value [ax]
#endif

unsigned int near entry_c(void) {
    _dos_write_small(1/*STDOUT*/,hello_world,sizeof(hello_world)-1u);
    return 0;
}

