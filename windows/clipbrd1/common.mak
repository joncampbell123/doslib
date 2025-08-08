# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = WINDOWS_HELLO_EXE
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."

RCFLAGS_THIS = -i.. -i"../.."

HELLO_EXE =  $(SUBDIR)$(HPS)hello.exe
HELLO_RES =  $(SUBDIR)$(HPS)hello.res

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
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
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

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
	  del $(SUBDIR)$(HPS)*.res
          del tmp.cmd
          @echo Cleaning done

