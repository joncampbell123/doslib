
enum {
    dosamp_time_source_id_null = 0,                 /* null */
    dosamp_time_source_id_8254 = 1,                 /* MS-DOS / Windows 3.x 8254 PIT timer */
    dosamp_time_source_id_rdtsc = 2,                /* Pentium RDTSC */
    dosamp_time_source_id_clock_monotonic = 3       /* CLOCK_MONOTONIC (Linux) */
};

/* forward def, to make typedef, before actual struct */
struct dosamp_time_source;
typedef struct dosamp_time_source dosamp_FAR * dosamp_time_source_t;

/* Time source.
 * NTS: For maximum portability, the caller should not expect accurate timekeeping UNLESS polling
 *      the time source on a semi-regular basis as indicated in the structure. The 8254 PIT time
 *      source especially needs this since the code does NOT hook the IRQ 0 interrupt.
 *
 *      Unlike file sources, these are statically declared and are NOT malloc'd or free'd.
 *      But you must open/close them regardless. */
struct dosamp_time_source {
    unsigned int                        obj_id;     /* what exactly this is */
    volatile unsigned int               refcount;   /* reference count. will NOT auto-free when zero. */
    unsigned int                        open_flags; /* zero if not open, nonzero if open */
    unsigned long                       clock_rate; /* tick rate of the time source */
    unsigned long long                  counter;    /* counter */
    unsigned long                       poll_requirement; /* you must poll the source this often (clock ticks) to keep time (Intel 8254 source) */
    int                                 (dosamp_FAR *close)(dosamp_time_source_t inst);
    int                                 (dosamp_FAR *open)(dosamp_time_source_t inst);
    unsigned long long                  (dosamp_FAR *poll)(dosamp_time_source_t inst);
};

#if defined(HAS_8254)
/* 8254 time source */
extern struct dosamp_time_source                dosamp_time_source_8254;
#endif

#if defined(HAS_RDTSC)
/* Pentium RDTSC time source (and availability check) */
extern struct dosamp_time_source                dosamp_time_source_rdtsc;
int dosamp_time_source_rdtsc_available(const dosamp_time_source_t clk);
#endif

#if defined(HAS_CLOCK_MONOTONIC)
extern struct dosamp_time_source                dosamp_time_source_clock_monotonic;
#endif

