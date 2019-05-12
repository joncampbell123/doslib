# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = EXT_BZIP2_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.." -dHAVE_CONFIG_H

OBJS = &
    $(PRFX)blocksrt.obj &
    $(PRFX)bzlib.obj &
    $(PRFX)compress.obj &
    $(PRFX)crctable.obj &
    $(PRFX)decomprs.obj &
    $(PRFX)huffman.obj &
    $(PRFX)randtabl.obj

all: lib exe .symbolic

!ifndef EXT_BZIP2_LIB_NO_EXE
$(EXT_BZIP2_LIB_BZIP2_EXE): $(EXT_BZIP2_LIB) $(SUBDIR)$(HPS)bzip2.obj
	*wlink option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)bzip2.obj library $(EXT_BZIP2_LIB) name $(EXT_BZIP2_LIB_BZIP2_EXE)
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(EXT_BZIP2_LIB_BZIP2REC_EXE): $(EXT_BZIP2_LIB) $(SUBDIR)$(HPS)bzip2rec.obj
	*wlink option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)bzip2rec.obj library $(EXT_BZIP2_LIB) name $(EXT_BZIP2_LIB_BZIP2REC_EXE)
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifndef EXT_BZIP2_LIB_NO_LIB
PRFX = $(SUBDIR)$(HPS)
LIB_OBJS = $+$(OBJS)$-
PRFX = -+$(SUBDIR)$(HPS)
LIB_CMDS = $+$(OBJS)$-

$(EXT_BZIP2_LIB): $(LIB_OBJS)
	*wlib -q -b -c $(EXT_BZIP2_LIB) $(LIB_CMDS)
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	$(CC) $(CFLAGS_THIS) $(CFLAGS) $[@

lib: $(EXT_BZIP2_LIB) .symbolic

exe: $(EXT_BZIP2_LIB_BZIP2_EXE) $(EXT_BZIP2_LIB_BZIP2REC_EXE) .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

