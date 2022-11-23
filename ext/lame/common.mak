# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = EXT_LAME_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i. -i.. -i"../.." -dHAVE_CONFIG_H

OBJS = &
    $(SUBDIR)$(HPS)bitstream.obj &
    $(SUBDIR)$(HPS)encoder.obj &
    $(SUBDIR)$(HPS)fft.obj &
    $(SUBDIR)$(HPS)gain_analysis.obj &
    $(SUBDIR)$(HPS)id3tag.obj &
    $(SUBDIR)$(HPS)lame.obj &
    $(SUBDIR)$(HPS)mpglib_interface.obj &
    $(SUBDIR)$(HPS)newmdct.obj &
    $(SUBDIR)$(HPS)presets.obj &
    $(SUBDIR)$(HPS)psymodel.obj &
    $(SUBDIR)$(HPS)quantize.obj &
    $(SUBDIR)$(HPS)quantize_pvt.obj &
    $(SUBDIR)$(HPS)reservoir.obj &
    $(SUBDIR)$(HPS)set_get.obj &
    $(SUBDIR)$(HPS)tables.obj &
    $(SUBDIR)$(HPS)takehiro.obj &
    $(SUBDIR)$(HPS)util.obj &
    $(SUBDIR)$(HPS)vbrquantize.obj &
    $(SUBDIR)$(HPS)vbrtag.obj &
    $(SUBDIR)$(HPS)version.obj &
    $(SUBDIR)$(HPS)common.obj &
    $(SUBDIR)$(HPS)dct64_i386.obj &
    $(SUBDIR)$(HPS)decode_i386.obj &
    $(SUBDIR)$(HPS)interface.obj &
    $(SUBDIR)$(HPS)layer1.obj &
    $(SUBDIR)$(HPS)layer2.obj &
    $(SUBDIR)$(HPS)layer3.obj &
    $(SUBDIR)$(HPS)tabinit.obj

!ifdef EXT_LAME_LAME_EXE
# NOTICE!!! Notice the "option stack=128000" below. It is needed to compensate for the Lame MP3 devs
#           and their fetish for declaring large arrays or data types on the call stack. When all of
#           those have been eliminated, then we can remove the option stack command.
#
#           Until that issue is fixed, you are guaranteed stack corruption and misery in any program
#           you link libmp3lame into and use, unless your program also declares a larger stack.
$(EXT_LAME_LAME_EXE): $(EXT_LAME_LIB) $(SUBDIR)$(HPS)brhist.obj $(SUBDIR)$(HPS)console.obj $(SUBDIR)$(HPS)get_audio.obj $(SUBDIR)$(HPS)lame_main.obj $(SUBDIR)$(HPS)lametime.obj $(SUBDIR)$(HPS)main.obj $(SUBDIR)$(HPS)parse.obj $(SUBDIR)$(HPS)timestatus.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)brhist.obj file $(SUBDIR)$(HPS)console.obj file $(SUBDIR)$(HPS)get_audio.obj file $(SUBDIR)$(HPS)lame_main.obj file $(SUBDIR)$(HPS)lametime.obj file $(SUBDIR)$(HPS)main.obj file $(SUBDIR)$(HPS)parse.obj file $(SUBDIR)$(HPS)timestatus.obj $(EXT_LAME_LIB_WLINK_LIBRARIES) option stack=128000 name $(EXT_LAME_LAME_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef EXT_LAME_LIB
$(EXT_LAME_LIB): $(OBJS)
	wlib -q -b -c $(EXT_LAME_LIB) &
    -+$(SUBDIR)$(HPS)bitstream.obj &
    -+$(SUBDIR)$(HPS)encoder.obj &
    -+$(SUBDIR)$(HPS)fft.obj &
    -+$(SUBDIR)$(HPS)gain_analysis.obj &
    -+$(SUBDIR)$(HPS)id3tag.obj &
    -+$(SUBDIR)$(HPS)lame.obj &
    -+$(SUBDIR)$(HPS)mpglib_interface.obj &
    -+$(SUBDIR)$(HPS)newmdct.obj &
    -+$(SUBDIR)$(HPS)presets.obj &
    -+$(SUBDIR)$(HPS)psymodel.obj &
    -+$(SUBDIR)$(HPS)quantize.obj &
    -+$(SUBDIR)$(HPS)quantize_pvt.obj &
    -+$(SUBDIR)$(HPS)reservoir.obj &
    -+$(SUBDIR)$(HPS)set_get.obj &
    -+$(SUBDIR)$(HPS)tables.obj &
    -+$(SUBDIR)$(HPS)takehiro.obj &
    -+$(SUBDIR)$(HPS)util.obj &
    -+$(SUBDIR)$(HPS)vbrquantize.obj &
    -+$(SUBDIR)$(HPS)vbrtag.obj &
    -+$(SUBDIR)$(HPS)version.obj &
    -+$(SUBDIR)$(HPS)common.obj &
    -+$(SUBDIR)$(HPS)dct64_i386.obj &
    -+$(SUBDIR)$(HPS)decode_i386.obj &
    -+$(SUBDIR)$(HPS)interface.obj &
    -+$(SUBDIR)$(HPS)layer1.obj &
    -+$(SUBDIR)$(HPS)layer2.obj &
    -+$(SUBDIR)$(HPS)layer3.obj &
    -+$(SUBDIR)$(HPS)tabinit.obj
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

all: lib exe .symbolic
       
lib: $(EXT_LAME_LIB) .symbolic

exe: $(EXT_LAME_LAME_EXE) .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

