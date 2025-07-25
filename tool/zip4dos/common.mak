
# TODO: OS/2 target: What can we #define to tell the header files which OS/2
#       environment we're doing? (Command prompt app. vs Presentation Manager app vs.
#       "fullscreen" app.)

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."
NOW_BUILDING = TOOL_ZIP4DOS

!ifndef NO_EXE
ZIP4DOS_EXE = $(SUBDIR)$(HPS)zip4dos.$(EXEEXT)
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	@$(CC) @tmp.cmd

all: lib exe

exe: $(ZIP4DOS_EXE) .symbolic

lib: .symbolic

!ifdef ZIP4DOS_EXE
$(ZIP4DOS_EXE): $(SUBDIR)$(HPS)zip4dos.obj $(SUBDIR)$(HPS)zipboots.obj $(SUBDIR)$(HPS)zipcrc.obj $(EXT_ZLIB_LIB)
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)zip4dos.obj file $(SUBDIR)$(HPS)zipboots.obj file $(SUBDIR)$(HPS)zipcrc.obj $(EXT_ZLIB_LIB_WLINK_LIBRARIES)
	%write tmp.cmd option map=$(ZIP4DOS_EXE).map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD FIXED DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
!  endif
! endif
	%write tmp.cmd name $(ZIP4DOS_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(ZIP4DOS_EXE)
	@wbind $(ZIP4DOS_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(ZIP4DOS_EXE)
! endif
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

