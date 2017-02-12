
# comment this definition out with a "#", if building a non-debugging version
#Debug=-DDEBUG

MASMOBJ=masm5 -p -w2 -Mx $(Debug) -I..\include 
MASMLST=masm5 -l -p -w2 -Mx $(Debug) -I..\include 

.asm.obj:
	$(MASMOBJ) $*;

.asm.lst:
	$(MASMLST) $*;

all:    dboxmpi.386

dboxvxd.obj dboxvxd.lst: dboxvxd.asm

OBJS = dboxvxd.obj

clean:
	del *.obj
	del *.sym
	del *.map

dboxmpi.386: dboxvxd.def $(OBJS)
        link386 @dboxvxd.lnk
        addhdr dboxmpi.386
        mapsym32 dboxvxd

