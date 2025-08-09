# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = WINDOWS_CLIPBRD_EXE
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."

RCFLAGS_THIS = -i.. -i"../.."

CLIPBRD_EXE =  $(SUBDIR)$(HPS)clipbrd.exe
CLIPBRD_RES =  $(SUBDIR)$(HPS)clipbrd.res

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	$(CC) @tmp.cmd

all: lib exe

lib: .symbolic

exe: $(CLIPBRD_EXE) .symbolic

!ifdef CLIPBRD_RES
$(CLIPBRD_RES): clipbrd.rc
	$(RC) $(RCFLAGS_THIS) $(RCFLAGS) -fo=$(SUBDIR)$(HPS)clipbrd.res  $[@
!endif

$(CLIPBRD_EXE): $(SUBDIR)$(HPS)clipbrd.obj $(CLIPBRD_RES)
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)clipbrd.obj
!ifeq TARGET_MSDOS 16
	%write tmp.cmd EXPORT WndProc.1 PRIVATE RESIDENT
# NTS: Real-mode Windows will NOT run our program unless segments are MOVEABLE DISCARDABLE. Especially Windows 2.x and 3.0.
	%write tmp.cmd segment TYPE CODE PRELOAD FIXED DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD FIXED
!endif
	%write tmp.cmd option map=$(CLIPBRD_EXE).map
!ifdef CLIPBRD_RES
	%write tmp.cmd op resource=$(CLIPBRD_RES) name $(CLIPBRD_EXE)
!endif
	@wlink @tmp.cmd
!ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(CLIPBRD_EXE)
	@wbind $(CLIPBRD_EXE) -q -R $(CLIPBRD_RES)
!endif
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(CLIPBRD_EXE)
!endif
!ifeq TARGET_WINDOWS 20
	../../tool/win2xhdrpatch.pl $(CLIPBRD_EXE)
	../../tool/win2xstubpatch.pl $(CLIPBRD_EXE)
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
	  del $(SUBDIR)$(HPS)*.res
          del tmp.cmd
          @echo Cleaning done

