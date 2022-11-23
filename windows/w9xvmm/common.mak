# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = WINDOWS_W9XVMM
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."
RCFLAGS_THIS = -i.. -i"../.."

VMMLIST_EXE = $(SUBDIR)$(HPS)vmmlist.exe

$(WINDOWS_W9XVMM_LIB): $(SUBDIR)$(HPS)vxd_enum.obj
	wlib -q -b -c $(WINDOWS_W9XVMM_LIB) -+$(SUBDIR)$(HPS)vxd_enum.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

.asm.obj:
	nasm -o $@ -f obj $(NASMFLAGS) $[@
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: lib exe

lib: $(WINDOWS_W9XVMM_LIB) .symbolic

exe: $(VMMLIST_EXE) .symbolic

$(VMMLIST_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(WINDOWS_W9XVMM_LIB) $(SUBDIR)$(HPS)vmmlist.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) library $(WINDOWS_W9XVMM_LIB) file $(SUBDIR)$(HPS)vmmlist.obj
!ifdef TARGET_WINDOWS
!ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD FIXED DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
!endif
!endif
	%write tmp.cmd name $(VMMLIST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(VMMLIST_EXE)
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
	  del $(SUBDIR)$(HPS)*.res
          del tmp.cmd
          @echo Cleaning done

