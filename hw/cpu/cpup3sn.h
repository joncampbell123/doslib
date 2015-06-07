
#include <stdint.h>
#include <dos.h>

#pragma pack(push,1)
struct cpu_serial_number {
	uint32_t	raw[4];	/* EDX, ECX, EBX, EAX */
};
#pragma pack(pop)

/* WARNING: Caller is expected to check CPUID information to ensure the
            processor supports this feature! */
void cpu_ask_serial();
void cpu_disable_serial();
extern struct cpu_serial_number cpu_serial;

