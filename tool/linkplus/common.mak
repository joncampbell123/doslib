
# TODO: OS/2 target: What can we #define to tell the header files which OS/2
#       environment we're doing? (Command prompt app. vs Presentation Manager app vs.
#       "fullscreen" app.)

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."
NOW_BUILDING = TOOL_LINKER

!ifeq TARGET_MSDOS 32
LNKDOS16_EXE = $(SUBDIR)$(HPS)lnkdos16.$(EXEEXT)
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	@$(CC) @tmp.cmd

all: lib exe

exe: $(LNKDOS16_EXE) .symbolic

lib: $(FMT_OMF_LIB) .symbolic

!ifdef LNKDOS16_EXE
$(LNKDOS16_EXE): $(FMT_OMF_LIB) $(FMT_OMF_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)lnkdos16.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)lnkdos16.obj $(FMT_OMF_LIB_WLINK_LIBRARIES)
	%write tmp.cmd option map=$(LNKDOS16_EXE).map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD FIXED DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
!  endif
! endif
	%write tmp.cmd name $(LNKDOS16_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(LNKDOS16_EXE)
	@wbind $(LNKDOS16_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(LNKDOS16_EXE)
! endif
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(FMT_OMF_LIB)
          del tmp.cmd
          @echo Cleaning done

