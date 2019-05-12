
HERE = $+$(%cwd)$-

OBJS =     drvc.obj

CC       = *wcc
CFLAGS   = -zq -ms -s -bt=dos -oilrsm -fr=nul -wx -0 -fo=dos86s/.obj -zl -q -zu -zdp -zff -zgf -zc -fpi87 -i"../.." -dTARGET_MSDOS=16 -dMSDOS=1 -dNEAR_DRVVAR

all: dos86s dos86s/drv.exe

dos86s:
        mkdir -p dos86s

drva.asm:
        ../../hw/dos/devhdgen.pl --asm $@ --name "hello$$" --type c --c-openclose --c-out-busy --no-stack

dos86s/drva.obj: drva.asm
        nasm -o $@ -f obj $(NASMFLAGS) $[@

.C.OBJ:
        $(CC) $(CFLAGS) $[@

../../hw/dos/dos86l/dos.lib:
        @cd ../../hw/dos
        @./make.sh build lib
        @cd $(HERE)

dos86s/drvc.obj: drvc.c
        $(CC) $(CFLAGS) $[@

dos86s/drvci.obj: drvci.c
        $(CC) $(CFLAGS) -nt=_INITTEXT -nd=_INIT -nc=INITCODE -g=_INIT_GROUP $[@

drv_objs = &
    dos86s/drva.obj &
    dos86s/drvc.obj &
    dos86s/drvci.obj

# special segment ordering
link_segm_order = order clname CODE clname DATA segment CONST segment CONST2 segment _DATA clname BSS clname END clname INITBEGIN clname INITCODE clname INITEND
# end segment ordering

dos86s/drv.exe: $(drv_objs)
        *wlink option quiet system dos file {$(drv_objs)} name $@ option map=dos86s/drv.map $(link_segm_order)

clean: .SYMBOLIC
          del *.obj
          @echo Cleaning done
