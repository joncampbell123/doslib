
extern unsigned char			cpu_sse_usable;
extern unsigned char			cpu_sse_usable_probed;
extern const char*			cpu_sse_unusable_reason;
extern unsigned char			cpu_sse_usable_can_turn_on;
extern unsigned char			cpu_sse_usable_can_turn_off;
extern const char*			cpu_sse_usable_detection_method;

int cpu_check_sse_is_usable();
int cpu_sse_disable();
int cpu_sse_enable();

