# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = WINDOWS_NTVDM
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."

RCFLAGS_THIS = -i.. -i"../.."

VDDC1_EXE =  $(SUBDIR)$(HPS)vddc1.exe
VDDC1D_DLL =  $(SUBDIR)$(HPS)vddc1d.dll

!ifdef WINDOWS_NTVDMVDD_LIB
$(WINDOWS_NTVDMVDD_LIB): $(SUBDIR)$(HPS)ntvdmvdd.obj
	wlib -q -b -c $(WINDOWS_NTVDMVDD_LIB) -+$(SUBDIR)$(HPS)ntvdmvdd.obj
	# NTVDMVDD.LIB also needs to trigger DLL imports from NTVDM.EXE.
	# This is again in a perl script, because there's no way in hell I'm going
	# to type out all that crap in this makefile
	perl ./wlib-ntvdm $(WINDOWS_NTVDMVDD_LIB)
!endif

$(WINDOWS_NTVDMLIB_LIB): $(SUBDIR)$(HPS)ntvdmlib.obj $(SUBDIR)$(HPS)nterrstr.obj $(SUBDIR)$(HPS)ntvdmiam.obj
	wlib -q -b -c $(WINDOWS_NTVDMLIB_LIB) -+$(SUBDIR)$(HPS)ntvdmlib.obj -+$(SUBDIR)$(HPS)nterrstr.obj -+$(SUBDIR)$(HPS)ntvdmiam.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

$(SUBDIR)$(HPS)vddc1d.obj: vddc1d.c
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	$(CC) @tmp.cmd

.asm.obj:
	nasm -o $@ -f obj $(NASMFLAGS) $[@
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: lib exe

lib: $(WINDOWS_NTVDMLIB_LIB) $(WINDOWS_NTVDMVDD_LIB) $(VDDC1D_DLL) .symbolic

exe: $(VDDC1_EXE) .symbolic

!ifdef TARGET_WINDOWS
! ifeq TARGET_MSDOS 32
!  ifeq TARGET_WINDOWS 40
VDDC1D_DLL_THIS_BUILD=1
!  endif
! endif
!endif

!ifdef VDDC1D_DLL_THIS_BUILD
$(VDDC1D_DLL): $(HW_DOS_LIB) $(HW_CPU_LIB) $(WINDOWS_NTVDMLIB_LIB) $(WINDOWS_NTVDMVDD_LIB) $(SUBDIR)$(HPS)vddc1d.obj
	%write tmp.cmd option quiet system $(WLINK_DLL_SYSTEM) library $(WINDOWS_NTVDMVDD_LIB) library $(WINDOWS_NTVDMLIB_LIB) file $(SUBDIR)$(HPS)vddc1d.obj
	%write tmp.cmd option modname='VDDC1D'
! ifeq TARGET_MSDOS 32
	%write tmp.cmd option nostdcall
! endif
# explanation: if we use the IMPLIB option, Watcom will go off and make an import library that
# cases all references to refer to HELLDLD1.DLL within the NE image, which Windows does NOT like.
# we need to ensure the DLL name is encoded by itself without a .DLL extension which is more
# compatible with Windows and it's internal functions.
#
# Frankly I'm surprised that Watcom has this bug considering how long it's been around... Kind of disappointed really
	%write tmp.cmd option impfile=$(SUBDIR)$(HPS)VDDC1D.LCF
	%write tmp.cmd name $(VDDC1D_DLL)
	@wlink @tmp.cmd
! ifeq TARGET_MSDOS 32
	perl lcflib.pl --nofilter $(SUBDIR)$(HPS)VDDC1D.LIB $(SUBDIR)$(HPS)VDDC1D.LCF
! endif
!else
# we ask the winnt target to build it's DLL, then copy it into our build tree
winnt$(HPS)vddc1d.dll:
	@$(MAKECMD) build lib winnt

$(VDDC1D_DLL): winnt$(HPS)vddc1d.dll
	@$(COPY) winnt$(HPS)vddc1d.dll $(VDDC1D_DLL)
!endif

$(VDDC1_EXE): $(HW_DOS_LIB) $(HW_CPU_LIB) $(WINDOWS_NTVDMLIB_LIB) $(SUBDIR)$(HPS)vddc1.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) library $(HW_DOS_LIB) library $(HW_CPU_LIB) library $(WINDOWS_NTVDMLIB_LIB) file $(SUBDIR)$(HPS)vddc1.obj
!ifdef TARGET_WINDOWS
!ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD FIXED DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
!endif
!endif
	%write tmp.cmd name $(VDDC1_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(VDDC1_EXE)
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
	  del $(SUBDIR)$(HPS)*.res
          del tmp.cmd
          @echo Cleaning done

