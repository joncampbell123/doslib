# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
!ifeq TARGET_MSDOS 16
NO_LIB = 1
NO_EXE = 1
!endif

NOW_BUILDING = MEDIA_PLAYMP3_EXE
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.." -dHAVE_CONFIG_H

OBJS = $(SUBDIR)$(HPS)bit.obj $(SUBDIR)$(HPS)decoder.obj $(SUBDIR)$(HPS)fixed.obj $(SUBDIR)$(HPS)frame.obj $(SUBDIR)$(HPS)huffman.obj $(SUBDIR)$(HPS)layer12.obj $(SUBDIR)$(HPS)layer3.obj $(SUBDIR)$(HPS)stream.obj $(SUBDIR)$(HPS)synth.obj $(SUBDIR)$(HPS)timer.obj $(SUBDIR)$(HPS)version.obj

!ifndef NO_EXE
PLAYMP3_EXE = $(SUBDIR)$(HPS)playmp3.exe
! ifdef TARGET_WINDOWS
PLAYMP3_RES = $(SUBDIR)$(HPS)playmp3.res
! endif
! ifndef TARGET_WINDOWS
M4ATOAAC_EXE = $(SUBDIR)$(HPS)m4atoaac.exe
! endif
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

all: lib exe .symbolic
       
lib: .symbolic

exe: $(PLAYMP3_EXE) $(M4ATOAAC_EXE) .symbolic

!ifndef NO_EXE

! ifdef PLAYMP3_RES
$(PLAYMP3_RES): playmp3.rc
	$(RC) $(RCFLAGS_THIS) $(RCFLAGS) -fo=$(SUBDIR)$(HPS)playmp3.res $[@
! endif

! ifdef TARGET_WINDOWS
$(PLAYMP3_EXE): $(EXT_ZLIB_LIB) $(EXT_FLAC_LIB) $(EXT_SPEEX_LIB) $(EXT_LIBOGG_LIB) $(EXT_VORBIS_LIB) $(EXT_LIBMAD_LIB) $(EXT_FAAD_LIB) $(SUBDIR)$(HPS)playmp3.obj $(SUBDIR)$(HPS)playcom.obj $(SUBDIR)$(HPS)qt.obj $(SUBDIR)$(HPS)qtreader.obj $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(PLAYMP3_RES)
! else
$(PLAYMP3_EXE): $(WIN_W9XVMM_LIB) $(EXT_ZLIB_LIB) $(EXT_FLAC_LIB) $(EXT_SPEEX_LIB) $(EXT_LIBOGG_LIB) $(EXT_VORBIS_LIB) $(EXT_LIBMAD_LIB) $(EXT_FAAD_LIB) $(SUBDIR)$(HPS)playmp3.obj $(SUBDIR)$(HPS)playcom.obj $(SUBDIR)$(HPS)qt.obj $(SUBDIR)$(HPS)qtreader.obj $(HW_SNDSB_LIB) $(HW_SNDSB_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_VGAGUI_LIB) $(HW_VGAGUI_LIB_DEPENDENCIES) $(HW_VGATTY_LIB) $(HW_VGATTY_LIB_DEPENDENCIES) $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_8237_LIB) $(HW_8237_LIB_DEPENDENCIES) $(HW_ISAPNP_LIB) $(HW_ISAPNP_LIB_DEPENDENCIES) $(HW_SNDSBPNP_LIB) $(HW_SNDSBPNP_LIB_DEPENDENCIES) $(HW_ULTRASND_LIB) $(HW_ULTRASND_LIB_DEPENDENCIES) $(PLAYMP3_RES) $(WINDOWS_W9XVMM_LIB)
! endif
	%write tmp.cmd option quiet option map=$(PLAYMP3_EXE).map option eliminate option showdead system $(WLINK_SYSTEM)
	%write tmp.cmd file $(SUBDIR)$(HPS)playcom.obj file $(SUBDIR)$(HPS)qt.obj file $(SUBDIR)$(HPS)qtreader.obj file $(SUBDIR)$(HPS)playmp3.obj
! ifndef TARGET_WINDOWS # pure DOS
	%write tmp.cmd $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_VGAGUI_LIB_WLINK_LIBRARIES) $(HW_VGATTY_LIB_WLINK_LIBRARIES) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_SNDSB_LIB_WLINK_LIBRARIES) $(HW_8237_LIB_WLINK_LIBRARIES) $(HW_ISAPNP_LIB_WLINK_LIBRARIES) $(HW_SNDSBPNP_LIB_WLINK_LIBRARIES) $(HW_ULTRASND_LIB_WLINK_LIBRARIES) $(WINDOWS_W9XVMM_LIB_WLINK_LIBRARIES)
! endif
	%write tmp.cmd $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(EXT_LIBMAD_LIB_WLINK_LIBRARIES) $(EXT_FAAD_LIB_WLINK_LIBRARIES) $(EXT_LIBOGG_LIB_WLINK_LIBRARIES) $(EXT_VORBIS_LIB_WLINK_LIBRARIES) $(EXT_SPEEX_LIB_WLINK_LIBRARIES) $(EXT_FLAC_LIB_WLINK_LIBRARIES) $(EXT_ZLIB_LIB_WLINK_LIBRARIES)
! ifdef TARGET_WINDOWS
	%write tmp.cmd op resource=$(PLAYMP3_RES) library winmm.lib
! endif
	%write tmp.cmd name $(PLAYMP3_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

! ifdef M4ATOAAC_EXE
$(M4ATOAAC_EXE): $(EXT_ZLIB_LIB) $(SUBDIR)$(HPS)m4atoaac.obj $(SUBDIR)$(HPS)qt.obj $(SUBDIR)$(HPS)qtreader.obj
	%write tmp.cmd option quiet option map=$(M4ATOAAC_EXE).map option eliminate option showdead system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)m4atoaac.obj file $(SUBDIR)$(HPS)qt.obj file $(SUBDIR)$(HPS)qtreader.obj $(EXT_ZLIB_LIB_WLINK_LIBRARIES) name $(M4ATOAAC_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! endif
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

