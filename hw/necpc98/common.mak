# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_NECPC98_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."

C_SOURCE =    necpc98.c
OBJS =        $(SUBDIR)$(HPS)necpc98.obj $(SUBDIR)$(HPS)sjis.obj $(SUBDIR)$(HPS)keyscan.obj
GDC_EXE =	  $(SUBDIR)$(HPS)gdc.$(EXEEXT)
TEST_EXE =    $(SUBDIR)$(HPS)test.$(EXEEXT)
TCHR_EXE =	  $(SUBDIR)$(HPS)tchr.$(EXEEXT)
TATTR_EXE =	  $(SUBDIR)$(HPS)tattr.$(EXEEXT)
SJIS1_EXE =	  $(SUBDIR)$(HPS)sjis1.$(EXEEXT)
SJIS2_EXE =	  $(SUBDIR)$(HPS)sjis2.$(EXEEXT)
SJIS3_EXE =	  $(SUBDIR)$(HPS)sjis3.$(EXEEXT)
TCGD_EXE =    $(SUBDIR)$(HPS)tcgd.$(EXEEXT)
CGDUMP_EXE =  $(SUBDIR)$(HPS)cgdump.$(EXEEXT)
INT18_EXE =   $(SUBDIR)$(HPS)int18.$(EXEEXT)
INT18KM_EXE = $(SUBDIR)$(HPS)int18km.$(EXEEXT)
TPAINT_EXE =  $(SUBDIR)$(HPS)tpaint.$(EXEEXT)

$(HW_NECPC98_LIB): $(OBJS)
	wlib -q -b -c $(HW_NECPC98_LIB) -+$(SUBDIR)$(HPS)necpc98.obj -+$(SUBDIR)$(HPS)sjis.obj -+$(SUBDIR)$(HPS)keyscan.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe
       
lib: $(HW_NECPC98_LIB) .symbolic

exe: $(TEST_EXE) $(TCHR_EXE) $(TATTR_EXE) $(SJIS1_EXE) $(SJIS2_EXE) $(SJIS3_EXE) $(GDC_EXE) $(TCGD_EXE) $(CGDUMP_EXE) $(INT18_EXE) $(INT18KM_EXE) $(TPAINT_EXE) .symbolic

# FIXME: Need a makefile var here what to run, rather than hard-code the path!! This assumes Linux!
isjp_cnv.h: isjp_utf.h
	echo Converting isjp_utf.h
	../../tool/utf2jis.pl <isjp_utf.h >isjp_cnv.h

$(TEST_EXE): isjp_cnv.h $(HW_NECPC98_LIB) $(HW_NECPC98_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(TEST_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj $(HW_NECPC98_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TEST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(TCHR_EXE): $(HW_NECPC98_LIB) $(HW_NECPC98_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)tchr.obj $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(TCHR_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)tchr.obj $(HW_NECPC98_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TCHR_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(TATTR_EXE): $(HW_NECPC98_LIB) $(HW_NECPC98_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)tattr.obj $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(TATTR_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)tattr.obj $(HW_NECPC98_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TATTR_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(SJIS1_EXE): isjp_cnv.h $(HW_NECPC98_LIB) $(HW_NECPC98_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)sjis1.obj $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(SJIS1_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)sjis1.obj $(HW_NECPC98_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(SJIS1_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(SJIS2_EXE): isjp_cnv.h $(HW_NECPC98_LIB) $(HW_NECPC98_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)sjis2.obj $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(SJIS2_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)sjis2.obj $(HW_NECPC98_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(SJIS2_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(SJIS3_EXE): isjp_cnv.h $(HW_NECPC98_LIB) $(HW_NECPC98_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)sjis3.obj $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(SJIS3_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)sjis3.obj $(HW_NECPC98_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(SJIS3_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(GDC_EXE): isjp_cnv.h $(HW_NECPC98_LIB) $(HW_NECPC98_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)gdc.obj $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(GDC_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)gdc.obj $(HW_NECPC98_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(GDC_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(TCGD_EXE): $(HW_NECPC98_LIB) $(HW_NECPC98_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)tcgd.obj $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(TCGD_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)tcgd.obj $(HW_NECPC98_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TCGD_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(CGDUMP_EXE): $(HW_NECPC98_LIB) $(HW_NECPC98_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)cgdump.obj $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(CGDUMP_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)cgdump.obj $(HW_NECPC98_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(CGDUMP_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(INT18_EXE): $(HW_NECPC98_LIB) $(HW_NECPC98_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)int18.obj $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(INT18_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)int18.obj $(HW_NECPC98_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(INT18_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(INT18KM_EXE): $(HW_NECPC98_LIB) $(HW_NECPC98_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)int18km.obj $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(INT18KM_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)int18km.obj $(HW_NECPC98_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(INT18KM_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(TPAINT_EXE): $(HW_NECPC98_LIB) $(HW_NECPC98_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)tpaint.obj $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(TPAINT_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)tpaint.obj $(HW_NECPC98_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TPAINT_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_NECPC98_LIB)
          del tmp.cmd
          @echo Cleaning done

