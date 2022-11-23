# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."
NOW_BUILDING = HW_VGA2_LIB

TEST_EXE =     $(SUBDIR)$(HPS)test.$(EXEEXT)
TEST2_EXE =    $(SUBDIR)$(HPS)test2.$(EXEEXT)
TEST3_EXE =    $(SUBDIR)$(HPS)test3.$(EXEEXT)
TEST4_EXE =    $(SUBDIR)$(HPS)test4.$(EXEEXT)
TEST5_EXE =    $(SUBDIR)$(HPS)test5.$(EXEEXT)

$(HW_VGA2_LIB): $(SUBDIR)$(HPS)vga2.obj $(SUBDIR)$(HPS)vgagdcc.obj $(SUBDIR)$(HPS)valtegam.obj $(SUBDIR)$(HPS)valegasw.obj $(SUBDIR)$(HPS)alphamem.obj
	wlib -q -b -c $(HW_VGA2_LIB) -+$(SUBDIR)$(HPS)vga2.obj     -+$(SUBDIR)$(HPS)vgagdcc.obj  -+$(SUBDIR)$(HPS)valtegam.obj
	wlib -q -b -c $(HW_VGA2_LIB) -+$(SUBDIR)$(HPS)valegasw.obj -+$(SUBDIR)$(HPS)alphamem.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe
       
lib: $(HW_VGA2_LIB) .symbolic
	
exe: $(TEST_EXE) $(TEST2_EXE) $(TEST3_EXE) $(TEST4_EXE) $(TEST5_EXE) .symbolic

!ifdef TEST_EXE
$(TEST_EXE): $(HW_VGA2_LIB) $(HW_VGA2_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj
	%write tmp.cmd option quiet option map=$(TEST_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA2_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)test.obj name $(TEST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TEST2_EXE
$(TEST2_EXE): $(HW_VGA2_LIB) $(HW_VGA2_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test2.obj
	%write tmp.cmd option quiet option map=$(TEST2_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA2_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)test2.obj name $(TEST2_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TEST3_EXE
$(TEST3_EXE): $(HW_VGA2_LIB) $(HW_VGA2_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test3.obj
	%write tmp.cmd option quiet option map=$(TEST3_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA2_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)test3.obj name $(TEST3_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TEST4_EXE
$(TEST4_EXE): $(HW_VGA2_LIB) $(HW_VGA2_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test4.obj
	%write tmp.cmd option quiet option map=$(TEST4_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA2_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)test4.obj name $(TEST4_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TEST5_EXE
$(TEST5_EXE): $(HW_VGA2_LIB) $(HW_VGA2_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test5.obj
	%write tmp.cmd option quiet option map=$(TEST5_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA2_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)test5.obj name $(TEST5_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA2_LIB)
          del tmp.cmd
          @echo Cleaning done

