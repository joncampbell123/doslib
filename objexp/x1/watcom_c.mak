
OBJS =     cmm.obj

CC       = wcc
CFLAGS   = -zq -ms -s -bt=dos -oilrtm -fr=nul -wx -0 -fo=.obj -q -zu -zdf -zff -zgf -zc -zl -fpi87

all: final.exe

header.obj: header.asm
	nasm -o $@ -f obj $(NASMFLAGS) $[@

.C.OBJ:
	$(CC) $(CFLAGS) $[@

final.exe: cmm.obj header.obj
	%write tmp.cmd option quiet system dos
	%write tmp.cmd file header.obj
	%write tmp.cmd file cmm.obj
	%write tmp.cmd name final.exe
	%write tmp.cmd option map=final.map
	@wlink @tmp.cmd

clean: .SYMBOLIC
          del *.obj
          @echo Cleaning done
