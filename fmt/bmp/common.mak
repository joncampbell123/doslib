
# TODO: OS/2 target: What can we #define to tell the header files which OS/2
#       environment we're doing? (Command prompt app. vs Presentation Manager app vs.
#       "fullscreen" app.)

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."
NOW_BUILDING = FMT_BMP_LIB

OBJS =        $(SUBDIR)$(HPS)bmpfimg.obj $(SUBDIR)$(HPS)bmpread.obj $(SUBDIR)$(HPS)bmpwrite.obj $(SUBDIR)$(HPS)cpy.obj $(SUBDIR)$(HPS)c32t24.obj $(SUBDIR)$(HPS)maskshft.obj $(SUBDIR)$(HPS)mkbf8.obj $(SUBDIR)$(HPS)strid1.obj $(SUBDIR)$(HPS)c_16555.obj $(SUBDIR)$(HPS)c_16565.obj $(SUBDIR)$(HPS)c16t555b.obj $(SUBDIR)$(HPS)c_32b8.obj $(SUBDIR)$(HPS)c_32t24.obj $(SUBDIR)$(HPS)c32t24.obj $(SUBDIR)$(HPS)cht24555.obj $(SUBDIR)$(HPS)cht24565.obj $(SUBDIR)$(HPS)c_none.obj

!ifndef NO_EXE
T_CPY0_EXE = $(SUBDIR)$(HPS)t_cpy0.$(EXEEXT)
T_CPY1_EXE = $(SUBDIR)$(HPS)t_cpy1.$(EXEEXT)
!endif

$(FMT_BMP_LIB): $(OBJS)
	wlib -q -b -c $(FMT_BMP_LIB) -+$(SUBDIR)$(HPS)bmpfimg.obj  -+$(SUBDIR)$(HPS)bmpread.obj
	wlib -q -b -c $(FMT_BMP_LIB) -+$(SUBDIR)$(HPS)bmpwrite.obj -+$(SUBDIR)$(HPS)c32t24.obj
	wlib -q -b -c $(FMT_BMP_LIB) -+$(SUBDIR)$(HPS)maskshft.obj -+$(SUBDIR)$(HPS)mkbf8.obj
	wlib -q -b -c $(FMT_BMP_LIB) -+$(SUBDIR)$(HPS)strid1.obj   -+$(SUBDIR)$(HPS)cpy.obj
	wlib -q -b -c $(FMT_BMP_LIB) -+$(SUBDIR)$(HPS)c_16555.obj  -+$(SUBDIR)$(HPS)c_16565.obj
	wlib -q -b -c $(FMT_BMP_LIB) -+$(SUBDIR)$(HPS)c16t555b.obj -+$(SUBDIR)$(HPS)c_32b8.obj
	wlib -q -b -c $(FMT_BMP_LIB) -+$(SUBDIR)$(HPS)c_32t24.obj  -+$(SUBDIR)$(HPS)c32t24.obj
	wlib -q -b -c $(FMT_BMP_LIB) -+$(SUBDIR)$(HPS)cht24555.obj -+$(SUBDIR)$(HPS)cht24565.obj
	wlib -q -b -c $(FMT_BMP_LIB) -+$(SUBDIR)$(HPS)c_none.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	@$(CC) @tmp.cmd

all: lib exe

exe: $(T_CPY0_EXE) $(T_CPY1_EXE) .symbolic

lib: $(FMT_BMP_LIB) .symbolic

!ifdef T_CPY0_EXE
$(T_CPY0_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(FMT_BMP_LIB) $(FMT_BMP_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)t_cpy0.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)t_cpy0.obj $(HW_DOS_LIB_WLINK_LIBRARIES) $(FMT_BMP_LIB_WLINK_LIBRARIES)
	%write tmp.cmd option map=$(T_CPY0_EXE).map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD FIXED DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
!  endif
! endif
	%write tmp.cmd name $(T_CPY0_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(T_CPY0_EXE)
	@wbind $(T_CPY0_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(T_CPY0_EXE)
! endif
!endif

!ifdef T_CPY1_EXE
$(T_CPY1_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(FMT_BMP_LIB) $(FMT_BMP_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)t_cpy1.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)t_cpy1.obj $(HW_DOS_LIB_WLINK_LIBRARIES) $(FMT_BMP_LIB_WLINK_LIBRARIES)
	%write tmp.cmd option map=$(T_CPY1_EXE).map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD FIXED DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
!  endif
! endif
	%write tmp.cmd name $(T_CPY1_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(T_CPY1_EXE)
	@wbind $(T_CPY1_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(T_CPY1_EXE)
! endif
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(FMT_BMP_LIB)
          del tmp.cmd
          @echo Cleaning done

 

