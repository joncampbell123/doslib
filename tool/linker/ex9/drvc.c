
extern const char hellostr2[];

const char hello_world[] = "Hello world. This is code without a C runtime.\r\n";
const char crlf[] = "\r\n";

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

extern unsigned long ex_data32;

unsigned int near entry_c(void) {
    dos_puts(hello_world);
    dos_puts(hellostr2);

    /* in code32.asm there is 32-bit code and data.
     * test that we can access it relative to our 16-bit segment.
     * the 32-bit code/data is linked together as if part of our 16-bit code and data.
     * NTS: Open Watcom's Linker WILL NOT link this test properly, this test will fail. */
    if (ex_data32 == 0x89ABCDEF) {
        dos_puts("EX_DATA32 YES\nEX_DATA32 string: ");
        dos_puts((const char *)(&ex_data32) + 4ul);
        dos_puts(crlf);
    }
    else {
        dos_puts("EX_DATA32 NO");
    }

    return 0;
}

