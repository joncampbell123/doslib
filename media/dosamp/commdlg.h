
#if defined(TARGET_WINDOWS)
/* dynamic loading of COMMDLG */
extern HMODULE             commdlg_dll;
extern unsigned char       commdlg_tried;

typedef BOOL (WINAPI *GETOPENFILENAMEPROC)(OPENFILENAME FAR *);

GETOPENFILENAMEPROC commdlg_getopenfilenameproc(void);
void free_commdlg(void);
void commdlg_atexit(void);
int init_commdlg(void);
#endif

