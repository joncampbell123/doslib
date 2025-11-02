# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = GAME_EXE
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."

RCFLAGS_THIS = -i.. -i"../.."

GAME_EXE =  $(SUBDIR)$(HPS)game.exe
GAME_RES =  $(SUBDIR)$(HPS)game.res

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	$(CC) @tmp.cmd

all: lib exe

lib: .symbolic

exe: $(GAME_EXE) .symbolic

!ifdef GAME_RES
$(GAME_RES): game.rc
	$(RC) $(RCFLAGS_THIS) $(RCFLAGS) -fo=$(SUBDIR)$(HPS)game.res  $[@
!endif

$(GAME_EXE): $(SUBDIR)$(HPS)game.obj $(GAME_RES)
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)game.obj
!ifeq TARGET_MSDOS 16
	%write tmp.cmd EXPORT WndProc.1 PRIVATE RESIDENT
# NTS: Real-mode Windows will NOT run our program unless segments are MOVEABLE DISCARDABLE. Especially Windows 2.x and 3.0.
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
!endif
	%write tmp.cmd option map=$(GAME_EXE).map
!ifdef GAME_RES
	%write tmp.cmd op resource=$(GAME_RES) name $(GAME_EXE)
!endif
	@wlink @tmp.cmd
!ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(GAME_EXE)
	@wbind $(GAME_EXE) -q -R $(GAME_RES)
!endif
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(GAME_EXE)
!endif
!ifeq TARGET_WINDOWS 20
	../../tool/win2xhdrpatch.pl $(GAME_EXE)
	../../tool/win2xstubpatch.pl $(GAME_EXE)
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
	  del $(SUBDIR)$(HPS)*.res
          del tmp.cmd
          @echo Cleaning done

