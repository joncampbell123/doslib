
HERE = $+$(%cwd)$-

OBJS =     drvc.obj

CC       = wcc
CFLAGS   = -zq -ml -s -bt=dos -oilrtm -fr=nul -wx -0 -fo=.obj -q -zu -zdf -zff -zgf -zc -fpi87 -i../.. -dTARGET_MSDOS=16 -dMSDOS=1

all: drv.exe

drva.obj: drva.asm
	nasm -o $@ -f obj $(NASMFLAGS) $[@

.C.OBJ:
	$(CC) $(CFLAGS) $[@

../../hw/dos/dos86l/dos.lib:
	@cd ../../hw/dos
	@./make.sh build lib
	@cd $(HERE)

drvc.obj: drvc.c
	$(CC) $(CFLAGS) -ms -zl $[@

drv.exe: drvc.obj drva.obj
	%write tmp.cmd option quiet system dos
	%write tmp.cmd file drva.obj
	%write tmp.cmd file drvc.obj
	%write tmp.cmd name drv.exe
	%write tmp.cmd option map=drv.map
	# special segment ordering
	%write tmp.cmd order clname CODE clname DATA segment CONST segment CONST2 segment _DATA segment _END
	# end segment ordering
	@wlink @tmp.cmd

clean: .SYMBOLIC
          del *.obj
          @echo Cleaning done
