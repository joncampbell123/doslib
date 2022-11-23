# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_ADLIB_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."

C_SOURCE =    adlib.c
OBJS =        $(SUBDIR)$(HPS)adlib.obj $(SUBDIR)$(HPS)adlib.obj
TEST_EXE =    $(SUBDIR)$(HPS)test.$(EXEEXT)
MIDI_EXE =    $(SUBDIR)$(HPS)midi.$(EXEEXT)
IMFPLAY_EXE = $(SUBDIR)$(HPS)imfplay.$(EXEEXT)
MIDI2IMF_EXE =$(SUBDIR)$(HPS)midi2imf.$(EXEEXT)

$(HW_ADLIB_LIB): $(OBJS)
	wlib -q -b -c $(HW_ADLIB_LIB) -+$(SUBDIR)$(HPS)adlib.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe
       
lib: $(HW_ADLIB_LIB) .symbolic

exe: $(TEST_EXE) $(MIDI_EXE) $(MIDI2IMF_EXE) $(IMFPLAY_EXE) .symbolic

!ifdef TEST_EXE
$(TEST_EXE): $(HW_ADLIB_LIB) $(HW_ADLIB_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_VGATTY_LIB) $(HW_VGATTY_LIB_DEPENDENCIES) $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(TEST_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj $(HW_ADLIB_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_VGATTY_LIB_WLINK_LIBRARIES) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TEST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

$(MIDI_EXE): $(HW_ADLIB_LIB) $(HW_ADLIB_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)midi.obj $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(MIDI_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)midi.obj $(HW_ADLIB_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(MIDI_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(MIDI2IMF_EXE): $(HW_ADLIB_LIB) $(HW_ADLIB_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)midi2imf.obj $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(MIDI2IMF_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)midi2imf.obj $(HW_ADLIB_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(MIDI2IMF_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(IMFPLAY_EXE): $(HW_ADLIB_LIB) $(HW_ADLIB_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)imfplay.obj $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(IMFPLAY_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)imfplay.obj $(HW_ADLIB_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(IMFPLAY_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_ADLIB_LIB)
          del tmp.cmd
          @echo Cleaning done

