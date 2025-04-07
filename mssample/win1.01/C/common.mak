# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = MSSAMPLE_WIN1_01_C
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../../.."

RCFLAGS_THIS = -i.. -i"../../.."

HELLO_EXE =  $(SUBDIR)$(HPS)hello.exe
HELLO_RES =  $(SUBDIR)$(HPS)hello.res

TRACK_EXE =  $(SUBDIR)$(HPS)track.exe
TRACK_RES =  $(SUBDIR)$(HPS)track.res

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	$(CC) @tmp.cmd

all: lib exe

lib: .symbolic

exe: $(HELLO_EXE) $(TRACK_EXE) .symbolic

$(HELLO_RES): hello.rc
	../../../tool/icon1to3.pl hello.ico.win1 hello.ico
	$(RC) $(RCFLAGS_THIS) $(RCFLAGS) -fo=$(SUBDIR)$(HPS)hello.res  $[@

$(TRACK_RES): track.rc
	../../../tool/icon1to3.pl track.ico.win1 track.ico
	$(RC) $(RCFLAGS_THIS) $(RCFLAGS) -fo=$(SUBDIR)$(HPS)track.res  $[@

$(HELLO_EXE): $(SUBDIR)$(HPS)hello.obj $(HELLO_RES)
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)hello.obj
!ifeq TARGET_MSDOS 16
	%write tmp.cmd EXPORT HelloWndProc.1 PRIVATE RESIDENT
	%write tmp.cmd EXPORT About.2 PRIVATE RESIDENT
# NTS: Real-mode Windows will NOT run our program unless segments are MOVEABLE DISCARDABLE. Especially Windows 2.x and 3.0.
	%write tmp.cmd option DESCRIPTION 'Simple Microsoft Windows Application'
	%write tmp.cmd segment TYPE CODE MOVEABLE
	%write tmp.cmd segment TYPE DATA MOVEABLE
	%write tmp.cmd option HEAPSIZE=1024
	%write tmp.cmd option STACK=4096
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
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(HELLO_EXE)
!endif
!ifeq TARGET_WINDOWS 20
	../../../tool/win2xhdrpatch.pl $(HELLO_EXE)
	../../../tool/win2xstubpatch.pl $(HELLO_EXE)
!endif

$(TRACK_EXE): $(SUBDIR)$(HPS)track.obj $(TRACK_RES)
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)track.obj
!ifeq TARGET_MSDOS 16
	%write tmp.cmd EXPORT TrackWndProc.1 PRIVATE RESIDENT
	%write tmp.cmd EXPORT About.2 PRIVATE RESIDENT
# NTS: Real-mode Windows will NOT run our program unless segments are MOVEABLE DISCARDABLE. Especially Windows 2.x and 3.0.
	%write tmp.cmd option DESCRIPTION 'Microsoft Windows GDI, Mouse, & Menu Sample Application'
	%write tmp.cmd segment TYPE CODE MOVEABLE
	%write tmp.cmd segment TYPE DATA MOVEABLE
	%write tmp.cmd option HEAPSIZE=1024
	%write tmp.cmd option STACK=4096
!endif
	%write tmp.cmd option map=$(TRACK_EXE).map
!ifdef TRACK_RES
	%write tmp.cmd op resource=$(TRACK_RES) name $(TRACK_EXE)
!endif
	@wlink @tmp.cmd
!ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(TRACK_EXE)
	@wbind $(TRACK_EXE) -q -R $(TRACK_RES)
!endif
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(TRACK_EXE)
!endif
!ifeq TARGET_WINDOWS 20
	../../../tool/win2xhdrpatch.pl $(TRACK_EXE)
	../../../tool/win2xstubpatch.pl $(TRACK_EXE)
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
	  del $(SUBDIR)$(HPS)*.res
          del tmp.cmd
          @echo Cleaning done

