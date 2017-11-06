
#if defined(TARGET_WINDOWS)
/* dynamic loading of MMSYSTEM/WINMM */
extern HMODULE             mmsystem_dll;
extern unsigned char       mmsystem_tried;

void free_mmsystem(void);
void mmsystem_atexit(void);
int init_mmsystem(void);
int check_mmsystem_time(void);
#endif

