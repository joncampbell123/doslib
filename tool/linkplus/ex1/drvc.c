
const char hello_world[] = "Hello world. This is code without a C runtime.\r\n";

void dos_putc(const char c);
#pragma aux dos_putc = \
    "mov    ah,0x02" \
    "int    21h" \
    modify [ah] \
    parm [dl]

unsigned int near entry_c(void) {
    {
        const char *p = hello_world;
        char c;

        while ((c = *p++) != 0)
            dos_putc(c);
    }

    return 0;
}

