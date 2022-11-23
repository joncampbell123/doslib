# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = EXT_BZIP2_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.." -dHAVE_CONFIG_H

OBJS = $(SUBDIR)$(HPS)blocksrt.obj $(SUBDIR)$(HPS)bzlib.obj $(SUBDIR)$(HPS)compress.obj $(SUBDIR)$(HPS)crctable.obj $(SUBDIR)$(HPS)decomprs.obj $(SUBDIR)$(HPS)huffman.obj $(SUBDIR)$(HPS)randtabl.obj

!ifndef EXT_BZIP2_LIB_NO_EXE
$(EXT_BZIP2_LIB_BZIP2_EXE): $(EXT_BZIP2_LIB) $(SUBDIR)$(HPS)bzip2.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)bzip2.obj library $(EXT_BZIP2_LIB) name $(EXT_BZIP2_LIB_BZIP2_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(EXT_BZIP2_LIB_BZIP2REC_EXE): $(EXT_BZIP2_LIB) $(SUBDIR)$(HPS)bzip2rec.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)bzip2rec.obj library $(EXT_BZIP2_LIB) name $(EXT_BZIP2_LIB_BZIP2REC_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifndef EXT_BZIP2_LIB_NO_LIB
$(EXT_BZIP2_LIB): $(OBJS)
	wlib -q -b -c $(EXT_BZIP2_LIB) -+$(SUBDIR)$(HPS)blocksrt.obj -+$(SUBDIR)$(HPS)bzlib.obj -+$(SUBDIR)$(HPS)compress.obj -+$(SUBDIR)$(HPS)crctable.obj -+$(SUBDIR)$(HPS)decomprs.obj -+$(SUBDIR)$(HPS)huffman.obj -+$(SUBDIR)$(HPS)randtabl.obj
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

all: lib exe .symbolic
       
lib: $(EXT_BZIP2_LIB) .symbolic

exe: $(EXT_BZIP2_LIB_BZIP2_EXE) $(EXT_BZIP2_LIB_BZIP2REC_EXE) .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

