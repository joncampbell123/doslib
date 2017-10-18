
enum {
    dosamp_file_source_id_null = 0,
    dosamp_file_source_id_file_fd = 1
};

#if TARGET_MSDOS == 32
# define dosamp_file_io_maxb            ((unsigned int) (INT_MAX - 1U))
# define dosamp_file_io_err             ((unsigned int) (UINT_MAX))
#else
# define dosamp_file_io_maxb            ((unsigned int) (UINT_MAX - 1U))
# define dosamp_file_io_err             ((unsigned int) (UINT_MAX))
#endif

#if TARGET_MSDOS == 32
# if defined(TARGET_WINDOWS) && !defined(WIN386)
/* 32-bit Windows and later can support files >= 4GB */
typedef uint64_t                        dosamp_file_off_t;
#  define dosamp_file_off_max           ((dosamp_file_off_t) (ULLONG_MAX - 1ULL))
#  define dosamp_file_off_err           ((dosamp_file_off_t) (ULLONG_MAX))
# else
/* 32-bit MS-DOS is limited to 2GB or less (4GB if FAT32) */
typedef uint32_t                        dosamp_file_off_t;
#  define dosamp_file_off_max           ((dosamp_file_off_t) (ULONG_MAX - 1UL))
#  define dosamp_file_off_err           ((dosamp_file_off_t) (ULONG_MAX))
# endif
#else
/* 16-bit MS-DOS and Windows is limited to 2GB or less (4GB if FAT32) */
typedef uint32_t                        dosamp_file_off_t;
# define dosamp_file_off_max            ((dosamp_file_off_t) (ULONG_MAX - 1UL))
# define dosamp_file_off_err            ((dosamp_file_off_t) (ULONG_MAX))
#endif

/* obj_id == dosamp_file_source_id_file_fd.
 * must be sizeof() <= sizeof(private) */
typedef struct dosamp_file_source_priv_file_fd {
    int                                 fd;
};

typedef struct dosamp_file_source;
typedef struct dosamp_file_source dosamp_FAR * dosamp_file_source_t;
typedef struct dosamp_file_source dosamp_FAR * dosamp_FAR * dosamp_file_source_ptr_t;
typedef const struct dosamp_file_source dosamp_FAR * const_dosamp_file_source_t;

typedef struct dosamp_file_source {
    unsigned int                        obj_id;     /* what exactly this is */
    volatile unsigned int               refcount;   /* reference count. will NOT auto-free when zero. */
    int                                 open_flags; /* O_RDONLY, O_WRONLY, O_RDWR, etc. used to open file or 0 if closed */
    int64_t                             file_size;  /* file size in bytes (if known) or -1LL */
    int64_t                             file_pos;   /* file pointer or -1LL if not known */
    void                                (dosamp_FAR * free)(dosamp_file_source_t const inst); /* free the file source */
    int                                 (dosamp_FAR * close)(dosamp_file_source_t const inst); /* call this to close the file */
    unsigned int                        (dosamp_FAR * read)(dosamp_file_source_t const inst,void dosamp_FAR *buf,unsigned int count); /* read function */
    unsigned int                        (dosamp_FAR * write)(dosamp_file_source_t const inst,const void dosamp_FAR *buf,unsigned int count); /* write function */
    dosamp_file_off_t                   (dosamp_FAR * seek)(dosamp_file_source_t const inst,dosamp_file_off_t pos); /* seek function */
    union {
        struct dosamp_file_source_priv_file_fd      file_fd;
    } p;
};

unsigned int dosamp_FAR dosamp_file_source_addref(dosamp_file_source_t const inst);
unsigned int dosamp_FAR dosamp_file_source_release(dosamp_file_source_t const inst);
unsigned int dosamp_FAR dosamp_file_source_autofree(dosamp_file_source_ptr_t inst);
dosamp_file_source_t dosamp_FAR dosamp_file_source_alloc(const_dosamp_file_source_t const inst_template);
void dosamp_FAR dosamp_file_source_free(dosamp_file_source_t const inst);

