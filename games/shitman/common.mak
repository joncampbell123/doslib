
# TODO: OS/2 target: What can we #define to tell the header files which OS/2
#       environment we're doing? (Command prompt app. vs Presentation Manager app vs.
#       "fullscreen" app.)

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i=.. -i..$(HPS)..
NOW_BUILDING = GAMES_SHITMAN

OBJS =        $(SUBDIR)$(HPS)shitman.obj

!ifndef TARGET_WINDOWS
! ifeq TARGET_MSDOS 16
!  ifeq MMODE l
SHITMAN_EXE = $(SUBDIR)$(HPS)shitman.$(EXEEXT)
!  endif
! endif
! ifeq TARGET_MSDOS 32
!  ifeq MMODE f
SHITMAN_EXE = $(SUBDIR)$(HPS)shitman.$(EXEEXT)
!  endif
! endif
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	@$(CC) @tmp.cmd

all: exe .symbolic

exe: $(SHITMAN_EXE) .symbolic

lib: .symbolic

!ifdef SHITMAN_EXE
$(SHITMAN_EXE): $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_8042_LIB) $(HW_8042_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)dgif_lib.obj $(SUBDIR)$(HPS)gifalloc.obj $(SUBDIR)$(HPS)gif_err.obj $(SUBDIR)$(HPS)openbsd-reallocarray.obj $(SUBDIR)$(HPS)shitman.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)dgif_lib.obj file $(SUBDIR)$(HPS)gifalloc.obj file $(SUBDIR)$(HPS)gif_err.obj file $(SUBDIR)$(HPS)openbsd-reallocarray.obj file $(SUBDIR)$(HPS)shitman.obj $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_8042_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_VGA_LIB_WLINK_LIBRARIES)
	%write tmp.cmd option map=$(SHITMAN_EXE).map
	%write tmp.cmd name $(SHITMAN_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(FMT_OMF_LIB)
		  del DEBUG.TXT
          del tmp.cmd
          @echo Cleaning done

