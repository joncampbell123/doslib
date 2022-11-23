# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_8259_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."

!ifeq TARGET_MSDOS 16
MAKE_IRQPATCH = 1
!endif

C_SOURCE =    8259.c
OBJS =        $(SUBDIR)$(HPS)8259.obj $(SUBDIR)$(HPS)8259poll.obj $(SUBDIR)$(HPS)8259icw.obj
TEST_EXE =    $(SUBDIR)$(HPS)test.$(EXEEXT)

!ifdef MAKE_IRQPATCH
IRQPATCH_EXE = $(SUBDIR)$(HPS)irqpatch.$(EXEEXT)
!endif

$(HW_8259_LIB): $(OBJS)
	wlib -q -b -c $(HW_8259_LIB) -+$(SUBDIR)$(HPS)8259.obj -+$(SUBDIR)$(HPS)8259poll.obj -+$(SUBDIR)$(HPS)8259icw.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@ 
	$(CC) @tmp.cmd

all: $(OMFSEGDG) lib exe
      
lib: $(HW_8259_LIB) .symbolic
	
exe: $(TEST_EXE) $(IRQPATCH_EXE) .symbolic

!ifdef IRQPATCH_EXE
$(IRQPATCH_EXE): $(SUBDIR)$(HPS)irqpatch.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)irqpatch.obj name $(IRQPATCH_EXE) option map=$(IRQPATCH_EXE).map
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

$(TEST_EXE): $(HW_8259_LIB) $(SUBDIR)$(HPS)test.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj name $(TEST_EXE) option map=$(TEST_EXE).map $(HW_8259_LIB_WLINK_LIBRARIES)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_8259_LIB)
          del tmp.cmd
          @echo Cleaning done

