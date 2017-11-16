# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_NECPC98_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i=.. -i..$(HPS)..

C_SOURCE =    necpc98.c
OBJS =        $(SUBDIR)$(HPS)necpc98.obj $(SUBDIR)$(HPS)necpc98.obj
TEST_EXE =    $(SUBDIR)$(HPS)test.$(EXEEXT)
TATTR_EXE =	  $(SUBDIR)$(HPS)tattr.$(EXEEXT)

$(HW_NECPC98_LIB): $(OBJS)
	wlib -q -b -c $(HW_NECPC98_LIB) -+$(SUBDIR)$(HPS)necpc98.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe
       
lib: $(HW_NECPC98_LIB) .symbolic

exe: $(TEST_EXE) $(TATTR_EXE) .symbolic

# FIXME: Need a makefile var here what to run, rather than hard-code the path!! This assumes Linux!
isjp_cnv.h: isjp_utf.h
	echo Converting isjp_utf.h
	../../tool/utf2jis.pl <isjp_utf.h >isjp_cnv.h

$(TEST_EXE): isjp_cnv.h $(HW_NECPC98_LIB) $(HW_NECPC98_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(TEST_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj $(HW_NECPC98_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TEST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(TATTR_EXE): $(HW_NECPC98_LIB) $(HW_NECPC98_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)tattr.obj $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(TATTR_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)tattr.obj $(HW_NECPC98_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TATTR_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_NECPC98_LIB)
          del tmp.cmd
          @echo Cleaning done

