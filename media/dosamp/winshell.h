
#if defined(TARGET_WINDOWS)
# include <shellapi.h>
/* dynamic loading of MMSYSTEM/WINMM */
extern HMODULE             shell_dll;
extern unsigned char       shell_tried;

extern UINT (WINAPI * __DragQueryFile)(HDROP,UINT,LPSTR,UINT);
extern VOID (WINAPI * __DragAcceptFiles)(HWND,BOOL);
extern VOID (WINAPI * __DragFinish)(HDROP hDrop);

void free_shell(void);
void shell_atexit(void);
int init_shell(void);
#endif

