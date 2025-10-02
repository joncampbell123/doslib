
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>

#include "cc.hpp"

/* default */
const data_type_set_t data_types_default = {
	{ { /*size*/sizeof(char),       /*align*/addrmask_make(alignof(char))        }, TS_CHAR                 }, /* bool */
	{ { /*size*/sizeof(char),       /*align*/addrmask_make(alignof(char))        }, TS_CHAR                 }, /* char */
	{ { /*size*/sizeof(short),      /*align*/addrmask_make(alignof(short))       }, TS_SHORT                }, /* short */
	{ { /*size*/sizeof(int),        /*align*/addrmask_make(alignof(int))         }, TS_INT                  }, /* int */
	{ { /*size*/sizeof(long),       /*align*/addrmask_make(alignof(long))        }, TS_LONG                 }, /* long */
	{ { /*size*/sizeof(long long),  /*align*/addrmask_make(alignof(long long))   }, TS_LONGLONG             }, /* longlong */
	{ { /*size*/sizeof(float),      /*align*/addrmask_make(alignof(float))       }, TS_FLOAT                }, /* float */
	{ { /*size*/sizeof(double),     /*align*/addrmask_make(alignof(double))      }, TS_DOUBLE               }, /* double */
	{ { /*size*/sizeof(long double),/*align*/addrmask_make(alignof(long double)) }, TS_DOUBLE               }  /* longdouble */
};

const data_type_set_ptr_t data_ptr_types_default = {
	{ { /*size*/sizeof(void*),     /*align*/addrmask_make(alignof(void*))     }, TQ_NEAR                 }, /* ptr */
	{ { /*size*/sizeof(void*),     /*align*/addrmask_make(alignof(void*))     }, TQ_NEAR                 }, /* near ptr */
	{ { /*size*/sizeof(void*),     /*align*/addrmask_make(alignof(void*))     }, TQ_NEAR                 }, /* far ptr */
	{ { /*size*/sizeof(void*),     /*align*/addrmask_make(alignof(void*))     }, TQ_NEAR                 }, /* huge ptr */
	{ { /*size*/sizeof(uintptr_t), /*align*/addrmask_make(alignof(uintptr_t)) }, TS_LONG                 }, /* size_t/ssize_t */
	{ { /*size*/sizeof(uintptr_t), /*align*/addrmask_make(alignof(uintptr_t)) }, TS_LONG                 }  /* intptr_t/uintptr_t */
};

/* 16-bit segmented x86 (MS-DOS, Windows, OS/2, etc) */
const data_type_set_t data_types_intel16 = {
	{ { /*size*/1u,                /*align*/addrmask_make(1u)                 }, TS_CHAR                 }, /* bool */
	{ { /*size*/1u,                /*align*/addrmask_make(1u)                 }, TS_CHAR                 }, /* char */
	{ { /*size*/2u,                /*align*/addrmask_make(2u)                 }, TS_SHORT                }, /* short */
	{ { /*size*/2u,                /*align*/addrmask_make(2u)                 }, TS_INT                  }, /* int */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_LONG                 }, /* long */
	{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TS_LONGLONG             }, /* longlong */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_FLOAT                }, /* float */
	{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TS_DOUBLE               }, /* double */
	{ { /*size*/10u,               /*align*/addrmask_make(2u)                 }, TS_LONG|TS_DOUBLE       }  /* longdouble */
};

const data_type_set_ptr_t data_ptr_types_intel16_small = {
	{ { /*size*/2u,                /*align*/addrmask_make(2u)                 }, TQ_NEAR                 }, /* ptr */
	{ { /*size*/2u,                /*align*/addrmask_make(2u)                 }, TQ_NEAR                 }, /* near ptr */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_FAR                  }, /* far ptr */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_HUGE                 }, /* huge ptr */
	{ { /*size*/2u,                /*align*/addrmask_make(2u)                 }, TS_INT                  }, /* size_t/ssize_t */
	{ { /*size*/2u,                /*align*/addrmask_make(2u)                 }, TS_INT                  }  /* intptr_t/uintptr_t */
};

const data_type_set_ptr_t data_ptr_types_intel16_big = {
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_FAR                  }, /* ptr */
	{ { /*size*/2u,                /*align*/addrmask_make(2u)                 }, TQ_NEAR                 }, /* near ptr */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_FAR                  }, /* far ptr */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_HUGE                 }, /* huge ptr */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_LONG                 }, /* size_t/ssize_t */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_LONG                 }  /* intptr_t/uintptr_t */
};

const data_type_set_ptr_t data_ptr_types_intel16_huge = {
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_HUGE                 }, /* ptr */
	{ { /*size*/2u,                /*align*/addrmask_make(2u)                 }, TQ_NEAR                 }, /* near ptr */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_FAR                  }, /* far ptr */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_HUGE                 }, /* huge ptr */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_LONG                 }, /* size_t/ssize_t */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_LONG                 }  /* intptr_t/uintptr_t */
};

/* Open Watcom definitions:
 *   small = small code + small data
 *   medium = big code + small data
 *   compact = small code + big data
 *   large = big code + big data
 *   huge = big code + hude data
 *
 * "Huge" pointers in Open Watcom are like 32-bit pointers in a flat memory space,
 * which are converted on the fly to 16:16 segmented real mode addresses. The code
 * runs slower, but this also allows converting existing code not written for the
 * 16-bit segmented model to run anyway. */

/* 32-bit segmented or flat x86 (MS-DOS, Windows, OS/2, etc) */
const data_type_set_t data_types_intel32 = {
	{ { /*size*/1u,                /*align*/addrmask_make(1u)                 }, TS_CHAR                 }, /* bool */
	{ { /*size*/1u,                /*align*/addrmask_make(1u)                 }, TS_CHAR                 }, /* char */
	{ { /*size*/2u,                /*align*/addrmask_make(2u)                 }, TS_SHORT                }, /* short */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_INT                  }, /* int */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_LONG                 }, /* long */
	{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TS_LONGLONG             }, /* longlong */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_FLOAT                }, /* float */
	{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TS_DOUBLE               }, /* double */
	{ { /*size*/10u,               /*align*/addrmask_make(4u)                 }, TS_LONG|TS_DOUBLE       }  /* longdouble */
};

/* 32-bit segmented (though perfectly usable with flat memory models too) */
const data_type_set_ptr_t data_ptr_types_intel32_segmented = {
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_NEAR                 }, /* ptr */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_NEAR                 }, /* near ptr */
	{ { /*size*/6u,                /*align*/addrmask_make(8u)                 }, TQ_FAR                  }, /* far ptr */
	{ { /*size*/6u,                /*align*/addrmask_make(8u)                 }, TQ_HUGE                 }, /* huge ptr */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_LONG                 }, /* size_t/ssize_t */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_LONG                 }  /* intptr_t/uintptr_t */
};

/* 32-bit flat memory models such as Linux i386 where you do not need far pointers, EVER */
const data_type_set_ptr_t data_ptr_types_intel32_flat = {
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_NEAR                 }, /* ptr */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_NEAR                 }, /* near ptr */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_NEAR                 }, /* far ptr */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TQ_NEAR                 }, /* huge ptr */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_LONG                 }, /* size_t/ssize_t */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_LONG                 }  /* intptr_t/uintptr_t */
};

/* 64-bit flat x86_64 (Linux, etc) */
const data_type_set_t data_types_intel64 = {
	{ { /*size*/1u,                /*align*/addrmask_make(1u)                 }, TS_CHAR                 }, /* bool */
	{ { /*size*/1u,                /*align*/addrmask_make(1u)                 }, TS_CHAR                 }, /* char */
	{ { /*size*/2u,                /*align*/addrmask_make(2u)                 }, TS_SHORT                }, /* short */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_INT                  }, /* int */
	{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TS_LONG                 }, /* long */
	{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TS_LONGLONG             }, /* longlong */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_FLOAT                }, /* float */
	{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TS_DOUBLE               }, /* double */
	{ { /*size*/10u,               /*align*/addrmask_make(8u)                 }, TS_LONG|TS_DOUBLE       }  /* longdouble */
};

/* 64-bit flat x86_64 (Windows) */
const data_type_set_t data_types_intel64_windows = {
	{ { /*size*/1u,                /*align*/addrmask_make(1u)                 }, TS_CHAR                 }, /* bool */
	{ { /*size*/1u,                /*align*/addrmask_make(1u)                 }, TS_CHAR                 }, /* char */
	{ { /*size*/2u,                /*align*/addrmask_make(2u)                 }, TS_SHORT                }, /* short */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_INT                  }, /* int */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_LONG                 }, /* long */
	{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TS_LONGLONG             }, /* longlong */
	{ { /*size*/4u,                /*align*/addrmask_make(4u)                 }, TS_FLOAT                }, /* float */
	{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TS_DOUBLE               }, /* double */
	{ { /*size*/10u,               /*align*/addrmask_make(8u)                 }, TS_LONG|TS_DOUBLE       }  /* longdouble */
};

/* 64-bit flat memory model */
const data_type_set_ptr_t data_ptr_types_intel64_flat = {
	{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TQ_NEAR                 }, /* ptr */
	{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TQ_NEAR                 }, /* near ptr */
	{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TQ_NEAR                 }, /* far ptr */
	{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TQ_NEAR                 }, /* huge ptr */
	{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TS_LONGLONG             }, /* size_t/ssize_t */
	{ { /*size*/8u,                /*align*/addrmask_make(8u)                 }, TS_LONGLONG             }  /* intptr_t/uintptr_t */
};

