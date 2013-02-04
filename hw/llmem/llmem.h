
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>

enum {
	LLMEMCPY_PF_NONE=0,
	LLMEMCPY_PF_386_4KB,		/* 386-compatible two-level 4KB pages */
	LLMEMCPY_PF_PSE_4M,		/* Pentium PSE style one-level 4M pages */
	LLMEMCPY_PF_PAE_2M,		/* Modern PAE 64-bit pages 2M each */
	LLMEMCPY_PF_PSE_4M_VCPI,	/* PSE 4M pages, with one 4KB page for VCPI server compatibility */
	LLMEMCPY_PF_PAE_2M_VCPI		/* PAE 2M pages, with two 4KB pages for VCPI compat, and one extra page */
};

extern unsigned char		llmem_probed;
extern unsigned char		llmem_meth_pse;
extern unsigned char		llmem_meth_pae;
extern unsigned char		llmem_available;
extern uint64_t			llmem_phys_limit;
extern uint64_t			llmem_pse_limit;
extern const char*		llmem_reason;

extern volatile void FAR*	llmemcpy_pagetables;
extern volatile void FAR*	llmemcpy_pagetables_raw;
extern size_t			llmemcpy_pagetables_size;
extern unsigned char		llmemcpy_pagefmt;
#if TARGET_MSDOS == 16
extern uint32_t far*		llmemcpy_gdt;
extern uint16_t			llmemcpy_gdtr[4];
extern uint16_t			llmemcpy_idtr[4];
extern uint32_t			llmemcpy_vcpi[0x20];
extern uint32_t			llmemcpy_vcpi_return[2];
extern uint8_t			llmemcpy_vcpi_tss[108];
#endif

/* returns 0xFFFFFFFFFFFFFFFF if unmappable */
#if TARGET_MSDOS == 16
uint64_t llmem_ptr2ofs(unsigned char far *ptr);
#else
uint64_t llmem_ptr2ofs(unsigned char *ptr);
#endif

int llmem_init();
void llmemcpy_free();
int llmemcpy_alloc(size_t len,unsigned char typ);
uint32_t llmem_virt_phys_recommended_copy_size();
size_t llmemcpy(uint64_t dst,uint64_t src,size_t len);

