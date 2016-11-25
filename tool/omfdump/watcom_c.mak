
HERE = $+$(%cwd)$-

OBJS =     omfdump.obj

CC       = wcc
CFLAGS   = -zq -ms -s -bt=dos -oilrsm -fr=nul -wx -0 -fo=dos86s/.obj -q -zdp -zff -zgf -zc -fpi87 -i../.. -dTARGET_MSDOS=16 -dMSDOS=1

all: dos86s dos86s/omfdump.exe

dos86s:
	mkdir -p dos86s

.C.OBJ:
	$(CC) $(CFLAGS) $[@

dos86s/omfdump.obj: omfdump.c
	$(CC) $(CFLAGS) $[@

dos86s/omfdump.exe: dos86s/omfcstr.obj dos86s/omfrec.obj dos86s/omfrecs.obj dos86s/olnames.obj dos86s/ogrpdefs.obj dos86s/osegdefs.obj dos86s/osegdeft.obj dos86s/oextdefs.obj dos86s/oextdeft.obj dos86s/omfdump.obj
	%write tmp.cmd option quiet system dos
	%write tmp.cmd file dos86s/oextdeft.obj
	%write tmp.cmd file dos86s/oextdefs.obj
	%write tmp.cmd file dos86s/ogrpdefs.obj
	%write tmp.cmd file dos86s/osegdeft.obj
	%write tmp.cmd file dos86s/osegdefs.obj
	%write tmp.cmd file dos86s/olnames.obj
	%write tmp.cmd file dos86s/omfcstr.obj
	%write tmp.cmd file dos86s/omfrec.obj
	%write tmp.cmd file dos86s/omfrecs.obj
	%write tmp.cmd file dos86s/omfdump.obj
	%write tmp.cmd name dos86s/omfdump.exe
	%write tmp.cmd option map=dos86s/omfdump.map
	@wlink @tmp.cmd

clean: .SYMBOLIC
          del *.obj
          @echo Cleaning done
