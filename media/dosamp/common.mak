
NOW_BUILDING = MEDIA_DOSAMP_EXE

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."

#DEBUG
#CFLAGS_THIS += -DDBG

!ifeq TARGET_MSDOS 16
! ifeq MMODE c
! else
!  ifeq MMODE s
!  else
DOSAMP_EXE =    $(SUBDIR)$(HPS)dosamp.$(EXEEXT)
! endif
! endif
!else
DOSAMP_EXE =    $(SUBDIR)$(HPS)dosamp.$(EXEEXT)
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	@$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) exe

exe: $(DOSAMP_EXE) .symbolic

!ifdef DOSAMP_EXE
DOSAMP_EXE_DEPS = $(SUBDIR)$(HPS)dosamp.obj $(SUBDIR)$(HPS)ts8254.obj $(SUBDIR)$(HPS)tsrdtsc.obj $(SUBDIR)$(HPS)tsrdtsc2.obj $(SUBDIR)$(HPS)fsref.obj $(SUBDIR)$(HPS)fsalloc.obj $(SUBDIR)$(HPS)fssrcfd.obj $(SUBDIR)$(HPS)cvip816.obj $(SUBDIR)$(HPS)cvip168.obj $(SUBDIR)$(HPS)cvipsm8.obj $(SUBDIR)$(HPS)cvipsm16.obj $(SUBDIR)$(HPS)cvipsm.obj $(SUBDIR)$(HPS)cvipms16.obj $(SUBDIR)$(HPS)cvipms8.obj $(SUBDIR)$(HPS)cvipms.obj $(SUBDIR)$(HPS)cvrdbuf.obj $(SUBDIR)$(HPS)cvrdbfrs.obj $(SUBDIR)$(HPS)cvrdbfrf.obj $(SUBDIR)$(HPS)cvrdbfrb.obj $(SUBDIR)$(HPS)trkrbase.obj $(SUBDIR)$(HPS)tmpbuf.obj $(SUBDIR)$(HPS)resample.obj $(SUBDIR)$(HPS)snirq.obj $(SUBDIR)$(HPS)sndcard.obj $(SUBDIR)$(HPS)sc_sb.obj $(SUBDIR)$(HPS)termios.obj $(SUBDIR)$(HPS)cstr.obj $(SUBDIR)$(HPS)fs.obj $(SUBDIR)$(HPS)pof_gofn.obj $(SUBDIR)$(HPS)pof_tty.obj $(SUBDIR)$(HPS)shdropls.obj $(SUBDIR)$(HPS)shdropwn.obj $(SUBDIR)$(HPS)isadma.obj

DOSAMP_EXE_WLINK = file $(SUBDIR)$(HPS)dosamp.obj file $(SUBDIR)$(HPS)ts8254.obj file $(SUBDIR)$(HPS)tsrdtsc.obj file $(SUBDIR)$(HPS)tsrdtsc2.obj file $(SUBDIR)$(HPS)fsref.obj file $(SUBDIR)$(HPS)fsalloc.obj file $(SUBDIR)$(HPS)fssrcfd.obj file $(SUBDIR)$(HPS)cvip816.obj file $(SUBDIR)$(HPS)cvip168.obj file $(SUBDIR)$(HPS)cvipsm8.obj file $(SUBDIR)$(HPS)cvipsm16.obj file $(SUBDIR)$(HPS)cvipsm.obj file $(SUBDIR)$(HPS)cvipms16.obj file $(SUBDIR)$(HPS)cvipms8.obj file $(SUBDIR)$(HPS)cvipms.obj file $(SUBDIR)$(HPS)cvrdbuf.obj file $(SUBDIR)$(HPS)cvrdbfrs.obj file $(SUBDIR)$(HPS)cvrdbfrf.obj file $(SUBDIR)$(HPS)cvrdbfrb.obj file $(SUBDIR)$(HPS)trkrbase.obj file $(SUBDIR)$(HPS)tmpbuf.obj file $(SUBDIR)$(HPS)resample.obj file $(SUBDIR)$(HPS)snirq.obj file $(SUBDIR)$(HPS)sndcard.obj file $(SUBDIR)$(HPS)sc_sb.obj file $(SUBDIR)$(HPS)termios.obj file $(SUBDIR)$(HPS)cstr.obj file $(SUBDIR)$(HPS)fs.obj file $(SUBDIR)$(HPS)pof_gofn.obj file $(SUBDIR)$(HPS)pof_tty.obj file $(SUBDIR)$(HPS)shdropls.obj file $(SUBDIR)$(HPS)shdropwn.obj file $(SUBDIR)$(HPS)isadma.obj

! ifdef TARGET_WINDOWS
# Windows target.
# NTS: We include code to talk directly to 8254 in case the WINMM multimedia timer or RDTSC are not available
DOSAMP_EXE_DEPS += $(WINDOWS_W9XVMM_LIB) $(WINDOWS_W9XVMM_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)tswinmm.obj $(SUBDIR)$(HPS)sc_winmm.obj $(SUBDIR)$(HPS)sc_dsound.obj $(SUBDIR)$(HPS)tsqpc.obj $(SUBDIR)$(HPS)dsound.obj $(SUBDIR)$(HPS)winshell.obj $(SUBDIR)$(HPS)mmsystem.obj $(SUBDIR)$(HPS)commdlg.obj

DOSAMP_EXE_WLINK += $(WINDOWS_W9XVMM_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)tswinmm.obj file $(SUBDIR)$(HPS)sc_winmm.obj file $(SUBDIR)$(HPS)sc_dsound.obj file $(SUBDIR)$(HPS)tsqpc.obj file $(SUBDIR)$(HPS)dsound.obj file $(SUBDIR)$(HPS)winshell.obj file $(SUBDIR)$(HPS)mmsystem.obj file $(SUBDIR)$(HPS)commdlg.obj
! else
# MS-DOS target
DOSAMP_EXE_DEPS += $(WINDOWS_W9XVMM_LIB) $(WINDOWS_W9XVMM_LIB_DEPENDENCIES) $(HW_SNDSB_LIB) $(HW_SNDSB_LIB_DEPENDENCIES) $(HW_SNDSBPNP_LIB) $(HW_SNDSBPNP_LIB_DEPENDENCIES) $(HW_8237_LIB) $(HW_8237_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_ISAPNP_LIB) $(HW_ISAPNP_LIB_DEPENDENCIES)

DOSAMP_EXE_WLINK += $(WINDOWS_W9XVMM_LIB_WLINK_LIBRARIES) $(HW_SNDSB_LIB_WLINK_LIBRARIES) $(HW_8237_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_SNDSBPNP_LIB_WLINK_LIBRARIES) $(HW_ISAPNP_LIB_WLINK_LIBRARIES)
! endif
!endif

!ifdef DOSAMP_EXE
$(DOSAMP_EXE): $(DOSAMP_EXE_DEPS)
	%write tmp.cmd
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
# Windows 3.x
	%append tmp.cmd segment TYPE CODE PRELOAD FIXED DISCARDABLE SHARED
	%append tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
# I can't be bothered to make DOSAMP work in Windows real mode. Too much work
# to deal with real mode segments that can change on a whim. No thanks.
	%append tmp.cmd option protmode
!  endif
! else
# MS-DOS
!  ifeq TARGET_MSDOS 16 # Sound Blaster library needs more stack than default
	%append tmp.cmd option stack=4096
!  endif
! endif
	%append tmp.cmd option quiet option map=$(DOSAMP_EXE).map system $(WLINK_CON_SYSTEM) $(DOSAMP_EXE_WLINK)
	%append tmp.cmd name $(DOSAMP_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

