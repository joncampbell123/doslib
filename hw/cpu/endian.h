
/* Watcom C does not have endian.h */
#ifdef TARGET_MSDOS

/* MS-DOS and Windows are little endian x86 */
#define htole16(x) (x)
#define htole32(x) (x)
#define htole64(x) (x)

#define le16toh(x) (x)
#define le32toh(x) (x)
#define le64toh(x) (x)

#endif

