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
CFLAGS +=       -DUSE_DOSLIB=1
! ifdef PC98
# NEC PC-9821
IFICT_EXE =     ifictd98.$(EXEEXT)
! else
# MS-DOS IBM PC/AT
IFICT_EXE =     ifictdos.$(EXEEXT)
! endif
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
$(IFICT_EXE): $(HW_DOSBOXID_LIB) $(HW_DOSBOXID_LIB_DEPENDENCIES) $(HW_8251_LIB) $(HW_8251_LIB_DEPENDENCIES) $(HW_8042_LIB) $(HW_8042_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_VESA_LIB) $(HW_VESA_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(FMT_MINIPNG_LIB) $(FMT_MINIPNG_LIB_DEPENDENCIES) $(COMMON_LIB) $(SUBDIR)$(HPS)ifict.obj $(SUBDIR)$(HPS)utils.obj $(SUBDIR)$(HPS)debug.obj $(SUBDIR)$(HPS)palette.obj $(SUBDIR)$(HPS)fatal.obj $(SUBDIR)$(HPS)t_sdl2.obj $(SUBDIR)$(HPS)t_win32.obj $(SUBDIR)$(HPS)t_doslib.obj
	%write tmp.cmd option quiet option map=$(IFICT_EXE).map system $(WLINK_SYSTEM) $(HW_8251_LIB_WLINK_LIBRARIES) $(HW_DOSBOXID_LIB_WLINK_LIBRARIES) $(HW_8042_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_VESA_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(FMT_MINIPNG_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)ifict.obj file $(SUBDIR)$(HPS)utils.obj file $(SUBDIR)$(HPS)debug.obj file $(SUBDIR)$(HPS)palette.obj file $(SUBDIR)$(HPS)fatal.obj file $(SUBDIR)$(HPS)t_sdl2.obj file $(SUBDIR)$(HPS)t_win32.obj file $(SUBDIR)$(HPS)t_doslib.obj
	%write tmp.cmd name $(IFICT_EXE)
	@wlink @tmp.cmd
! ifdef TARGET_WINDOWS
! else
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)dos32a.dat dos4gw.exe
! endif
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA_LIB)
          del tmp.cmd
          @echo Cleaning done

