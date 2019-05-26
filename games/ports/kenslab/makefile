LAB3D = lab3d.obj graphx.obj subs.obj init.obj
LAB3DOBJS = lab3d graphx subs init
                     
lab3d :  $(LAB3D)  makefile
      link  $(LAB3DOBJS) ;

lab3d.obj : lab3d.c lab3d.h
graphx.obj : graphx.c lab3d.h
subs.obj : subs.c lab3d.h 
init.obj : init.c lab3d.h

.c.obj:
		 cl /c /AM $*.c
