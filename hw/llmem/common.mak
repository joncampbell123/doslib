# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_LLMEM_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."
AFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."
NASMFLAGS_THIS = 

# NTS: CPU functions here are to be moved at some point to the cpu library!
C_SOURCE =    cpu.c
OBJS =        $(SUBDIR)$(HPS)llmemasm.obj $(SUBDIR)$(HPS)llmem.obj

TEST_EXE =    $(SUBDIR)$(HPS)test.$(EXEEXT)

$(HW_LLMEM_LIB): $(OBJS)
	wlib -q -b -c $(HW_LLMEM_LIB) -+$(SUBDIR)$(HPS)llmemasm.obj -+$(SUBDIR)$(HPS)llmem.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

.asm.obj:
	nasm -o $@ -f obj $(NASMFLAGS) $[@
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe
	
lib: $(HW_LLMEM_LIB) .symbolic
	
exe: $(TEST_EXE) .symbolic

$(TEST_EXE): $(HW_LLMEM_LIB) $(HW_LLMEM_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj library $(HW_LLMEM_LIB) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TEST_EXE) option map=$(TEST_EXE).map
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_CPU_LIB)
          del tmp.cmd
          @echo Cleaning done

