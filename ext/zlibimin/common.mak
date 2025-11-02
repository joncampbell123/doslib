# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = EXT_ZLIBIMIN_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.." -dHAVE_CONFIG_H -dSTDC

OBJS = $(SUBDIR)$(HPS)adler32.obj $(SUBDIR)$(HPS)crc32.obj $(SUBDIR)$(HPS)inffast.obj $(SUBDIR)$(HPS)inflate.obj $(SUBDIR)$(HPS)inftrees.obj $(SUBDIR)$(HPS)trees.obj $(SUBDIR)$(HPS)zutil.obj

!ifndef EXT_ZLIBIMIN_LIB_NO_LIB
$(EXT_ZLIBIMIN_LIB): $(OBJS)
	wlib -q -b -c $(EXT_ZLIBIMIN_LIB) -+$(SUBDIR)$(HPS)adler32.obj -+$(SUBDIR)$(HPS)crc32.obj -+$(SUBDIR)$(HPS)inffast.obj -+$(SUBDIR)$(HPS)inflate.obj -+$(SUBDIR)$(HPS)inftrees.obj -+$(SUBDIR)$(HPS)trees.obj -+$(SUBDIR)$(HPS)zutil.obj
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

all: lib exe .symbolic
       
lib: $(EXT_ZLIBIMIN_LIB) .symbolic

exe: .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

