# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_DOSBOXID_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i=.. -i..$(HPS)..

C_SOURCE =    iglib.c
OBJS =        $(SUBDIR)$(HPS)iglib.obj $(SUBDIR)$(HPS)igregio.obj $(SUBDIR)$(HPS)igrselio.obj $(SUBDIR)$(HPS)igprobe.obj $(SUBDIR)$(HPS)igreset.obj $(SUBDIR)$(HPS)igrident.obj $(SUBDIR)$(HPS)igverstr.obj
TEST_EXE =    $(SUBDIR)$(HPS)test.exe

$(HW_DOSBOXID_LIB): $(OBJS)
	wlib -q -b -c $(HW_DOSBOXID_LIB) -+$(SUBDIR)$(HPS)iglib.obj    -+$(SUBDIR)$(HPS)igregio.obj -+$(SUBDIR)$(HPS)igrselio.obj
	wlib -q -b -c $(HW_DOSBOXID_LIB) -+$(SUBDIR)$(HPS)igprobe.obj  -+$(SUBDIR)$(HPS)igreset.obj -+$(SUBDIR)$(HPS)igrident.obj
	wlib -q -b -c $(HW_DOSBOXID_LIB) -+$(SUBDIR)$(HPS)igverstr.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

all: lib exe
       
lib: $(HW_DOSBOXID_LIB) .symbolic

exe: $(TEST_EXE) .symbolic

!ifdef TEST_EXE
$(TEST_EXE): $(HW_DOSBOXID_LIB) $(HW_DOSBOXID_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj $(HW_DOSBOXID_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TEST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_DOSBOXID_LIB)
          del tmp.cmd
          @echo Cleaning done

