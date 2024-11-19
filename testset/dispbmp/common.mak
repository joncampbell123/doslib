# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."
NOW_BUILDING = LIBBMP_LIB

OBJS =        $(SUBDIR)$(HPS)libbmp.obj

LIBBMP_LIB =  $(SUBDIR)$(HPS)libbmp.lib

$(LIBBMP_LIB): $(OBJS)
	wlib -q -b -c $(LIBBMP_LIB) -+$(SUBDIR)$(HPS)libbmp.obj

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
	
exe: .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA_LIB)
          del tmp.cmd
          @echo Cleaning done

