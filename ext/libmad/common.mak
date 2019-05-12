# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = EXT_LIBMAD_LIB
CFLAGS_THIS = -fr=nul -fo=$@ -i.. -i"../.." -dHAVE_CONFIG_H

OBJS = &
    $(PRFX)bit.obj &
    $(PRFX)decoder.obj &
    $(PRFX)fixed.obj &
    $(PRFX)frame.obj &
    $(PRFX)huffman.obj &
    $(PRFX)layer12.obj &
    $(PRFX)layer3.obj &
    $(PRFX)stream.obj &
    $(PRFX)synth.obj &
    $(PRFX)timer.obj &
    $(PRFX)version.obj

all: lib exe .symbolic
       
!ifdef EXT_LIBMAD_LIB
PRFX = $(SUBDIR)$(HPS)
LIB_OBJS = $+$(OBJS)$-
PRFX = -+$(SUBDIR)$(HPS)
LIB_CMDS = $+$(OBJS)$-

$(EXT_LIBMAD_LIB): $(LIB_OBJS)
        *wlib -q -b -c $(EXT_LIBMAD_LIB) $(LIB_CMDS)
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
        $(CC) $(CFLAGS_THIS) $(CFLAGS) $[@

lib: $(EXT_LIBMAD_LIB) .symbolic

exe: .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

