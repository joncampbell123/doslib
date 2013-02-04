# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_8237_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i=.. -i..$(HPS)..

C_SOURCE =    8237.c
OBJS =        $(SUBDIR)$(HPS)8237.obj

$(HW_8237_LIB): $(OBJS)
	wlib -q -b -c $(HW_8237_LIB) -+$(SUBDIR)$(HPS)8237.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	$(CC) @tmp.cmd

all: lib exe
	
lib: $(HW_8237_LIB) .symbolic

exe: .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_8237_LIB)
          del tmp.cmd
          @echo Cleaning done

