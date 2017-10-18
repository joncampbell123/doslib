
/* making the API use FAR CALL function pointers will allow future development
 * where file access and media playback code can exist in external DLLs, whether
 * 16-bit or 32-bit code */
#if TARGET_MSDOS == 32 && !defined(WIN386)
# define dosamp_FAR
#else
# define dosamp_FAR far
#endif

