
#include <stdint.h>

extern uint16_t *sin2048fps16_table;

int sin2048fps16_lookup(unsigned int a);
void sin2048fps16_free(void);

/* cos(x) is just sin(x) shifted by pi/2 (90 degrees) */
inline int cos2048fps16_lookup(unsigned int a) {
    return sin2048fps16_lookup(a + 0x800u);
}

