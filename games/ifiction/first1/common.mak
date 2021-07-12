# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../../.."
NOW_BUILDING = IFICTION

!ifdef TARGET_WINDOWS
# Windows 3.1 with Win32s or Windows 95
IFICT_EXE =     ifictw31.$(EXEEXT)
CFLAGS +=       -DUSE_WIN32=1
!else
# MS-DOS
IFICT_EXE =     ifictdos.$(EXEEXT)
CFLAGS +=       -DUSE_DOSLIB=1
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	$(CC) @tmp.cmd

.CPP.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	$(CXX) @tmp.cmd


all: $(OMFSEGDG) lib exe
       
lib: .symbolic

exe: $(IFICT_EXE) .symbolic

!ifdef IFICT_EXE
$(IFICT_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(FMT_MINIPNG_LIB) $(FMT_MINIPNG_LIB_DEPENDENCIES) $(COMMON_LIB) $(SUBDIR)$(HPS)ifict.obj
	%write tmp.cmd option quiet option map=$(IFICT_EXE).map system $(WLINK_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(FMT_MINIPNG_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)ifict.obj name $(IFICT_EXE)
	@wlink @tmp.cmd
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA_LIB)
          del tmp.cmd
          @echo Cleaning done

