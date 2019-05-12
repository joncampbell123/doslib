# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = EXT_VORBIS_LIB
CFLAGS_THIS = -fr=nul -fo=$@ -i.. -i"../.." -dHAVE_CONFIG_H

OBJS = &
    $(PRFX)analysis.obj &
    $(PRFX)barkmel.obj &
    $(PRFX)bitrate.obj &
    $(PRFX)block.obj &
    $(PRFX)codebook.obj &
    $(PRFX)envelope.obj &
    $(PRFX)floor0.obj &
    $(PRFX)floor1.obj &
    $(PRFX)info.obj &
    $(PRFX)lookup.obj &
    $(PRFX)lpc.obj &
    $(PRFX)lsp.obj &
    $(PRFX)mapping0.obj &
    $(PRFX)mdct.obj &
    $(PRFX)psy.obj &
    $(PRFX)registry.obj &
    $(PRFX)res0.obj &
    $(PRFX)sharedbook.obj &
    $(PRFX)smallft.obj &
    $(PRFX)synthesis.obj &
    $(PRFX)vorbisenc.obj &
    $(PRFX)vorbisfile.obj &
    $(PRFX)window.obj

all: lib exe .symbolic
       
!ifdef EXT_VORBIS_LIB
PRFX = $(SUBDIR)$(HPS)
LIB_OBJS = $+$(OBJS)$-
PRFX = -+$(SUBDIR)$(HPS)
LIB_CMDS = $+$(OBJS)$-

$(EXT_VORBIS_LIB): $(OBJS)
        *wlib -q -b -c $(EXT_VORBIS_LIB) $(LIB_CMDS)
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
        $(CC) $(CFLAGS_THIS) $(CFLAGS) $[@

lib: $(EXT_VORBIS_LIB) .symbolic

exe: .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

