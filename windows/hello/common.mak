# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = WINDOWS_HELLO_EXE
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."

RCFLAGS_THIS = -i.. -i"../.."

HELLO_EXE =  $(SUBDIR)$(HPS)hello.exe

!ifeq TARGET_WINDOWS 20
# Open Watcom cannot yet compile Windows 1.x/2.x compatible resources
!else
HELLO_RES =  $(SUBDIR)$(HPS)hello.res
!endif

!ifndef WIN386
HELLDLL1_EXE =  $(SUBDIR)$(HPS)HELLDLL1.EXE
HELLDLL1_RES =  $(SUBDIR)$(HPS)HELLDLL1.RES
HELLDLD1_DLL =  $(SUBDIR)$(HPS)HELLDLD1.DLL
HELLDLD1_DLL_NOEXT =  $(SUBDIR)$(HPS)HELLDLD1
! ifeq TARGET_MSDOS 16
HELLDLD1_LIB =  $(SUBDIR)$(HPS)HELLDLD1.LIB
! endif
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	$(CC) @tmp.cmd

$(SUBDIR)$(HPS)helldld1.obj: helldld1.c
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_DLL) $[@
	$(CC) @tmp.cmd

all: lib exe

lib: $(HELLDLD1_DLL) $(HELLDLD1_LIB) .symbolic

exe: $(HELLO_EXE) $(HELLDLL1_EXE) .symbolic

!ifdef HELLO_RES
$(HELLO_RES): hello.rc
	$(RC) $(RCFLAGS_THIS) $(RCFLAGS) -fo=$(SUBDIR)$(HPS)hello.res  $[@
!endif

!ifdef HELLDLL1_RES
$(HELLDLL1_RES): helldll1.rc
	$(RC) $(RCFLAGS_THIS) $(RCFLAGS) -fo=$(SUBDIR)$(HPS)HELLDLL1.RES  $[@
!endif

$(HELLO_EXE): $(SUBDIR)$(HPS)hello.obj $(HELLO_RES)
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)hello.obj
!ifeq TARGET_MSDOS 16
	%write tmp.cmd EXPORT WndProc.1 PRIVATE RESIDENT
# NTS: Real-mode Windows will NOT run our program unless segments are MOVEABLE DISCARDABLE. Especially Windows 2.x and 3.0.
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
!endif
	%write tmp.cmd option map=$(HELLO_EXE).map
!ifdef HELLO_RES
	%write tmp.cmd op resource=$(HELLO_RES) name $(HELLO_EXE)
!endif
	@wlink @tmp.cmd
!ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(HELLO_EXE)
	@wbind $(HELLO_EXE) -q -R $(HELLO_RES)
!endif
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(HELLO_EXE)
!endif
!ifeq TARGET_WINDOWS 20
	../../tool/win2xhdrpatch.pl $(HELLO_EXE)
	../../tool/win2xstubpatch.pl $(HELLO_EXE)
!endif

!ifdef HELLDLL1_EXE
$(HELLDLL1_EXE): $(SUBDIR)$(HPS)helldll1.obj $(HELLDLL1_RES)
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)helldll1.obj
	%write tmp.cmd library $(SUBDIR)$(HPS)HELLDLD1.LIB
!ifeq TARGET_MSDOS 16
	%write tmp.cmd EXPORT WndProc.1 PRIVATE RESIDENT
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
! endif
	%write tmp.cmd op resource=$(HELLDLL1_RES) name $(HELLDLL1_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(HELLDLL1_EXE)
! endif
!endif

!ifdef HELLDLD1_DLL
$(HELLDLD1_DLL) $(HELLDLD1_LIB): $(SUBDIR)$(HPS)helldld1.obj
	%write tmp.cmd option quiet system $(WLINK_DLL_SYSTEM) file $(SUBDIR)$(HPS)helldld1.obj
! ifndef WIN386
	%write tmp.cmd option modname='HELLDLD1'
! endif
!ifeq TARGET_MSDOS 16
	%write tmp.cmd EXPORT HELLOMSG.1
	%write tmp.cmd EXPORT HELLODRAW.2
! endif
!ifeq TARGET_MSDOS 32
!  ifndef WIN386
	%write tmp.cmd option nostdcall
	%write tmp.cmd EXPORT HelloMsg.1='_HelloMsg@0'
	%write tmp.cmd EXPORT HelloDraw.2='_HelloDraw@12'
!  endif
! endif
!ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
! endif
# explanation: if we use the IMPLIB option, Watcom will go off and make an import library that
# cases all references to refer to HELLDLD1.DLL within the NE image, which Windows does NOT like.
# we need to ensure the DLL name is encoded by itself without a .DLL extension which is more
# compatible with Windows and it's internal functions.
#
# Frankly I'm surprised that Watcom has this bug considering how long it's been around... Kind of disappointed really
! ifndef WIN386
	%write tmp.cmd option impfile=$(SUBDIR)$(HPS)HELLDLD1.LCF
! endif
	%write tmp.cmd name $(HELLDLD1_DLL)
	@wlink @tmp.cmd
! ifeq TARGET_MSDOS 16
# here'w where we need the perl interpreter
	perl lcflib.pl --filter $(SUBDIR)$(HPS)HELLDLD1.LIB $(SUBDIR)$(HPS)HELLDLD1.LCF
! endif
! ifeq TARGET_MSDOS 32
	perl lcflib.pl --nofilter $(SUBDIR)$(HPS)HELLDLD1.LIB $(SUBDIR)$(HPS)HELLDLD1.LCF
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(HELLDLD1_DLL)
! endif
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
	  del $(SUBDIR)$(HPS)*.res
          del tmp.cmd
          @echo Cleaning done

