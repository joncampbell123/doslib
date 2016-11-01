
#include <dos.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <hw/dos/exeload.h>

#define CLSG_EXPORT_PROC __cdecl far

struct exeload_ctx      final_exe = exeload_ctx_INIT;

int exeload_clsg_validate_header(const struct exeload_ctx * const exe) {
    if (exe == NULL) return 0;

    {
        /* CLSG at the beginning of the resident image and a valid function call table */
        unsigned char far *p = MK_FP(exe->base_seg,0);
        unsigned short far *functbl = (unsigned short far*)(p+4UL+2UL);
        unsigned long exe_len = exeload_resident_length(exe);
        unsigned short numfunc;
        unsigned int i;

        if (*((unsigned long far*)(p+0)) != 0x47534C43UL) return 0; // seg:0 = CLSG

        numfunc = *((unsigned short far*)(p+4UL));
        if (((numfunc*2UL)+4UL+2UL) > exe_len) return 0;

        /* none of the functions can point outside the EXE resident image. */
        for (i=0;i < numfunc;i++) {
            unsigned short fo = functbl[i];
            if ((unsigned long)fo >= exe_len) return 0;
        }

        /* the last entry after function table must be 0xFFFF */
        if (functbl[numfunc] != 0xFFFFU) return 0;
    }

    return 1;
}

static inline unsigned short exeload_clsg_function_count(const struct exeload_ctx * const exe) {
    return *((unsigned short*)MK_FP(exe->base_seg,4));
}

static inline unsigned short exeload_clsg_function_offset(const struct exeload_ctx * const exe,const unsigned int i) {
    return *((unsigned short*)MK_FP(exe->base_seg,4+2+(i*2U)));
}

/* NTS: You're supposed to typecast return value into function pointer prototype */
static inline void far *exeload_clsg_function_ptr(const struct exeload_ctx * const exe,const unsigned int i) {
    return (void far*)MK_FP(exe->base_seg,exeload_clsg_function_offset(exe,i));
}

int main() {
    if (!exeload_load(&final_exe,"final.exe")) {
        fprintf(stderr,"Load failed\n");
        return 1;
    }
    if (!exeload_clsg_validate_header(&final_exe)) {
        fprintf(stderr,"EXE is not valid CLSG\n");
        exeload_free(&final_exe);
        return 1;
    }

    fprintf(stderr,"EXE image loaded to %04x:0000 residentlen=%lu\n",final_exe.base_seg,(unsigned long)final_exe.len_seg << 4UL);
    {
        unsigned int i,m=exeload_clsg_function_count(&final_exe);

        fprintf(stderr,"%u functions:\n",m);
        for (i=0;i < m;i++)
            fprintf(stderr,"  [%u]: %04x (%Fp)\n",i,exeload_clsg_function_offset(&final_exe,i),exeload_clsg_function_ptr(&final_exe,i));
    }

    /* let's call some! */
    {
        /* index 0:
           const char far * CLSG_EXPORT_PROC get_message(void); */
        const char far * (CLSG_EXPORT_PROC *get_message)(void) = exeload_clsg_function_ptr(&final_exe,0);
        const char far *msg;

        fprintf(stderr,"Calling entry 0 (get_message) now.\n");
        msg = get_message();
        fprintf(stderr,"Result: %Fp = %Fs\n",msg,msg);
    }

    {
        /* index 1:
           unsigned int CLSG_EXPORT_PROC callmemaybe(void); */
        unsigned int (CLSG_EXPORT_PROC *callmemaybe)(void) = exeload_clsg_function_ptr(&final_exe,1);
        unsigned int val;

        fprintf(stderr,"Calling entry 1 (callmemaybe) now.\n");
        val = callmemaybe();
        fprintf(stderr,"Result: %04x\n",val);
    }

    exeload_free(&final_exe);
    return 0;
}

