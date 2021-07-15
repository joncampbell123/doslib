
void IFEDBG(const char *msg,...);

#if defined(USE_DOSLIB)
extern bool			dosbox_ig; /* DOSBox Integration Device detected */
#endif

extern bool			ifedbg_en;
extern char			fatal_tmp[256];

#if defined(USE_DOSLIB)
void IFE_win95_tf_hang_check(void);
#endif

