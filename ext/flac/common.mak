# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = EXT_FLAC_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.." -dHAVE_CONFIG_H

OBJS = $(SUBDIR)$(HPS)bitmath.obj $(SUBDIR)$(HPS)bitreader.obj $(SUBDIR)$(HPS)bitwriter.obj $(SUBDIR)$(HPS)cpu.obj $(SUBDIR)$(HPS)crc.obj $(SUBDIR)$(HPS)fixed.obj $(SUBDIR)$(HPS)float.obj $(SUBDIR)$(HPS)format.obj $(SUBDIR)$(HPS)lpc.obj $(SUBDIR)$(HPS)md5.obj $(SUBDIR)$(HPS)memory.obj $(SUBDIR)$(HPS)metadata_iterators.obj $(SUBDIR)$(HPS)metadata_object.obj $(SUBDIR)$(HPS)ogg_decoder_aspect.obj $(SUBDIR)$(HPS)ogg_encoder_aspect.obj $(SUBDIR)$(HPS)ogg_helper.obj $(SUBDIR)$(HPS)ogg_mapping.obj $(SUBDIR)$(HPS)stream_decoder.obj $(SUBDIR)$(HPS)stream_encoder.obj $(SUBDIR)$(HPS)stream_encoder_framing.obj $(SUBDIR)$(HPS)window.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

all: lib exe .symbolic
       
lib: $(EXT_FLAC_LIB) .symbolic

exe: $(EXT_FLAC_EXE) .symbolic

!ifdef EXT_FLAC_LIB
$(EXT_FLAC_LIB): $(OBJS)
	wlib -q -b -c $(EXT_FLAC_LIB) -+$(SUBDIR)$(HPS)bitmath.obj -+$(SUBDIR)$(HPS)bitreader.obj -+$(SUBDIR)$(HPS)bitwriter.obj -+$(SUBDIR)$(HPS)cpu.obj -+$(SUBDIR)$(HPS)crc.obj -+$(SUBDIR)$(HPS)fixed.obj -+$(SUBDIR)$(HPS)float.obj -+$(SUBDIR)$(HPS)format.obj -+$(SUBDIR)$(HPS)lpc.obj -+$(SUBDIR)$(HPS)md5.obj -+$(SUBDIR)$(HPS)memory.obj -+$(SUBDIR)$(HPS)metadata_iterators.obj -+$(SUBDIR)$(HPS)metadata_object.obj -+$(SUBDIR)$(HPS)ogg_decoder_aspect.obj -+$(SUBDIR)$(HPS)ogg_encoder_aspect.obj -+$(SUBDIR)$(HPS)ogg_helper.obj -+$(SUBDIR)$(HPS)ogg_mapping.obj -+$(SUBDIR)$(HPS)stream_decoder.obj -+$(SUBDIR)$(HPS)stream_encoder.obj -+$(SUBDIR)$(HPS)stream_encoder_framing.obj -+$(SUBDIR)$(HPS)window.obj
!endif

!ifdef EXT_FLAC_EXE
$(EXT_FLAC_EXE): $(EXT_LIBOGG_LIB) $(EXT_FLAC_LIB) $(SUBDIR)$(HPS)main.obj $(SUBDIR)$(HPS)analyze.obj $(SUBDIR)$(HPS)decode.obj $(SUBDIR)$(HPS)encode.obj $(SUBDIR)$(HPS)iffscan.obj $(SUBDIR)$(HPS)utils.obj $(SUBDIR)$(HPS)vorbiscomment.obj $(SUBDIR)$(HPS)foreign_metadata.obj $(SUBDIR)$(HPS)getopt.obj $(SUBDIR)$(HPS)getopt1.obj $(SUBDIR)$(HPS)local_string_utils.obj $(SUBDIR)$(HPS)cuesheet.obj $(SUBDIR)$(HPS)file.obj $(SUBDIR)$(HPS)picture.obj $(SUBDIR)$(HPS)replaygain.obj $(SUBDIR)$(HPS)seektable.obj $(SUBDIR)$(HPS)replaygain_synthesis.obj $(SUBDIR)$(HPS)replaygain_analysis.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) $(EXT_LIBOGG_LIB_WLINK_LIBRARIES) $(EXT_FLAC_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)main.obj file $(SUBDIR)$(HPS)analyze.obj file $(SUBDIR)$(HPS)decode.obj file $(SUBDIR)$(HPS)encode.obj file $(SUBDIR)$(HPS)iffscan.obj file $(SUBDIR)$(HPS)utils.obj file $(SUBDIR)$(HPS)vorbiscomment.obj file $(SUBDIR)$(HPS)foreign_metadata.obj file $(SUBDIR)$(HPS)getopt.obj file $(SUBDIR)$(HPS)getopt1.obj file $(SUBDIR)$(HPS)local_string_utils.obj file $(SUBDIR)$(HPS)cuesheet.obj file $(SUBDIR)$(HPS)file.obj file $(SUBDIR)$(HPS)picture.obj file $(SUBDIR)$(HPS)replaygain.obj file $(SUBDIR)$(HPS)seektable.obj file $(SUBDIR)$(HPS)replaygain_synthesis.obj file $(SUBDIR)$(HPS)replaygain_analysis.obj name $(EXT_FLAC_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

