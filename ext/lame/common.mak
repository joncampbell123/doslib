# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = EXT_LAME_LIB
CFLAGS_THIS = -fr=nul -fo=$@ -i. -i.. -i"../.." -dHAVE_CONFIG_H

OBJS = &
    $(PRFX)bitstream.obj &
    $(PRFX)encoder.obj &
    $(PRFX)fft.obj &
    $(PRFX)gain_analysis.obj &
    $(PRFX)id3tag.obj &
    $(PRFX)lame.obj &
    $(PRFX)mpglib_interface.obj &
    $(PRFX)newmdct.obj &
    $(PRFX)presets.obj &
    $(PRFX)psymodel.obj &
    $(PRFX)quantize.obj &
    $(PRFX)quantize_pvt.obj &
    $(PRFX)reservoir.obj &
    $(PRFX)set_get.obj &
    $(PRFX)tables.obj &
    $(PRFX)takehiro.obj &
    $(PRFX)util.obj &
    $(PRFX)vbrquantize.obj &
    $(PRFX)vbrtag.obj &
    $(PRFX)version.obj &
    $(PRFX)common.obj &
    $(PRFX)dct64_i386.obj &
    $(PRFX)decode_i386.obj &
    $(PRFX)interface.obj &
    $(PRFX)layer1.obj &
    $(PRFX)layer2.obj &
    $(PRFX)layer3.obj &
    $(PRFX)tabinit.obj

all: lib exe .symbolic
       
!ifdef EXT_LAME_LAME_EXE
# NOTICE!!! Notice the "option stack=128000" below. It is needed to compensate for the Lame MP3 devs
#           and their fetish for declaring large arrays or data types on the call stack. When all of
#           those have been eliminated, then we can remove the option stack command.
#
#           Until that issue is fixed, you are guaranteed stack corruption and misery in any program
#           you link libmp3lame into and use, unless your program also declares a larger stack.
EXE_OBJS = &
    $(SUBDIR)$(HPS)brhist.obj &
    $(SUBDIR)$(HPS)console.obj &
    $(SUBDIR)$(HPS)get_audio.obj &
    $(SUBDIR)$(HPS)lame_main.obj &
    $(SUBDIR)$(HPS)lametime.obj &
    $(SUBDIR)$(HPS)main.obj &
    $(SUBDIR)$(HPS)parse.obj &
    $(SUBDIR)$(HPS)timestatus.obj

$(EXT_LAME_LAME_EXE): $(EXT_LAME_LIB) $(EXE_OBJS)
        *wlink option quiet system $(WLINK_SYSTEM) file {$(EXE_OBJS)} $(EXT_LAME_LIB_WLINK_LIBRARIES) option stack=128000 name $(EXT_LAME_LAME_EXE)
        @$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef EXT_LAME_LIB
PRFX = $(SUBDIR)$(HPS)
LIB_OBJS = $+$(OBJS)$-
PRFX = -+$(SUBDIR)$(HPS)
LIB_CMDS = $+$(OBJS)$-

$(EXT_LAME_LIB): $(LIB_OBJS)
        *wlib -q -b -c $(EXT_LAME_LIB) $(LIB_CMDS)
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
        $(CC) $(CFLAGS_THIS) $(CFLAGS) $[@

lib: $(EXT_LAME_LIB) .symbolic

exe: $(EXT_LAME_LAME_EXE) .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

