
# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."
NOW_BUILDING = TOOL_IPSDUMP

!ifndef NO_EXE
IPSDUMP_EXE = $(SUBDIR)$(HPS)ipsdump.$(EXEEXT)
IPSPATCH_EXE = $(SUBDIR)$(HPS)ipspatch.$(EXEEXT)
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	@$(CC) @tmp.cmd

all: exe

exe: $(IPSDUMP_EXE) $(IPSPATCH_EXE) .symbolic

!ifdef IPSDUMP_EXE
$(IPSDUMP_EXE): $(SUBDIR)$(HPS)ipsdump.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)ipsdump.obj
	%write tmp.cmd option map=$(IPSDUMP_EXE).map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD FIXED DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
!  endif
! endif
	%write tmp.cmd name $(IPSDUMP_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(IPSDUMP_EXE)
	@wbind $(IPSDUMP_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(IPSDUMP_EXE)
! endif
!endif

!ifdef IPSPATCH_EXE
$(IPSPATCH_EXE): $(SUBDIR)$(HPS)ipspatch.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)ipspatch.obj
	%write tmp.cmd option map=$(IPSPATCH_EXE).map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD FIXED DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
!  endif
! endif
	%write tmp.cmd name $(IPSPATCH_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(IPSPATCH_EXE)
	@wbind $(IPSPATCH_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(IPSPATCH_EXE)
! endif
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

