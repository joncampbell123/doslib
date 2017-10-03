
#include <hw/sndsb/sndsb.h>

#include <hw/dos/tgusmega.h>
#include <hw/dos/tgussbos.h>

#if TARGET_MSDOS == 32
signed char			sndsb_nmi_32_hook = -1;
#endif

#if TARGET_MSDOS == 32
int sb_nmi_32_auto_choose_hook() {
	if (sndsb_nmi_32_hook >= 0)
		return sndsb_nmi_32_hook;

	/* auto-detect SBOS/MEGA-EM and enable nmi reflection if present */
	if (gravis_mega_em_detect(&megaem_info) || gravis_sbos_detect() >= 0)
		return 1;

	return 0;
}
#endif

