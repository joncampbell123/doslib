
# TODO: OS/2 target: What can we #define to tell the header files which OS/2
#       environment we're doing? (Command prompt app. vs Presentation Manager app vs.
#       "fullscreen" app.)

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."
NOW_BUILDING = HW_DOS_LIB

!ifdef TARGET_WINDOWS
! ifndef TARGET_OS2
!  ifeq TARGET_MSDOS 16
BUILD_WIN=1
!  endif
! endif
!endif

!ifdef BUILD_WIN
..$(HPS)stub$(HPS)dos86s$(HPS)stub.exe:
	@cd ..$(HPS)stub
	@$(MAKECMD) build exe dos86s
	@cd $(HERE)

HELLO_EXE=$(SUBDIR)$(HPS)hello.exe
$(HELLO_EXE): $(SUBDIR)$(HPS)hello.obj ..$(HPS)stub$(HPS)dos86s$(HPS)stub.exe
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)hello.obj
	%write tmp.cmd option map=$(SUBDIR)$(HPS)hello.map
	%write tmp.cmd option stub=..$(HPS)stub$(HPS)dos86s$(HPS)stub.exe
	%write tmp.cmd option stack=4096 option heapsize=1024
	%write tmp.cmd EXPORT myWndProc.1
	%write tmp.cmd segment TYPE CODE FIXED SHARED PRELOAD READONLY
	%write tmp.cmd segment TYPE DATA FIXED SHARED PRELOAD READWRITE
	%write tmp.cmd name $(HELLO_EXE)
	@wlink @tmp.cmd
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(HELLO_EXE)
! endif
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

.asm.obj:
	nasm -o $@ -f obj $(NASMFLAGS) $[@

all: lib exe .symbolic

exe: $(HELLO_EXE) $(STUB_EXE) .symbolic

lib: .symbolic

clean: .symbolic
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_DOS_LIB)
          del tmp.cmd
          @echo Cleaning done

