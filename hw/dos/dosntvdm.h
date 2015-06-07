
#include <hw/cpu/cpu.h>
#include <stdint.h>

#if !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
/* NTVDM.EXE DOSNTAST.VDD call support */
#include <windows/ntvdm/ntvdmlib.h>
#endif

#if defined(NTVDM_CLIENT) && !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
extern uint8_t ntvdm_dosntast_tried;
extern uint16_t ntvdm_dosntast_handle;
#endif

#if defined(NTVDM_CLIENT) && !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
extern uint16_t ntvdm_dosntast_io_base;

int ntvdm_dosntast_init();
void ntvdm_dosntast_unload();
uint32_t ntvdm_dosntast_GetTickCount();
int ntvdm_dosntast_MessageBox(const char *text);
unsigned int ntvdm_dosntast_waveOutGetNumDevs();
unsigned int ntvdm_dosntast_getversionex(OSVERSIONINFO *ovi);
uint32_t ntvdm_dosntast_waveOutGetDevCaps(uint32_t uDeviceID,WAVEOUTCAPS *pwoc,uint16_t cbwoc);
#endif

