/* winfcon.h
 *
 * Fake console for Windows applications where a console is not available.
 * (C) 2011-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

/* This code only works for Windows 3.0 and higher, and only in protected mode */
#if WINVER >= 0x300

/* Windows 3.1 and earlier, even for Win32s: there is no system given console.
 * We either have to draw and maintain our own window, or find some other way to printf() and display it. */
#if defined(TARGET_WINDOWS) && (TARGET_MSDOS == 16 || (TARGET_MSDOS == 32 && TARGET_WINDOWS == 31) || (defined(WIN386) && TARGET_MSDOS == 32))
# define WIN_STDOUT_CONSOLE
# define getch _win_getch
# define kbhit _win_kbhit
# define fflush _win_fflush
# define fprintf _win_fprintf
# define printf _win_printf
# define isatty _win_isatty
# define write _win_write
# define read _win_read

# include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void _win_pump();
int _win_kbhit();
int _win_getch();
HWND _win_hwnd();
void _gdi_pause();
void _win_pump_wait();
void _win_putc(char c);
int _win_isatty(int fd);
int _win_fflush(FILE *stream);
int _win_read(int fd,void *buf,size_t sz);
size_t _win_printf(const char *fmt,...);
size_t _win_fprintf(FILE *fp,const char *fmt,...);
int _win_write(int fd,const void *buf,int sz);
int _cdecl _fake_main(int argc,char **argv,char **envp);
int PASCAL _win_main_con_entry(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow,int (_cdecl *_main_f)(int argc,char**,char**));

#ifdef __cplusplus
}
#endif

# include <shellapi.h>

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned int (*_win_dropFilesHandler)(HDROP hDrop);

extern HINSTANCE _win_hInstance;

# ifdef WINFCON_STOCK_WIN_MAIN
#  define main _fake_main
int PASCAL WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow) {
	return _win_main_con_entry(hInstance,hPrevInstance,lpCmdLine,nCmdShow,_fake_main);
}
# endif

#ifdef __cplusplus
}
#endif

#endif

#endif //WINVER

