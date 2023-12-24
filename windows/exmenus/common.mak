# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = WINDOWS_EXMENUS_EXE
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."

RCFLAGS_THIS = -i.. -i"../.."

HELLO_EXE =  $(SUBDIR)$(HPS)hello.exe
HELLO_RES =  $(SUBDIR)$(HPS)hello.res

HELLO2_EXE =  $(SUBDIR)$(HPS)hello2.exe
HELLO2_RES =  $(SUBDIR)$(HPS)hello2.res

HELLO3_EXE =  $(SUBDIR)$(HPS)hello3.exe
HELLO3_RES =  $(SUBDIR)$(HPS)hello3.res

HELLO4_EXE =  $(SUBDIR)$(HPS)hello4.exe
HELLO4_RES =  $(SUBDIR)$(HPS)hello4.res

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	$(CC) @tmp.cmd

$(SUBDIR)$(HPS)helldld1.obj: helldld1.c
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_DLL) $[@
	$(CC) @tmp.cmd

all: lib exe

lib: .symbolic

exe: $(HELLO_EXE) $(HELLO2_EXE) $(HELLO3_EXE) $(HELLO4_EXE) .symbolic

$(HELLO_RES): hello.rc
	$(RC) $(RCFLAGS_THIS) $(RCFLAGS) -fo=$(SUBDIR)$(HPS)hello.res $[@

$(HELLO2_RES): hello2.rc
	$(RC) $(RCFLAGS_THIS) $(RCFLAGS) -fo=$(SUBDIR)$(HPS)hello2.res $[@

$(HELLO3_RES): hello3.rc
	$(RC) $(RCFLAGS_THIS) $(RCFLAGS) -fo=$(SUBDIR)$(HPS)hello3.res $[@

$(HELLO4_RES): hello4.rc
	$(RC) $(RCFLAGS_THIS) $(RCFLAGS) -fo=$(SUBDIR)$(HPS)hello4.res $[@

$(HELLO_EXE): $(SUBDIR)$(HPS)hello.obj $(HELLO_RES)
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)hello.obj
!ifeq TARGET_MSDOS 16
	%write tmp.cmd EXPORT WndProc.1 PRIVATE RESIDENT
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
# FIXME: How do you say "greater than or equal to" in Watcom WMAKE?
! ifeq TARGET_WINDOWS 31
	%write tmp.cmd library commdlg.lib
! endif
!endif
	%write tmp.cmd op resource=$(HELLO_RES) name $(HELLO_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(HELLO_EXE)
!endif

$(HELLO2_EXE): $(SUBDIR)$(HPS)hello2.obj $(HELLO2_RES)
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)hello2.obj
!ifeq TARGET_MSDOS 16
	%write tmp.cmd EXPORT WndProc.1 PRIVATE RESIDENT
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
# FIXME: How do you say "greater than or equal to" in Watcom WMAKE?
! ifeq TARGET_WINDOWS 31
	%write tmp.cmd library commdlg.lib
! endif
!endif
	%write tmp.cmd op resource=$(HELLO2_RES) name $(HELLO2_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(HELLO2_EXE)
!endif

$(HELLO3_EXE): $(SUBDIR)$(HPS)hello3.obj $(HELLO3_RES)
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)hello3.obj
!ifeq TARGET_MSDOS 16
	%write tmp.cmd EXPORT WndProc.1 PRIVATE RESIDENT
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
# FIXME: How do you say "greater than or equal to" in Watcom WMAKE?
! ifeq TARGET_WINDOWS 31
	%write tmp.cmd library commdlg.lib
! endif
!endif
	%write tmp.cmd op resource=$(HELLO3_RES) name $(HELLO3_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(HELLO3_EXE)
!endif

$(HELLO4_EXE): $(SUBDIR)$(HPS)hello4.obj $(HELLO4_RES)
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)hello4.obj
!ifeq TARGET_MSDOS 16
	%write tmp.cmd EXPORT WndProc.1 PRIVATE RESIDENT
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
# FIXME: How do you say "greater than or equal to" in Watcom WMAKE?
! ifeq TARGET_WINDOWS 31
	%write tmp.cmd library commdlg.lib
! endif
!endif
	%write tmp.cmd op resource=$(HELLO4_RES) name $(HELLO4_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(HELLO4_EXE)
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
	  del $(SUBDIR)$(HPS)*.res
          del tmp.cmd
          @echo Cleaning done

