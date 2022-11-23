
NOW_BUILDING = GAMES_GAME

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."

#DEBUG
#CFLAGS_THIS += -DDBG

!ifeq TARGET_MSDOS 16
GAME_EXE =    $(SUBDIR)$(HPS)game.$(EXEEXT)
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	@$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) exe

exe: $(GAME_EXE) .symbolic

!ifdef GAME_EXE
GAME_EXE_DEPS = $(SUBDIR)$(HPS)game.obj

GAME_EXE_WLINK = file $(SUBDIR)$(HPS)game.obj

# MS-DOS target
GAME_EXE_DEPS += $(WINDOWS_W9XVMM_LIB) $(WINDOWS_W9XVMM_LIB_DEPENDENCIES) $(HW_SNDSB_LIB) $(HW_SNDSB_LIB_DEPENDENCIES) $(HW_8237_LIB) $(HW_8237_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_VGATTY_LIB) $(HW_VGATTY_LIB_DEPENDENCIES)

GAME_EXE_WLINK += $(WINDOWS_W9XVMM_LIB_WLINK_LIBRARIES) $(HW_SNDSB_LIB_WLINK_LIBRARIES) $(HW_8237_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_VGATTY_LIB_WLINK_LIBRARIES)
!endif

!ifdef GAME_EXE
$(GAME_EXE): $(GAME_EXE_DEPS)
	%write tmp.cmd
# MS-DOS
! ifeq TARGET_MSDOS 16 # Sound Blaster library needs more stack than default
	%append tmp.cmd option stack=4096
! endif
	%append tmp.cmd option quiet option map=$(GAME_EXE).map system $(WLINK_CON_SYSTEM) $(GAME_EXE_WLINK)
	%append tmp.cmd name $(GAME_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

