
#include <stdio.h>
#include <stdint.h>
#ifdef LINUX
#include <endian.h>
#include <sys/types.h>
#include <sys/stat.h>
#else
#include <hw/cpu/endian.h>
#endif
#ifndef LINUX
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <direct.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <malloc.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#ifndef LINUX
#include <dos.h>
#endif

#ifndef LINUX
#include <hw/dos/dos.h>
#include <hw/cpu/cpu.h>
#include <hw/8237/8237.h>       /* 8237 DMA */
#include <hw/8254/8254.h>       /* 8254 timer */
#include <hw/8259/8259.h>       /* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/cpu/cpurdtsc.h>
#include <hw/dos/doswin.h>
#include <hw/dos/tgusmega.h>
#include <hw/dos/tgussbos.h>
#include <hw/dos/tgusumid.h>
#include <hw/isapnp/isapnp.h>
#include <hw/sndsb/sndsbpnp.h>
#endif

#include "wavefmt.h"
#include "dosamp.h"
#include "timesrc.h"
#include "dosptrnm.h"
#include "filesrc.h"

#ifndef O_BINARY
#define O_BINARY (0)
#endif

static int dosamp_FAR dosamp_file_source_file_fd_close(dosamp_file_source_t const inst) {
    /* ASSUME: inst != NULL */
    if (inst->p.file_fd.fd >= 0) {
        close(inst->p.file_fd.fd);
        inst->p.file_fd.fd = -1;
    }

    return 0;/*success*/
}

static void dosamp_FAR dosamp_file_source_file_fd_free(dosamp_file_source_t const inst) {
    dosamp_file_source_file_fd_close(inst);
    dosamp_file_source_free(inst);
}

static unsigned int dosamp_FAR dosamp_file_source_file_fd_read(dosamp_file_source_t const inst,void dosamp_FAR * buf,unsigned int count) {
    int rd = 0;

    if (inst->p.file_fd.fd < 0 || count > dosamp_file_io_maxb)
        return dosamp_file_io_err;

    if (count > 0) {
#if TARGET_MSDOS == 16
        /* NTS: For 16-bit MS-DOS we must call MS-DOS read() directly instead of using the C runtime because
         *      the read() function in Open Watcom takes only a near pointer in small and compact memory models. */
        rd = _dos_xread(inst->p.file_fd.fd,buf,count);
#else
        rd = read(inst->p.file_fd.fd,buf,count);
#endif
        /* TODO: Open Watcom C always returns -1 on error (I checked the C runtime source to verify this)
         *       The _dos_xread() function in DOSLIB also returns -1 on error (note the SBB BX,BX and OR AX,BX in that code to enforce this when CF=1).
         *       For other OSes like Win32 and Linux we can't assume the same behavior. */
        if (rd == -1) return dosamp_file_io_err; /* also sets errno */
        inst->file_pos += (unsigned int)rd;
    }

    return (unsigned int)rd;
}

static unsigned int dosamp_FAR dosamp_file_source_file_fd_write(dosamp_file_source_t const inst,const void dosamp_FAR * buf,unsigned int count) {
    errno = EIO; /* not implemented */
    return dosamp_file_io_err;
}

static dosamp_file_off_t dosamp_FAR dosamp_file_source_file_fd_seek(dosamp_file_source_t const inst,dosamp_file_off_t pos) {
    off_t r;

    if (inst->p.file_fd.fd < 0 || pos == dosamp_file_io_err)
        return dosamp_file_off_err;

    if (pos > dosamp_file_off_max)
        pos = dosamp_file_off_max;

    r = lseek(inst->p.file_fd.fd,(off_t)pos,SEEK_SET);
    if (r == (off_t)-1L)
        return dosamp_file_off_err;

    return (inst->file_pos = (dosamp_file_off_t)r);
}
 
static const struct dosamp_file_source dosamp_file_source_priv_file_fd_init = {
    .obj_id =                           dosamp_file_source_id_file_fd,
    .file_size =                        -1LL,
    .file_pos =                         0,
    .free =                             dosamp_file_source_file_fd_free,
    .close =                            dosamp_file_source_file_fd_close,
    .read =                             dosamp_file_source_file_fd_read,
    .write =                            dosamp_file_source_file_fd_write,
    .seek =                             dosamp_file_source_file_fd_seek,
    .p.file_fd.fd =                     -1
};

/* FIXME: Possible API problem: Someday when we hand a pointer to this function off to external DLLs
 *        the "path" member will have to be const char dosamp_FAR * in order to accept C-strings from
 *        other data segments in 16-bit memory models, or else nobody except ourselves or anyone with
 *        with the same DGROUP segment will be able to use this function. The problem is that in all
 *        but the large memory models Open Watcom C's open() function takes only const char * (near).
 *        If Open Watcom's C runtime actually does have an _open() or variant that takes a const char FAR *
 *        path string in all 16-bit memory models that would solve this problem.
 *
 *        Then again, I suppose the ABI between us and those future DLLs could simply be defined instead
 *        where WE open the file and hand the DLL the file source object, and it doesn't have to care
 *        about opening, but that would then prevent future media code that may have to read from multiple
 *        files. */
dosamp_file_source_t dosamp_file_source_file_fd_open(const char * const path) {
    dosamp_file_source_t inst;
    struct stat st;

    if (path == NULL) return NULL;
    if (*path == 0) return NULL;

    inst = dosamp_file_source_alloc(&dosamp_file_source_priv_file_fd_init);
    if (inst == NULL) return NULL;

    /* TODO: #ifdef this code out so that it only applies to MS-DOS and Windows.
     *       C runtimes in MS-DOS and Windows cannot identify much about a file
     *       from it's handle but can sufficiently identify from the path. */
    if (stat(path,&st)) goto fail; /* cannot stat: fail */
    if (!S_ISREG(st.st_mode)) goto fail; /* not a file: fail */
    inst->file_size = (dosamp_file_off_t)st.st_size;

    inst->p.file_fd.fd = open(path,O_RDONLY|O_BINARY);
    if (inst->p.file_fd.fd < 0) goto fail;

    /* TODO: For Linux, and other OSes where we can fully identify a file
     *       from it's handle, use fstat() and check S_ISREG() and such HERE */

    if (lseek(inst->p.file_fd.fd,0,SEEK_SET) != 0) goto fail_fd; /* cannot lseek: fail */

    return inst;
fail_fd:
fail:
    inst->close(inst);
    inst->free(inst);
    return NULL;
}

