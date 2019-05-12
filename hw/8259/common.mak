# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_8259_LIB
CFLAGS_THIS = -fr=nul -fo=$@ -i.. -i"../.."

!ifeq TARGET_MSDOS 16
MAKE_IRQPATCH = 1
!endif

C_SOURCE =    8259.c
TEST_EXE =    $(SUBDIR)$(HPS)test.$(EXEEXT)

OBJS = &
    $(PRFX)8259.obj &
    $(PRFX)8259poll.obj &
    $(PRFX)8259icw.obj

all: $(OMFSEGDG) lib exe
      
!ifdef MAKE_IRQPATCH
IRQPATCH_EXE = $(SUBDIR)$(HPS)irqpatch.$(EXEEXT)
!endif

PRFX = $(SUBDIR)$(HPS)
LIB_OBJS = $+$(OBJS)$-
PRFX = -+$(SUBDIR)$(HPS)
LIB_CMDS = $+$(OBJS)$-

$(HW_8259_LIB): $(LIB_OBJS)
	*wlib -q -b -c $(HW_8259_LIB) $(LIB_CMDS)

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	$(CC) $(CFLAGS_THIS) $(CFLAGS) $[@ 

lib: $(HW_8259_LIB) .symbolic
	
exe: $(TEST_EXE) $(IRQPATCH_EXE) .symbolic

!ifdef IRQPATCH_EXE
$(IRQPATCH_EXE): $(SUBDIR)$(HPS)irqpatch.obj
	*wlink option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)irqpatch.obj name $(IRQPATCH_EXE) option map=$(IRQPATCH_EXE).map
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

$(TEST_EXE): $(HW_8259_LIB) $(SUBDIR)$(HPS)test.obj
	*wlink option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj name $(TEST_EXE) option map=$(TEST_EXE).map $(HW_8259_LIB_WLINK_LIBRARIES)
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_8259_LIB)
          del tmp.cmd
          @echo Cleaning done

