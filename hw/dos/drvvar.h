
#ifdef NEAR_DRVVAR
# define DOSDRV_NF near
#else
# define DOSDRV_NF far
#endif

extern unsigned char _cdecl DOSDRV_NF                               dosdrv_end;
extern unsigned char _cdecl DOSDRV_NF                               dosdrv_initbegin;
extern struct dosdrv_header_t _cdecl DOSDRV_NF                      dosdrv_header;
extern struct dosdrv_request_base_t far _cdecl * DOSDRV_NF          dosdrv_req_ptr;

/* NTS: __saveregs in Watcom C has no effect unless calling convention is watcall */
#define DOSDEVICE_INTERRUPT_PROC void __watcall __loadds __saveregs far

/* if using far stub */
#define DOSDEVICE_INTERRUPT_FAR_PROC void __watcall __saveregs far

/* if using near stub */
#define DOSDEVICE_INTERRUPT_NEAR_PROC void __watcall __saveregs near

#undef DOSDRV_NF

