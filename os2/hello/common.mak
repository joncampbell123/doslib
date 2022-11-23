# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = OS2_HELLO_EXE
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."

RCFLAGS_THIS = -i.. -i"../.."

HELLO_EXE =  $(SUBDIR)$(HPS)hello.exe
HELLOPM_EXE =  $(SUBDIR)$(HPS)hellopm.exe

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	$(CC) @tmp.cmd

$(SUBDIR)$(HPS)hellopm.obj: hellopm.c
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	$(CC) @tmp.cmd

all: lib exe

lib: .symbolic

exe: $(HELLO_EXE) $(HELLOPM_EXE) .symbolic

$(HELLO_EXE): $(SUBDIR)$(HPS)hello.obj
	%write tmp.cmd op m option quiet system $(WLINK_CON_SYSTEM) $(WLINK_CON_FLAGS) file $(SUBDIR)$(HPS)hello.obj
	%write tmp.cmd name $(HELLO_EXE)
	wlink @tmp.cmd

$(HELLOPM_EXE): $(SUBDIR)$(HPS)hellopm.obj
	%write tmp.cmd op m option quiet system $(WLINK_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)hellopm.obj
	%write tmp.cmd name $(HELLOPM_EXE)
	wlink @tmp.cmd

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
	  del $(SUBDIR)$(HPS)*.res
          del tmp.cmd
          @echo Cleaning done

