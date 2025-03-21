#!/bin/bash

if [ "$1" == "clean" ]; then
	rm -fv *.obj *.OBJ
	exit 0
fi

if [[ "$1" == "build" || "$1" == "" ]]; then
	if [[ 0000.asm -nt 0000.obj ]] ; then nasm -o 0000.obj -f obj 0000.asm || exit 1; fi
	if [[ 0001.asm -nt 0001.obj ]] ; then nasm -o 0001.obj -f obj 0001.asm || exit 1; fi
	if [[ 0002.asm -nt 0002.obj ]] ; then nasm -o 0002.obj -f obj 0002.asm || exit 1; fi
	if [[ 0003.asm -nt 0003.obj ]] ; then nasm -o 0003.obj -f obj 0003.asm || exit 1; fi
	if [[ 0004.asm -nt 0004.obj ]] ; then nasm -o 0004.obj -f obj 0004.asm || exit 1; fi
	if [[ 0005.asm -nt 0005.obj ]] ; then nasm -o 0005.obj -f obj 0005.asm || exit 1; fi
	if [[ 0006.asm -nt 0006.obj ]] ; then nasm -o 0006.obj -f obj 0006.asm || exit 1; fi
	if [[ 0007.asm -nt 0007.obj ]] ; then nasm -o 0007.obj -f obj 0007.asm || exit 1; fi
	if [[ 0008.asm -nt 0008.obj ]] ; then nasm -o 0008.obj -f obj 0008.asm || exit 1; fi
	if [[ 0009.asm -nt 0009.obj ]] ; then nasm -o 0009.obj -f obj 0009.asm || exit 1; fi
	if [[ 0010.asm -nt 0010.obj ]] ; then ./masm.sh 0010.asm 0010.obj || exit 1; fi
	if [[ 0011.asm -nt 0011.obj ]] ; then ./masm.sh 0011.asm 0011.obj || exit 1; fi
	if [[ 0012.asm -nt 0012.obj ]] ; then ./masm.sh 0012.asm 0012.obj || exit 1; fi
	if [[ 0013.asm -nt 0013.obj ]] ; then ./masm.sh 0013.asm 0013.obj || exit 1; fi
	if [[ 0014.asm -nt 0014.obj ]] ; then ./masm.sh 0014.asm 0014.obj || exit 1; fi
fi

