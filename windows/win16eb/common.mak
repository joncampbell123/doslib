# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = WINDOWS_WIN16EB_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."

RCFLAGS_THIS = -i.. -i"../.."

HELLO_EXE =  $(SUBDIR)$(HPS)hello.exe
HELLO_RES =  $(SUBDIR)$(HPS)hello.res

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	$(CC) @tmp.cmd

all: lib exe

lib: $(WINDOWS_WIN16EB_LIB) .symbolic

exe: $(HELLO_EXE) .symbolic

$(HELLO_RES): hello.rc
	$(RC) $(RCFLAGS_THIS) $(RCFLAGS) -fo=$(SUBDIR)$(HPS)hello.res  $[@

$(WINDOWS_WIN16EB_LIB): $(SUBDIR)$(HPS)win16eb.obj
	wlib -q -b -c $(WINDOWS_WIN16EB_LIB) -+$(SUBDIR)$(HPS)win16eb.obj

$(HELLO_EXE): $(SUBDIR)$(HPS)hello.obj $(HELLO_RES) $(WINDOWS_WIN16EB_LIB) $(WINDOWS_WIN16EB_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) $(WINDOWS_WIN16EB_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)hello.obj
!ifeq TARGET_MSDOS 16
	%write tmp.cmd EXPORT WndProc.1 PRIVATE RESIDENT
	%write tmp.cmd segment TYPE CODE PRELOAD FIXED DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
!endif
	%write tmp.cmd op resource=$(HELLO_RES) name $(HELLO_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(HELLO_EXE)
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
	  del $(SUBDIR)$(HPS)*.res
          del tmp.cmd
          @echo Cleaning done

