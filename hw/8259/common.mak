# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_8259_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i=.. -i..$(HPS)..

!ifeq TARGET_MSDOS 16
MAKE_IRQPATCH = 1
!endif

C_SOURCE =    8259.c
OBJS =        $(SUBDIR)$(HPS)8259.obj $(SUBDIR)$(HPS)8259poll.obj $(SUBDIR)$(HPS)8259icw.obj
TEST_EXE =    $(SUBDIR)$(HPS)test.exe

!ifdef MAKE_IRQPATCH
IRQPATCH_EXE = $(SUBDIR)$(HPS)irqpatch.exe
!endif

$(HW_8259_LIB): $(OBJS)
	wlib -q -b -c $(HW_8259_LIB) -+$(SUBDIR)$(HPS)8259.obj -+$(SUBDIR)$(HPS)8259poll.obj -+$(SUBDIR)$(HPS)8259icw.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@ 
	$(CC) @tmp.cmd

all: lib exe
      
lib: $(HW_8259_LIB) .symbolic
	
exe: $(TEST_EXE) $(IRQPATCH_EXE) .symbolic

!ifdef IRQPATCH_EXE
$(IRQPATCH_EXE): $(SUBDIR)$(HPS)irqpatch.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)irqpatch.obj name $(IRQPATCH_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

$(TEST_EXE): $(HW_8259_LIB) $(SUBDIR)$(HPS)test.obj
	$(LINKER) -q $(LDFLAGS) -fe=$(TEST_EXE) $(SUBDIR)$(HPS)test.obj $(HW_8259_LIB)

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_8259_LIB)
          del tmp.cmd
          @echo Cleaning done

