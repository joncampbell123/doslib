# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = EXT_VORBIS_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.." -dHAVE_CONFIG_H

OBJS = $(SUBDIR)$(HPS)analysis.obj $(SUBDIR)$(HPS)barkmel.obj $(SUBDIR)$(HPS)bitrate.obj $(SUBDIR)$(HPS)block.obj $(SUBDIR)$(HPS)codebook.obj $(SUBDIR)$(HPS)envelope.obj $(SUBDIR)$(HPS)floor0.obj $(SUBDIR)$(HPS)floor1.obj $(SUBDIR)$(HPS)info.obj $(SUBDIR)$(HPS)lookup.obj $(SUBDIR)$(HPS)lpc.obj $(SUBDIR)$(HPS)lsp.obj $(SUBDIR)$(HPS)mapping0.obj $(SUBDIR)$(HPS)mdct.obj $(SUBDIR)$(HPS)psy.obj $(SUBDIR)$(HPS)registry.obj $(SUBDIR)$(HPS)res0.obj $(SUBDIR)$(HPS)sharedbook.obj $(SUBDIR)$(HPS)smallft.obj $(SUBDIR)$(HPS)synthesis.obj $(SUBDIR)$(HPS)vorbisenc.obj $(SUBDIR)$(HPS)vorbisfile.obj $(SUBDIR)$(HPS)window.obj

!ifdef EXT_VORBIS_LIB
$(EXT_VORBIS_LIB): $(OBJS)
	wlib -q -b -c $(EXT_VORBIS_LIB) -+$(SUBDIR)$(HPS)analysis.obj -+$(SUBDIR)$(HPS)barkmel.obj
	wlib -q -b -c $(EXT_VORBIS_LIB) -+$(SUBDIR)$(HPS)bitrate.obj -+$(SUBDIR)$(HPS)block.obj
	wlib -q -b -c $(EXT_VORBIS_LIB) -+$(SUBDIR)$(HPS)codebook.obj -+$(SUBDIR)$(HPS)envelope.obj
	wlib -q -b -c $(EXT_VORBIS_LIB) -+$(SUBDIR)$(HPS)floor0.obj -+$(SUBDIR)$(HPS)floor1.obj
	wlib -q -b -c $(EXT_VORBIS_LIB) -+$(SUBDIR)$(HPS)info.obj -+$(SUBDIR)$(HPS)lookup.obj
	wlib -q -b -c $(EXT_VORBIS_LIB) -+$(SUBDIR)$(HPS)lpc.obj -+$(SUBDIR)$(HPS)lsp.obj
	wlib -q -b -c $(EXT_VORBIS_LIB) -+$(SUBDIR)$(HPS)mapping0.obj -+$(SUBDIR)$(HPS)mdct.obj
	wlib -q -b -c $(EXT_VORBIS_LIB) -+$(SUBDIR)$(HPS)psy.obj -+$(SUBDIR)$(HPS)synthesis.obj
	wlib -q -b -c $(EXT_VORBIS_LIB) -+$(SUBDIR)$(HPS)registry.obj -+$(SUBDIR)$(HPS)res0.obj
	wlib -q -b -c $(EXT_VORBIS_LIB) -+$(SUBDIR)$(HPS)sharedbook.obj -+$(SUBDIR)$(HPS)smallft.obj
	wlib -q -b -c $(EXT_VORBIS_LIB) -+$(SUBDIR)$(HPS)vorbisenc.obj -+$(SUBDIR)$(HPS)vorbisfile.obj
	wlib -q -b -c $(EXT_VORBIS_LIB) -+$(SUBDIR)$(HPS)window.obj
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

all: lib exe .symbolic
       
lib: $(EXT_VORBIS_LIB) .symbolic

exe: .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

