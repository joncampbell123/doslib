
extern const char hellostr2[];

const char hello_world[] = "Hello world. This is code without a C runtime.\r\n";

void dos_putc(const char c);
#pragma aux dos_putc = \
    "mov    ah,0x02" \
    "int    21h" \
    modify [ah] \
    parm [dl]

void dos_puts(const char *p) {
    char c;

    while ((c = *p++) != 0)
        dos_putc(c);
}

unsigned int near entry_c(void) {
    dos_puts(hello_world);
    dos_puts(hellostr2);
    return 0;
}

