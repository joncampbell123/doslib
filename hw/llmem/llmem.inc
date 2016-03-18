
; extern definitions for *.c files in this library.

; ---------- DATA segment -----------------
%include "_segdata.inc"

%if TARGET_MSDOS == 16
extern _llmemcpy_gdt ; uint32_t far* near	llmemcpy_gdt = NULL;
extern _llmemcpy_gdtr ; uint16_t near		llmemcpy_gdtr[4];
extern _llmemcpy_idtr ; uint16_t near		llmemcpy_idtr[4];
extern _llmemcpy_vcpi ; uint32_t near		llmemcpy_vcpi[0x20];
extern _llmemcpy_vcpi_return ; uint32_t near		llmemcpy_vcpi_return[2];
extern _llmemcpy_pagetables ; volatile void FAR*	llmemcpy_pagetables = NULL;
%endif


