
NOW_BUILDING = TOOL_REC98_TOUHOU_EXE

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../../.."

#DEBUG
#CFLAGS_THIS += -DDBG

BMP2ARR_EXE =    $(SUBDIR)$(HPS)bmp2arr.$(EXEEXT)

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	@$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) exe

exe: $(BMP2ARR_EXE) .symbolic

!ifdef BMP2ARR_EXE
BMP2ARR_EXE_DEPS = $(SUBDIR)$(HPS)bmp2arr.obj
BMP2ARR_EXE_WLINK = file $(SUBDIR)$(HPS)bmp2arr.obj
!endif

!ifdef BMP2ARR_EXE
$(BMP2ARR_EXE): $(BMP2ARR_EXE_DEPS)
	%write tmp.cmd
	%append tmp.cmd option quiet option map=$(BMP2ARR_EXE).map system $(WLINK_CON_SYSTEM) $(BMP2ARR_EXE_WLINK)
	%append tmp.cmd name $(BMP2ARR_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

