# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

# The build system we have has no effect on NASM (at the moment) and
# all memory models+CPU modes build the same binary. So why build them
# again and again? Build them only in dos86s mode.
!ifndef TARGET_WINDOWS
! ifndef TARGET_OS2
!  ifeq TARGET_MSDOS 16
!   ifeq TARGET86 86
!    ifeq MMODE s
BUILD_NASM_COM = 1
!    endif
!   endif
!  endif
! endif
!endif

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."
AFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."
NASMFLAGS_THIS = 
NOW_BUILDING = HW_CPU_LIB

# NTS: CPU functions here are to be moved at some point to the cpu library!
OBJS =        $(SUBDIR)$(HPS)cpuasm.obj $(SUBDIR)$(HPS)cpup3sn.obj $(SUBDIR)$(HPS)cpup3snc.obj $(SUBDIR)$(HPS)cpusse.obj $(SUBDIR)$(HPS)cpu.obj $(SUBDIR)$(HPS)cpuidext.obj $(SUBDIR)$(HPS)gdt_enum.obj $(SUBDIR)$(HPS)libgrind.obj $(SUBDIR)$(HPS)cpurdtsc.obj $(SUBDIR)$(HPS)cpuiopd.obj $(SUBDIR)$(HPS)cpustrlv.obj $(SUBDIR)$(HPS)resetasm.obj $(SUBDIR)$(HPS)cpuid.obj $(SUBDIR)$(HPS)cpurdmsr.obj $(SUBDIR)$(HPS)cpussea.obj $(SUBDIR)$(HPS)cpuwinf.obj $(SUBDIR)$(HPS)cpumycs.obj

# test programs (MS-DOS and Windows)
MMX_EXE =     $(SUBDIR)$(HPS)mmx.$(EXEEXT)
SSE_EXE =     $(SUBDIR)$(HPS)sse.$(EXEEXT)
TEST_EXE =    $(SUBDIR)$(HPS)test.$(EXEEXT)
GRIND_EXE =   $(SUBDIR)$(HPS)grind.$(EXEEXT)
GR_ADD_EXE =  $(SUBDIR)$(HPS)gr_add.$(EXEEXT)
DISPSN_EXE =  $(SUBDIR)$(HPS)dispsn.$(EXEEXT)
SSEOFF_EXE =  $(SUBDIR)$(HPS)sseoff.$(EXEEXT)
GDTLIST_EXE = $(SUBDIR)$(HPS)gdtlist.$(EXEEXT)
!ifndef WIN386
GDTTAE_EXE =  $(SUBDIR)$(HPS)gdttae.$(EXEEXT)
!endif
RDTSC_EXE =   $(SUBDIR)$(HPS)rdtsc.$(EXEEXT)

!ifndef TARGET_WINDOWS
! ifndef TARGET_OS2
# test programs (MS-DOS only)
RESET_EXE =   $(SUBDIR)$(HPS)reset.$(EXEEXT)
APIC_EXE =    $(SUBDIR)$(HPS)apic.$(EXEEXT)
INP_EXE =	  $(SUBDIR)$(HPS)inp.$(EXEEXT)
OUTP_EXE =	  $(SUBDIR)$(HPS)outp.$(EXEEXT)
! endif
!endif

!ifdef BUILD_NASM_COM
# MS-DOS COM output
V86_COM =     $(SUBDIR)$(HPS)v86.com
TSS_COM =     $(SUBDIR)$(HPS)tss.com
PROT286_COM = $(SUBDIR)$(HPS)prot286.com
PROT386_COM = $(SUBDIR)$(HPS)prot386.com
TSSRING_COM = $(SUBDIR)$(HPS)tssring.com
V86KERN_COM = $(SUBDIR)$(HPS)v86kern.com
PROTVCPI_COM =$(SUBDIR)$(HPS)protvcpi.com
PROTDPMI_COM =$(SUBDIR)$(HPS)protdpmi.com
V86KERN2_COM =$(SUBDIR)$(HPS)v86kern2.com
ALIGNCHK_COM =$(SUBDIR)$(HPS)alignchk.com
!endif

!ifdef APIC_EXE
OBJS +=       $(SUBDIR)$(HPS)apiclib.obj
!endif

$(HW_CPU_LIB): $(OBJS)
	wlib -q -b -c $(HW_CPU_LIB) -+$(SUBDIR)$(HPS)cpu.obj -+$(SUBDIR)$(HPS)cpuasm.obj
	wlib -q -b -c $(HW_CPU_LIB) -+$(SUBDIR)$(HPS)gdt_enum.obj -+$(SUBDIR)$(HPS)libgrind.obj
	wlib -q -b -c $(HW_CPU_LIB) -+$(SUBDIR)$(HPS)cpuidext.obj -+$(SUBDIR)$(HPS)cpusse.obj
	wlib -q -b -c $(HW_CPU_LIB) -+$(SUBDIR)$(HPS)cpup3sn.obj -+$(SUBDIR)$(HPS)cpup3snc.obj
	wlib -q -b -c $(HW_CPU_LIB) -+$(SUBDIR)$(HPS)cpurdtsc.obj -+$(SUBDIR)$(HPS)cpuiopd.obj
	wlib -q -b -c $(HW_CPU_LIB) -+$(SUBDIR)$(HPS)cpustrlv.obj -+$(SUBDIR)$(HPS)resetasm.obj
	wlib -q -b -c $(HW_CPU_LIB) -+$(SUBDIR)$(HPS)cpuid.obj -+$(SUBDIR)$(HPS)cpurdmsr.obj
	wlib -q -b -c $(HW_CPU_LIB) -+$(SUBDIR)$(HPS)cpussea.obj -+$(SUBDIR)$(HPS)cpuwinf.obj
	wlib -q -b -c $(HW_CPU_LIB) -+$(SUBDIR)$(HPS)cpumycs.obj
!ifdef APIC_EXE
	wlib -q -b -c $(HW_CPU_LIB) -+$(SUBDIR)$(HPS)apiclib.obj
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

.asm.obj:
	nasm -o $@ -f obj $(NASMFLAGS_CON) $[@
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe
	
lib: $(HW_CPU_LIB) .symbolic
	
exe: $(GDTTAE_EXE) $(TEST_EXE) $(GRIND_EXE) $(GR_ADD_EXE) $(DISPSN_EXE) $(RESET_EXE) $(APIC_EXE) $(MMX_EXE) $(SSE_EXE) $(SSEOFF_EXE) $(PROT286_COM) $(PROT386_COM) $(TSS_COM) $(TSSRING_COM) $(ALIGNCHK_COM) $(V86_COM) $(V86KERN_COM) $(V86KERN2_COM) $(PROTVCPI_COM) $(PROTDPMI_COM) $(RDTSC_EXE) $(GDTLIST_EXE) $(INP_EXE) $(OUTP_EXE) .symbolic

!ifdef BUILD_NASM_COM
$(V86_COM): v86.asm
	nasm -o $@ -f bin $(NASMFLAGS) $[@

$(TSS_COM): tss.asm
	nasm -o $@ -f bin $(NASMFLAGS) $[@

$(TSSRING_COM): tssring.asm
	nasm -o $@ -f bin $(NASMFLAGS) $[@

$(PROT286_COM): prot286.asm
	nasm -o $@ -f bin $(NASMFLAGS) $[@

$(PROT386_COM): prot386.asm
	nasm -o $@ -f bin $(NASMFLAGS) $[@

$(V86KERN_COM): v86kern.asm
	nasm -o $@ -f bin $(NASMFLAGS) -l $(SUBDIR)$(HPS)v86kern.lst -O9 $[@

$(PROTVCPI_COM): protvcpi.asm
	nasm -o $@ -f bin $(NASMFLAGS) -l $(SUBDIR)$(HPS)protvcpi.lst -O9 $[@

$(PROTDPMI_COM): protdpmi.asm
	nasm -o $@ -f bin $(NASMFLAGS) -l $(SUBDIR)$(HPS)protdpmi.lst -O9 $[@

$(V86KERN2_COM): v86kern2.asm
	nasm -o $@ -f bin $(NASMFLAGS) -l $(SUBDIR)$(HPS)v86kern2.lst -O9 $[@

$(ALIGNCHK_COM): alignchk.asm
	nasm -o $@ -f bin $(NASMFLAGS) $[@
!endif

$(MMX_EXE): $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)mmx.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) $(HW_CPU_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)mmx.obj option map=$(SUBDIR)$(HPS)mmx.map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
	# protected mode only. real-mode Windows is a pain.
	%append tmp.cmd option protmode
!  endif
! endif
	%write tmp.cmd name $(MMX_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(MMX_EXE)
	@wbind $(MMX_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(MMX_EXE)
! endif

$(SSE_EXE): $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)sse.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) $(HW_CPU_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)sse.obj option map=$(SUBDIR)$(HPS)sse.map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
	# protected mode only. real-mode Windows is a pain.
	%append tmp.cmd option protmode
!  endif
! endif
	%write tmp.cmd name $(SSE_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(SSE_EXE)
	@wbind $(SSE_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(SSE_EXE)
! endif

$(TEST_EXE): $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) $(HW_CPU_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)test.obj option map=$(SUBDIR)$(HPS)test.map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
	# protected mode only. real-mode Windows is a pain.
	%append tmp.cmd option protmode
!  endif
! endif
	%write tmp.cmd name $(TEST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(TEST_EXE)
	@wbind $(TEST_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(TEST_EXE)
! endif

$(GRIND_EXE): $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)grind.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) $(HW_CPU_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)grind.obj option map=$(SUBDIR)$(HPS)grind.map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
	# protected mode only. real-mode Windows is a pain.
	%append tmp.cmd option protmode
!  endif
! endif
	%write tmp.cmd name $(GRIND_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(GRIND_EXE)
	@wbind $(GRIND_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(GRIND_EXE)
! endif

$(GR_ADD_EXE): $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)gr_add.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) $(HW_CPU_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)gr_add.obj option map=$(SUBDIR)$(HPS)gr_add.map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
	# protected mode only. real-mode Windows is a pain.
	%append tmp.cmd option protmode
!  endif
! endif
	%write tmp.cmd name $(GR_ADD_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(GR_ADD_EXE)
	@wbind $(GR_ADD_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(GR_ADD_EXE)
! endif

$(RDTSC_EXE): $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_8254_LIB) $(SUBDIR)$(HPS)rdtsc.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) $(HW_CPU_LIB_WLINK_LIBRARIES) library $(HW_8254_LIB) file $(SUBDIR)$(HPS)rdtsc.obj option map=$(SUBDIR)$(HPS)rdtsc.map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
	# protected mode only. real-mode Windows is a pain.
	%append tmp.cmd option protmode
!  endif
! endif
	%write tmp.cmd name $(RDTSC_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(RDTSC_EXE)
	@wbind $(RDTSC_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(RDTSC_EXE)
! endif

!ifdef RESET_EXE
$(RESET_EXE): $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_8042_LIB) $(HW_8254_LIB) $(SUBDIR)$(HPS)reset.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) $(WLINK_FLAGS) $(HW_CPU_LIB_WLINK_LIBRARIES) library $(HW_8042_LIB) library $(HW_8254_LIB) file $(SUBDIR)$(HPS)reset.obj option map=$(SUBDIR)$(HPS)reset.map
	%write tmp.cmd name $(RESET_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef APIC_EXE
$(APIC_EXE): $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_FLATREAL_LIB) $(SUBDIR)$(HPS)apic.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) $(WLINK_FLAGS) $(HW_FLATREAL_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)apic.obj option map=$(SUBDIR)$(HPS)apic.map
	%write tmp.cmd name $(APIC_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

$(DISPSN_EXE): $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)dispsn.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) $(HW_CPU_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)dispsn.obj option map=$(SUBDIR)$(HPS)dispsn.map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
	# protected mode only. real-mode Windows is a pain.
	%append tmp.cmd option protmode
!  endif
! endif
	%write tmp.cmd name $(DISPSN_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(DISPSN_EXE)
	@wbind $(DISPSN_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(DISPSN_EXE)
! endif

$(SSEOFF_EXE): $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)sseoff.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) $(HW_CPU_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)sseoff.obj option map=$(SUBDIR)$(HPS)sseoff.map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
	# protected mode only. real-mode Windows is a pain.
	%append tmp.cmd option protmode
!  endif
! endif
	%write tmp.cmd name $(SSEOFF_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(SSEOFF_EXE)
	@wbind $(SSEOFF_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(SSEOFF_EXE)
! endif

$(GDTLIST_EXE): $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)gdtlist.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) $(HW_CPU_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)gdtlist.obj option map=$(SUBDIR)$(HPS)gdtlist.map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
	# protected mode only. real-mode Windows is a pain.
	%append tmp.cmd option protmode
!  endif
! endif
	%write tmp.cmd name $(GDTLIST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(GDTLIST_EXE)
	@wbind $(GDTLIST_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(GDTLIST_EXE)
! endif

!ifdef GDTTAE_EXE
$(GDTTAE_EXE): $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)gdttae.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) $(HW_CPU_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)gdttae.obj option map=$(SUBDIR)$(HPS)gdttae.map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
	# protected mode only. real-mode Windows is a pain.
	%append tmp.cmd option protmode
!  endif
! endif
	%write tmp.cmd name $(GDTTAE_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(GDTTAE_EXE)
	@wbind $(GDTTAE_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(GDTTAE_EXE)
! endif
!endif

!ifdef INP_EXE
$(INP_EXE): $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)inp.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) $(HW_CPU_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)inp.obj option map=$(SUBDIR)$(HPS)inp.map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
	# protected mode only. real-mode Windows is a pain.
	%append tmp.cmd option protmode
!  endif
! endif
	%write tmp.cmd name $(INP_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(INP_EXE)
	@wbind $(INP_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(INP_EXE)
! endif
!endif

!ifdef OUTP_EXE
$(OUTP_EXE): $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)outp.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) $(HW_CPU_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)outp.obj option map=$(SUBDIR)$(HPS)outp.map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
	# protected mode only. real-mode Windows is a pain.
	%append tmp.cmd option protmode
!  endif
! endif
	%write tmp.cmd name $(OUTP_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(OUTP_EXE)
	@wbind $(OUTP_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(OUTP_EXE)
! endif
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_CPU_LIB)
          del tmp.cmd
          @echo Cleaning done

