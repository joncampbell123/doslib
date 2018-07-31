
static const char hello_world[] = "Hello world!\r\n";

void _dos_putc(const char c);
#pragma aux _dos_putc = \
    "mov    ah,0x02" \
    "int    21h" \
    modify [ah] \
    parm [dl]

void _dos_puts(const char *p) {
    char c;

    while ((c = *p++) != 0)
        _dos_putc(c);
}

unsigned int near entry_c(void) {
    _dos_puts(hello_world);
    return 0;
}

