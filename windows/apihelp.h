#ifdef TARGET_WINDOWS

/* advice: whether the target Windows version needs MakeProcInstance */
/* 32-bit windows: No (except Win386 Windows 3.0)
 * 16-bit windows: No (except Windows 3.0) */
# if TARGET_MSDOS == 16
#  if !defined(TARGET_OS2) && TARGET_WINDOWS <= 30
#   define WIN16_NEEDS_MAKEPROCINSTANCE 1
#  endif
# endif

# if TARGET_MSDOS == 16 || (TARGET_MSDOS == 32 && defined(WIN386))
/* Win16 */
#  define WindowProcType		LRESULT PASCAL FAR
#  define WindowProcType_NoLoadDS	LRESULT PASCAL FAR
# elif defined(WIN386)
#  define WindowProcType		LRESULT PASCAL
#  define WindowProcType_NoLoadDS	LRESULT PASCAL
# else
/* Win32s/Win32/WinNT */
#  define WindowProcType		LRESULT WINAPI
#  define WindowProcType_NoLoadDS	LRESULT WINAPI
# endif

# if TARGET_MSDOS == 16 || (TARGET_MSDOS == 32 && defined(WIN386))
/* Win16 */
#  define DialogProcType		BOOL PASCAL FAR
#  define DialogProcType_NoLoadDS	BOOL PASCAL FAR
# elif defined(WIN386)
#  define DialogProcType		BOOL PASCAL
#  define DialogProcType_NoLoadDS	BOOL PASCAL
# else
/* Win32s/Win32/WinNT */
#  define DialogProcType		BOOL WINAPI
#  define DialogProcType_NoLoadDS	BOOL WINAPI
# endif

# if TARGET_MSDOS == 16 || (TARGET_MSDOS == 32 && defined(WIN386))
/* Win16 */
#  define DllEntryType			PASCAL FAR
#  define DllEntryType_NoLoadDS		PASCAL FAR
# elif defined(WIN386)
#  define DllEntryType			PASCAL
#  define DllEntryType_NoLoadDS		PASCAL
# else
/* Win32s/Win32/WinNT */
#  define DllEntryType			__stdcall
#  define DllEntryType_NoLoadDS		__stdcall
# endif

#endif /* TARGET_WINDOWS */

