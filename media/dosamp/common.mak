
NOW_BUILDING = MEDIA_DOSAMP_EXE

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i=.. -i..$(HPS)..

#DEBUG
#CFLAGS_THIS += -DDBG

DOSAMP_EXE =    $(SUBDIR)$(HPS)dosamp.$(EXEEXT)

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) exe

exe: $(DOSAMP_EXE) .symbolic

!ifdef DOSAMP_EXE
$(DOSAMP_EXE): $(WINDOWS_W9XVMM_LIB) $(WINDOWS_W9XVMM_LIB_DEPENDENCIES) $(HW_SNDSB_LIB) $(HW_SNDSB_LIB_DEPENDENCIES) $(HW_SNDSBPNP_LIB) $(HW_SNDSBPNP_LIB_DEPENDENCIES) $(HW_8237_LIB) $(HW_8237_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_ISAPNP_LIB) $(HW_ISAPNP_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)dosamp.obj $(SUBDIR)$(HPS)ts8254.obj $(SUBDIR)$(HPS)tsrdtsc.obj $(SUBDIR)$(HPS)tsrdtsc2.obj $(SUBDIR)$(HPS)fsref.obj $(SUBDIR)$(HPS)fsalloc.obj $(SUBDIR)$(HPS)fssrcfd.obj $(SUBDIR)$(HPS)cvip816.obj $(SUBDIR)$(HPS)cvip168.obj $(SUBDIR)$(HPS)cvipsm8.obj $(SUBDIR)$(HPS)cvipsm16.obj $(SUBDIR)$(HPS)cvipsm.obj $(SUBDIR)$(HPS)cvipms16.obj $(SUBDIR)$(HPS)cvipms8.obj $(SUBDIR)$(HPS)cvipms.obj $(SUBDIR)$(HPS)cvrdbuf.obj $(SUBDIR)$(HPS)cvrdbfrs.obj $(SUBDIR)$(HPS)trkrbase.obj
	%write tmp.cmd
! ifeq TARGET_MSDOS 16 # Sound Blaster library needs more stack than default
	%append tmp.cmd option stack=4096
! endif
	%append tmp.cmd $(HW_ISAPNP_LIB_WLINK_LIBRARIES)
	%append tmp.cmd option quiet option map=$(DOSAMP_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)dosamp.obj file $(SUBDIR)$(HPS)ts8254.obj file $(SUBDIR)$(HPS)tsrdtsc.obj file $(SUBDIR)$(HPS)tsrdtsc2.obj file $(SUBDIR)$(HPS)fsref.obj file $(SUBDIR)$(HPS)fsalloc.obj file $(SUBDIR)$(HPS)fssrcfd.obj file $(SUBDIR)$(HPS)cvip816.obj file $(SUBDIR)$(HPS)cvip168.obj file $(SUBDIR)$(HPS)cvipsm8.obj file $(SUBDIR)$(HPS)cvipsm16.obj file $(SUBDIR)$(HPS)cvipsm.obj file $(SUBDIR)$(HPS)cvipms16.obj file $(SUBDIR)$(HPS)cvipms8.obj file $(SUBDIR)$(HPS)cvipms.obj file $(SUBDIR)$(HPS)cvrdbuf.obj file $(SUBDIR)$(HPS)cvrdbfrs.obj file $(SUBDIR)$(HPS)trkrbase.obj $(HW_SNDSB_LIB_WLINK_LIBRARIES) $(HW_8237_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(WINDOWS_W9XVMM_LIB_WLINK_LIBRARIES) $(HW_SNDSBPNP_LIB_WLINK_LIBRARIES)
	%append tmp.cmd name $(DOSAMP_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_SNDSB_LIB)
          del tmp.cmd
          @echo Cleaning done

