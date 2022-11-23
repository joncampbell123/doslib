# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = EXT_LIBMAD_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.." -dHAVE_CONFIG_H

OBJS = $(SUBDIR)$(HPS)bit.obj $(SUBDIR)$(HPS)decoder.obj $(SUBDIR)$(HPS)fixed.obj $(SUBDIR)$(HPS)frame.obj $(SUBDIR)$(HPS)huffman.obj $(SUBDIR)$(HPS)layer12.obj $(SUBDIR)$(HPS)layer3.obj $(SUBDIR)$(HPS)stream.obj $(SUBDIR)$(HPS)synth.obj $(SUBDIR)$(HPS)timer.obj $(SUBDIR)$(HPS)version.obj

!ifdef EXT_LIBMAD_LIB
$(EXT_LIBMAD_LIB): $(OBJS)
	wlib -q -b -c $(EXT_LIBMAD_LIB) -+$(SUBDIR)$(HPS)bit.obj -+$(SUBDIR)$(HPS)decoder.obj
	wlib -q -b -c $(EXT_LIBMAD_LIB) -+$(SUBDIR)$(HPS)fixed.obj -+$(SUBDIR)$(HPS)frame.obj 
	wlib -q -b -c $(EXT_LIBMAD_LIB) -+$(SUBDIR)$(HPS)huffman.obj -+$(SUBDIR)$(HPS)layer12.obj 
	wlib -q -b -c $(EXT_LIBMAD_LIB) -+$(SUBDIR)$(HPS)layer3.obj -+$(SUBDIR)$(HPS)stream.obj 
	wlib -q -b -c $(EXT_LIBMAD_LIB) -+$(SUBDIR)$(HPS)synth.obj -+$(SUBDIR)$(HPS)timer.obj 
	wlib -q -b -c $(EXT_LIBMAD_LIB) -+$(SUBDIR)$(HPS)version.obj
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

all: lib exe .symbolic
       
lib: $(EXT_LIBMAD_LIB) .symbolic

exe: .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

