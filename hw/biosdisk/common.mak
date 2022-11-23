# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (BIOSDISK) or / (Linux)
NOW_BUILDING = HW_BIOSDISK_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."

C_SOURCE =    biosdisk.c
OBJS =        $(SUBDIR)$(HPS)biosdisk.obj
TEST_EXE =    $(SUBDIR)$(HPS)test.$(EXEEXT)
!ifeq TARGET_MSDOS 16
! ifndef TARGET_WINDOWS
!  ifndef TARGET_OS2
DUMPHDP_EXE = $(SUBDIR)$(HPS)dumphdp.$(EXEEXT)
!  endif
! endif
!endif

$(HW_BIOSDISK_LIB): $(OBJS)
	wlib -q -b -c $(HW_BIOSDISK_LIB) -+$(SUBDIR)$(HPS)biosdisk.obj

# NTS we have to construct the command line into tmp.cmd because for MS-BIOSDISK
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

.asm.obj:
	nasm -o $@ -f obj $(NASMFLAGS) $[@

all: $(OMFSEGDG) lib exe

lib: $(HW_BIOSDISK_LIB) .symbolic

exe: $(TEST_EXE) $(DUMPHDP_EXE) .symbolic

$(TEST_EXE): $(HW_BIOSDISK_LIB) $(HW_BIOSDISK_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(TEST_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj $(HW_BIOSDISK_LIB_WLINK_LIBRARIES) $(HW_BIOSDISK_CPU_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TEST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

!ifdef DUMPHDP_EXE
$(DUMPHDP_EXE): $(SUBDIR)$(HPS)dumphdp.obj $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(DUMPHDP_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)dumphdp.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(DUMPHDP_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_BIOSDISK_LIB)
          del tmp.cmd
          @echo Cleaning done

