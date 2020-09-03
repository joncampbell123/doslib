# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."
NOW_BUILDING = FMT_MINIPNG_LIB

OBJS =         $(SUBDIR)$(HPS)minipng.obj $(SUBDIR)$(HPS)minipnid.obj $(SUBDIR)$(HPS)minipnph.obj $(SUBDIR)$(HPS)minipnrb.obj $(SUBDIR)$(HPS)minipnrw.obj $(SUBDIR)$(HPS)minipnx8.obj

$(FMT_MINIPNG_LIB): $(OBJS)
	wlib -q -b -c $(FMT_MINIPNG_LIB) -+$(SUBDIR)$(HPS)minipng.obj -+$(SUBDIR)$(HPS)minipnid.obj
	wlib -q -b -c $(FMT_MINIPNG_LIB) -+$(SUBDIR)$(HPS)minipnph.obj -+$(SUBDIR)$(HPS)minipnrb.obj
	wlib -q -b -c $(FMT_MINIPNG_LIB) -+$(SUBDIR)$(HPS)minipnrw.obj -+$(SUBDIR)$(HPS)minipnx8.obj

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
       
lib: $(HW_VGA_LIB) $(FMT_MINIPNG_LIB) .symbolic
	
exe: $(TEST_EXE) $(TESTOLD1_EXE) .symbolic

!ifdef TEST_EXE
$(TEST_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(FMT_MINIPNG_LIB) $(FMT_MINIPNG_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj
	%write tmp.cmd option quiet option map=$(TEST_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(FMT_MINIPNG_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)test.obj name $(TEST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TESTOLD1_EXE
$(TESTOLD1_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(FMT_MINIPNG_LIB) $(FMT_MINIPNG_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)testold1.obj
	%write tmp.cmd option quiet option map=$(TESTOLD1_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(FMT_MINIPNG_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)testold1.obj name $(TESTOLD1_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA_LIB)
          del tmp.cmd
          @echo Cleaning done

