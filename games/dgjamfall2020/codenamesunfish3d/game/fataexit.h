
void unhook_irqs();
void fatal(const char *msg,...);

extern void (*other_unhook_irq)(void);
 
