# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../../../.."
NOW_BUILDING = DGJF2020

GAME_EXE =     $(SUBDIR)$(HPS)game.$(EXEEXT)

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe
       
lib: $(HW_VGA_LIB) .symbolic
	
exe: $(GAME_EXE) .symbolic

!ifdef GAME_EXE
$(GAME_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(FMT_MINIPNG_LIB) $(FMT_MINIPNG_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)game.obj
	%write tmp.cmd option quiet option map=$(GAME_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(FMT_MINIPNG_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)game.obj name $(GAME_EXE)
	@wlink @tmp.cmd
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA_LIB)
          del tmp.cmd
          @echo Cleaning done

