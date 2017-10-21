
/* IRQ state */
struct irq_state_t {
    void                                (interrupt *old_handler)();
    uint8_t                             irq_number;
    uint8_t                             int_number;
    unsigned int                        was_masked:1;
    unsigned int                        chain_irq:1;
    unsigned int                        was_iret:1;             /* prior IRQ handler doesn't ACK IRQs */
    unsigned int                        hooked:1;
};

extern struct irq_state_t               soundcard_irq;

int hook_irq(uint8_t irq,void (interrupt *func)());
int init_prepare_irq(void);
int unhook_irq(void);

