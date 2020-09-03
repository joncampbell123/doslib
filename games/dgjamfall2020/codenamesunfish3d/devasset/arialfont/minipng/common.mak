# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../../../../../.."
NOW_BUILDING = WOO_SORC_SET1_VRLTEST

TEST_EXE =     $(SUBDIR)$(HPS)test.$(EXEEXT)
TESTOLD1_EXE = $(SUBDIR)$(HPS)testold1.$(EXEEXT)

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe
       
lib: $(HW_VGA_LIB) $(EXT_ZLIBIMIN_LIB) .symbolic
	
exe: $(TEST_EXE) $(TESTOLD1_EXE) .symbolic

!ifdef TEST_EXE
$(TEST_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(EXT_ZLIBIMIN_LIB) $(EXT_ZLIBIMIN_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj $(SUBDIR)$(HPS)minipng.obj
	%write tmp.cmd option quiet option map=$(TEST_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(EXT_ZLIBIMIN_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)test.obj file $(SUBDIR)$(HPS)minipng.obj name $(TEST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif
	$(COPY) ..$(HPS)arialsmall_0.png sml.png
	$(COPY) ..$(HPS)arialmed_0.png   med.png
	$(COPY) ..$(HPS)ariallarge_0.png lrg.png

!ifdef TESTOLD1_EXE
$(TESTOLD1_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(EXT_ZLIBIMIN_LIB) $(EXT_ZLIBIMIN_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)testold1.obj $(SUBDIR)$(HPS)minipng.obj
	%write tmp.cmd option quiet option map=$(TESTOLD1_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(EXT_ZLIBIMIN_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)testold1.obj file $(SUBDIR)$(HPS)minipng.obj name $(TESTOLD1_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA_LIB)
          del tmp.cmd
          @echo Cleaning done

