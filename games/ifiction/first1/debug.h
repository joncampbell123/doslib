
void IFEDBG(const char *msg,...);

#if defined(USE_DOSLIB) || defined(USE_WIN32)
extern bool			dosbox_ig; /* DOSBox Integration Device detected */
extern bool			want_dosbox_ig;

extern bool			bochs_e9; /* Bochs port E9h hack (prints to console in Bochs) */
extern bool			want_bochs_e9;
#endif

extern bool			ifedbg_en;
extern bool			ifedbg_auto;
extern char			fatal_tmp[256];

extern bool			debug_file;
extern bool			want_debug_file;
extern int			debug_fd;

#if defined(USE_DOSLIB)
void IFE_win95_tf_hang_check(void);
#endif

