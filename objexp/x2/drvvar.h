
extern unsigned char _cdecl far                         dosdrv_end;
extern unsigned char _cdecl far                         dosdrv_initbegin;
extern struct dosdrv_header_t _cdecl far                dosdrv_header;
extern struct dosdrv_request_base_t far _cdecl * far    dosdrv_req_ptr;

/* NTS: __saveregs in Watcom C has no effect unless calling convention is watcall */
#define DOSDEVICE_INTERRUPT_PROC void __watcall __loadds __saveregs far

