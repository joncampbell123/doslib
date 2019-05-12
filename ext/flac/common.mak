# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = EXT_FLAC_LIB
CFLAGS_THIS = -fr=nul -fo=$@ -i.. -i"../.." -dHAVE_CONFIG_H

OBJS = &
    $(PRFX)bitmath.obj &
    $(PRFX)bitreader.obj &
    $(PRFX)bitwriter.obj &
    $(PRFX)cpu.obj &
    $(PRFX)crc.obj &
    $(PRFX)fixed.obj &
    $(PRFX)float.obj &
    $(PRFX)format.obj &
    $(PRFX)lpc.obj &
    $(PRFX)md5.obj &
    $(PRFX)memory.obj &
    $(PRFX)metadata_iterators.obj &
    $(PRFX)metadata_object.obj &
    $(PRFX)ogg_decoder_aspect.obj &
    $(PRFX)ogg_encoder_aspect.obj &
    $(PRFX)ogg_helper.obj &
    $(PRFX)ogg_mapping.obj &
    $(PRFX)stream_decoder.obj &
    $(PRFX)stream_encoder.obj &
    $(PRFX)stream_encoder_framing.obj &
    $(PRFX)window.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
        $(CC) $(CFLAGS_THIS) $(CFLAGS) $[@

all: lib exe .symbolic
       
lib: $(EXT_FLAC_LIB) .symbolic

exe: $(EXT_FLAC_EXE) .symbolic

!ifdef EXT_FLAC_LIB
PRFX = $(SUBDIR)$(HPS)
LIB_OBJS = $+$(OBJS)$-
PRFX = -+$(SUBDIR)$(HPS)
LIB_CMDS = $+$(OBJS)$-

$(EXT_FLAC_LIB): $(LIB_OBJS)
    *wlib -q -b -c $(EXT_FLAC_LIB) $(LIB_CMDS)
!endif

!ifdef EXT_FLAC_EXE
EXE_OBJS = &
    $(SUBDIR)$(HPS)main.obj &
    $(SUBDIR)$(HPS)analyze.obj &
    $(SUBDIR)$(HPS)decode.obj &
    $(SUBDIR)$(HPS)encode.obj &
    $(SUBDIR)$(HPS)iffscan.obj &
    $(SUBDIR)$(HPS)utils.obj &
    $(SUBDIR)$(HPS)vorbiscomment.obj &
    $(SUBDIR)$(HPS)foreign_metadata.obj &
    $(SUBDIR)$(HPS)getopt.obj &
    $(SUBDIR)$(HPS)getopt1.obj &
    $(SUBDIR)$(HPS)local_string_utils.obj &
    $(SUBDIR)$(HPS)cuesheet.obj &
    $(SUBDIR)$(HPS)file.obj &
    $(SUBDIR)$(HPS)picture.obj &
    $(SUBDIR)$(HPS)replaygain.obj &
    $(SUBDIR)$(HPS)seektable.obj &
    $(SUBDIR)$(HPS)replaygain_synthesis.obj &
    $(SUBDIR)$(HPS)replaygain_analysis.obj

$(EXT_FLAC_EXE): $(EXT_LIBOGG_LIB) $(EXT_FLAC_LIB) $(EXE_OBJS)
        *wlink option quiet system $(WLINK_SYSTEM) $(EXT_LIBOGG_LIB_WLINK_LIBRARIES) $(EXT_FLAC_LIB_WLINK_LIBRARIES) file {$(EXE_OBJS)} name $(EXT_FLAC_EXE)
        @$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

