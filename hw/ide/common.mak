# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_IDE_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i=.. -i..$(HPS)..

C_SOURCE =    idelib.c
OBJS =        $(SUBDIR)$(HPS)idelib.obj $(SUBDIR)$(HPS)idelib.obj

!ifndef NO_TEST_EXE
TEST_EXE =    $(SUBDIR)$(HPS)test.exe
!endif

$(HW_IDE_LIB): $(OBJS)
	wlib -q -b -c $(HW_IDE_LIB) -+$(SUBDIR)$(HPS)idelib.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

all: lib exe
       
lib: $(HW_IDE_LIB) .symbolic

exe: $(TEST_EXE) .symbolic

!ifdef TEST_EXE
$(TEST_EXE): $(HW_IDE_LIB) $(HW_IDE_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj $(SUBDIR)$(HPS)testutil.obj $(SUBDIR)$(HPS)testmbox.obj $(SUBDIR)$(HPS)testcmui.obj $(SUBDIR)$(HPS)testbusy.obj $(SUBDIR)$(HPS)testnop.obj $(SUBDIR)$(HPS)testpwr.obj $(SUBDIR)$(HPS)testpiom.obj $(SUBDIR)$(HPS)testpiot.obj $(SUBDIR)$(HPS)testrvfy.obj $(SUBDIR)$(HPS)testrdwr.obj $(SUBDIR)$(HPS)testidnt.obj $(SUBDIR)$(HPS)testcdej.obj $(SUBDIR)$(HPS)testtadj.obj $(SUBDIR)$(HPS)testcdrm.obj $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_VGAGUI_LIB) $(HW_VGAGUI_LIB_DEPENDENCIES) $(HW_VGATTY_LIB) $(HW_VGATTY_LIB_DEPENDENCIES) $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_PCI_LIB) $(HW_PCI_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj file $(SUBDIR)$(HPS)testutil.obj file $(SUBDIR)$(HPS)testmbox.obj file $(SUBDIR)$(HPS)testcmui.obj file $(SUBDIR)$(HPS)testbusy.obj file $(SUBDIR)$(HPS)testnop.obj file $(SUBDIR)$(HPS)testpwr.obj file $(SUBDIR)$(HPS)testpiom.obj file $(SUBDIR)$(HPS)testpiot.obj file $(SUBDIR)$(HPS)testrvfy.obj file $(SUBDIR)$(HPS)testrdwr.obj file $(SUBDIR)$(HPS)testidnt.obj file $(SUBDIR)$(HPS)testcdej.obj file $(SUBDIR)$(HPS)testtadj.obj file $(SUBDIR)$(HPS)testcdrm.obj $(HW_IDE_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_VGAGUI_LIB_WLINK_LIBRARIES) $(HW_VGATTY_LIB_WLINK_LIBRARIES) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_PCI_LIB_WLINK_LIBRARIES) name $(TEST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_IDE_LIB)
          del tmp.cmd
          @echo Cleaning done

