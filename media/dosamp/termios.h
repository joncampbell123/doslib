
#if defined(LINUX)
int free_termios(void);
int init_termios(void);

int getch(void);
int kbhit(void);
#else
static inline int init_termios(void) {
    return 0;
}

static inline int free_termios(void) {
    return 0;
}
#endif

