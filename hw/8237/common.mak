# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_8237_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."

C_SOURCE =    8237.c
OBJS =        $(SUBDIR)$(HPS)8237.obj $(SUBDIR)$(HPS)8237rdbs.obj
TEST_EXE =    $(SUBDIR)$(HPS)test.$(EXEEXT)

$(HW_8237_LIB): $(OBJS)
	wlib -q -b -c $(HW_8237_LIB) -+$(SUBDIR)$(HPS)8237.obj -+$(SUBDIR)$(HPS)8237rdbs.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	$(CC) @tmp.cmd

all: $(OMFSEGDG) lib exe
	
lib: $(HW_8237_LIB) .symbolic

exe: $(TEST_EXE) .symbolic

$(TEST_EXE): $(HW_8237_LIB) $(HW_8237_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj $(HW_8237_LIB_WLINK_LIBRARIES) name $(TEST_EXE) option map=$(TEST_EXE).map
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_8237_LIB)
          del tmp.cmd
          @echo Cleaning done

