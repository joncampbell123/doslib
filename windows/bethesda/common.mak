# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = WINDOWS_HELLO_EXE
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."

RCFLAGS_THIS = -i.. -i"../.."

HELLO_EXE =  $(SUBDIR)$(HPS)hello.exe

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	$(CC) @tmp.cmd

all: exe

exe: $(HELLO_EXE) .symbolic

$(HELLO_EXE): $(SUBDIR)$(HPS)hello.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)hello.obj
!ifeq TARGET_MSDOS 16
# NTS: Real-mode Windows will NOT run our program unless segments are MOVEABLE DISCARDABLE. Especially Windows 2.x and 3.0.
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
!endif
	%write tmp.cmd option map=$(HELLO_EXE).map
	@wlink @tmp.cmd
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

