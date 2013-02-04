/* winfcon.h
 *
 * Fake console for Windows applications where a console is not available.
 * (C) 2011-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

/* Windows 3.1 and earlier, even for Win32s: there is no system given console.
 * We either have to draw and maintain our own window, or find some other way to printf() and display it. */
#if defined(TARGET_WINDOWS) && (TARGET_MSDOS == 16 || (TARGET_MSDOS == 32 && TARGET_WINDOWS == 31) || (defined(WIN386) && TARGET_MSDOS == 32))
# define WIN_STDOUT_CONSOLE
# define getch _win_getch
# define kbhit _win_kbhit
# define fprintf __XXX_TODO_fprintf
# define printf _win_printf
# define isatty _win_isatty
# define write _win_write
# define read _win_read

void _win_pump();
int _win_kbhit();
int _win_getch();
HWND _win_hwnd();
void _gdi_pause();
void _win_pump_wait();
void _win_putc(char c);
int _win_isatty(int fd);
int _win_read(int fd,void *buf,int sz);
size_t _win_printf(const char *fmt,...);
int _win_write(int fd,const void *buf,int sz);
int _cdecl main(int argc,char **argv,char **envp);
int PASCAL _win_main_con_entry(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow,int (_cdecl *_main_f)(int argc,char**,char**));

extern HINSTANCE _win_hInstance;

# ifdef WINFCON_STOCK_WIN_MAIN
int PASCAL WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow) {
	return _win_main_con_entry(hInstance,hPrevInstance,lpCmdLine,nCmdShow,main);
}
# endif

#endif

