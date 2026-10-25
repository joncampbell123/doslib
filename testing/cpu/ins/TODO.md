# 386.EXE TODO

386.EXE passes 45 groups (0 fail) on DOSBox-X dosbox-x-ai with core=normal,
dynamic and full, on IBM PC (svga_s3) and PC-98. These are the areas it does
not test yet.

## Real hardware

- Run on a real 386 (and a 486 for comparison) and check the INFO lines and
  the "Limit 4GB" group: the observed behavior is that a 386 checks every limit
  except FFFFFFFFh, for both 16-bit and 32-bit segments.
- Run under EMM386 (V86 mode) to check that the protected mode and V86 groups
  are skipped cleanly.

## Segment protection

- Null selector: an access through DS/ES/FS/GS = 0 in protected mode raises
  #GP(0) (286P.EXE has a 16-bit version of this; add a 32-bit one).
- Expand-down data segment with B=0: the upper bound is FFFFh, so an access at
  offset 10000h and up faults (only B=1 is tested now).
- Segment type checks: a write through a read-only data segment or a code
  segment, a read through an execute-only code segment, loading SS with a
  read-only segment, loading DS with an execute-only segment.
- Accessed bit: loading a segment register sets the accessed bit in the GDT
  and LDT descriptor.
- Stack limit in 32-bit protected mode: PUSH/POP past the SS limit, and the
  expand-down stack, raise #SS(0) (286P.EXE tests the 16-bit case).
- String instructions (MOVS/STOS/LODS/CMPS/SCAS/INS/OUTS) and XLAT past a
  segment limit, with 16-bit and 32-bit addressing.
- LSS/LDS/LES/LFS/LGS where the operand is addressed through the segment
  register being loaded, for example `lss esp,[esp]` with a new SS base. The
  offset must come from the old segment. This found a DOSBox-X bug that broke
  CauseWay.
- A fault in the middle of a push during exception delivery: a stack fault
  while pushing the exception frame gives #SS, or #DF where the SDM says so.

## Instructions

- Instructions with 32-bit operands in V86 mode (only a basic set is run in
  V86 now).
- ENTER with a nesting level above 0, with 16-bit and 32-bit operand sizes.
- BCD instructions (DAA/DAS/AAA/AAS/AAM/AAD) with the flags that a 386
  defines, compared with 8086.EXE results.
- Undefined flags of BSF/BSR, SHLD/SHRD with count above 16, MUL/IMUL, which
  are masked now: report them as INFO so that real 386 and 486 values can be
  recorded.

## 486 and later (when the CPU is a 486)

- CMPXCHG, XADD, INVD/WBINVD, INVLPG.
- EFLAGS.AC with CR0.AM: alignment check #AC(0) at ring 3.
- CR0.WP: a ring 0 write to a read-only user page faults when WP=1.
- CPUID when EFLAGS.ID can be toggled.
