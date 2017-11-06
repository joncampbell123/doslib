
#if defined(LINUX)
/* case sensitive */
int fs_comparechar(char c1,char c2);
# define fs_strncasecmp strncmp
#else
/* case insensitive */
int fs_comparechar(char c1,char c2);
# define fs_strncasecmp strncasecmp
#endif

