# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = WINDOWS_HEXMEM_EXE
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i=.. -i..$(HPS)..

RCFLAGS_THIS = -i=.. -i..$(HPS)..

HEXMEM_EXE =  $(SUBDIR)$(HPS)hexmem.exe
HEXMEM_RES =  $(SUBDIR)$(HPS)hexmem.res

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	$(CC) @tmp.cmd

$(SUBDIR)$(HPS)helldld1.obj: helldld1.c
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_DLL) $[@
	$(CC) @tmp.cmd

all: lib exe

lib: .symbolic

exe: $(HEXMEM_EXE) .symbolic

$(HEXMEM_RES): hexmem.rc
	$(RC) $(RCFLAGS_THIS) $(RCFLAGS) -fo=$(SUBDIR)$(HPS)hexmem.res  $[@

$(HEXMEM_EXE): $(SUBDIR)$(HPS)hexmem.obj $(HEXMEM_RES) $(WINDOWS_W32IMPHK_OBJ) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) $(HW_DOS_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)hexmem.obj
!ifdef NEED_W32IMPHK_OBJ
	%write tmp.cmd file $(WINDOWS_W32IMPHK_OBJ)
!endif
!ifeq TARGET_MSDOS 16
	%write tmp.cmd EXPORT WndProc.1 PRIVATE RESIDENT
	%write tmp.cmd EXPORT GoToDlgProc.2 PRIVATE RESIDENT
	%write tmp.cmd segment TYPE CODE PRELOAD FIXED DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
!endif
	%write tmp.cmd op resource=$(HEXMEM_RES) name $(HEXMEM_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(HEXMEM_EXE)
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
	  del $(SUBDIR)$(HPS)*.res
          del tmp.cmd
          @echo Cleaning done

