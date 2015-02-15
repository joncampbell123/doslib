
#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/flatreal/flatreal.h>

#define APIC_FLAG_PRESENT		(1U << 0U)
#define APIC_FLAG_GLOBAL_ENABLE		(1U << 1U)
#define APIC_FLAG_PROBE_ON_BOOT_CPU	(1U << 2U)
#define APIC_FLAG_CANT_DETECT		(1U << 6U)
#define APIC_FLAG_PROBED		(1U << 7U)

extern uint32_t				apic_base;
extern unsigned char			apic_flags;
extern const char*			apic_error_str;

/* APIC read/write functions.
 * WARNING: You are responsible for making sure APIC was detected before using these functions!
 * Also, Intel warns about some newer registers requiring aligned access to work properly OR ELSE. */
#if TARGET_MSDOS == 32
static inline uint32_t apic_readd(uint32_t offset) {
	return *((volatile uint32_t*)(apic_base+offset));
}
#else
/* 16-bit real mode versions that rely on Flat Real Mode to work */
static inline uint32_t apic_readd(uint32_t offset) {
	if (flatrealmode_test()) flatrealmode_setup(FLATREALMODE_4GB);
	return flatrealmode_readd(apic_base+offset);
}
#endif

void forget_apic();
int probe_apic();

