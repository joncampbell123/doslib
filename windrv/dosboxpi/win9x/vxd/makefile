# sample make file for building vmd virtual device for Windows/386

# comment this definition out with a "#", if building a non-debugging version
Debug=-DDEBUG

MASMOBJ=masm5 -p -w2 -Mx $(Debug) -I..\include 
MASMLST=masm5 -l -p -w2 -Mx $(Debug) -I..\include 

.asm.obj:
        $(MASMOBJ) $*;

.asm.lst:
        $(MASMLST) $*;

all:    vmd.386

int33.obj int33.lst: int33.asm

vmd.obj vmd.lst: vmd.asm

vmdmsg.obj vmdmsg.lst: vmdmsg.asm

realmode.obj realmode.lst: realmode.asm

OBJS =  int33.obj vmd.obj vmdmsg.obj realmode.obj

clean:
	del vmd.386
	del *.obj
	del *.sym

vmd.386: vmd.def $(OBJS)
        link386 @vmd.lnk
        addhdr vmd.386
        mapsym32 vmd

