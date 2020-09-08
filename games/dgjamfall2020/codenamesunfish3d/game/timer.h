
#include <stdint.h>

extern uint32_t timer_counter;
extern uint16_t timer_tick_rate;

/* must disable interrupts temporarily to avoid incomplete read */
uint32_t read_timer_counter();
void init_timer_irq();
void restore_timer_irq();

