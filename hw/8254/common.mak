# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_8254_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i=.. -i..$(HPS)..

C_SOURCE =    8254.c
OBJS =        $(SUBDIR)$(HPS)8254.obj $(SUBDIR)$(HPS)8254rd.obj $(SUBDIR)$(HPS)8254tick.obj $(SUBDIR)$(HPS)8254ticr.obj $(SUBDIR)$(HPS)8254wait.obj $(SUBDIR)$(HPS)8254prbe.obj
TEST_EXE =    $(SUBDIR)$(HPS)test.$(EXEEXT)

TPCRAPID_EXE = $(SUBDIR)$(HPS)tpcrapid.$(EXEEXT)
TPCRAPI2_EXE = $(SUBDIR)$(HPS)tpcrapi2.$(EXEEXT)
TPCRAPI3_EXE = $(SUBDIR)$(HPS)tpcrapi3.$(EXEEXT)
TPCMON1_EXE  = $(SUBDIR)$(HPS)tpcmon1.$(EXEEXT)

$(HW_8254_LIB): $(OBJS)
	wlib -q -b -c $(HW_8254_LIB) -+$(SUBDIR)$(HPS)8254.obj     -+$(SUBDIR)$(HPS)8254rd.obj   -+$(SUBDIR)$(HPS)8254tick.obj
	wlib -q -b -c $(HW_8254_LIB) -+$(SUBDIR)$(HPS)8254ticr.obj -+$(SUBDIR)$(HPS)8254wait.obj -+$(SUBDIR)$(HPS)8254prbe.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe

lib: $(HW_8254_LIB) .symbolic

exe: $(TEST_EXE) $(TPCRAPID_EXE) $(TPCRAPI2_EXE) $(TPCRAPI3_EXE) $(TPCMON1_EXE) .symbolic

$(TEST_EXE): $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TEST_EXE) option map=$(TEST_EXE).map
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(TPCRAPID_EXE): $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)tpcrapid.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)tpcrapid.obj $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TPCRAPID_EXE) option map=$(TPCRAPID_EXE).map
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(TPCRAPI2_EXE): $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)tpcrapi2.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)tpcrapi2.obj $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TPCRAPI2_EXE) option map=$(TPCRAPI2_EXE).map
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(TPCRAPI3_EXE): $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)tpcrapi3.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)tpcrapi3.obj $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TPCRAPI3_EXE) option map=$(TPCRAPI3_EXE).map
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(TPCMON1_EXE): $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)tpcmon1.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)tpcmon1.obj $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TPCMON1_EXE) option map=$(TPCMON1_EXE).map
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_8254_LIB)
          del tmp.cmd
          @echo Cleaning done

