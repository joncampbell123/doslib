# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."
NOW_BUILDING = HW_VGA_LIB

OBJS =        $(SUBDIR)$(HPS)libbmp.obj

LIBBMP_LIB =  $(SUBDIR)$(HPS)libbmp.lib

$(LIBBMP_LIB): $(OBJS)
	wlib -q -b -c $(LIBBMP_LIB) -+$(SUBDIR)$(HPS)libbmp.obj

!ifndef PC98
! ifndef TARGET_WINDOWS
320X200A_EXE =     $(SUBDIR)$(HPS)320x200a.$(EXEEXT)
! endif
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe
       
lib: $(LIBBMP_LIB) .symbolic
	
exe: $(320X200A_EXE) .symbolic

!ifdef 320X200A_EXE
$(320X200A_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)320x200a.obj
	%write tmp.cmd option quiet option map=$(320X200A_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)320x200a.obj name $(320X200A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w320$(HPS)200vga.bmp $(SUBDIR)$(HPS)200l8v.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA_LIB)
          del tmp.cmd
          @echo Cleaning done

