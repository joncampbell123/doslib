#!/bin/bash

if [ "$1" == "clean" ]; then
	rm -fv *.obj *.OBJ
	exit 0
fi

if [[ "$1" == "build" || "$1" == "" ]]; then
	nasm -o 0000.obj -f obj 0000.asm || exit 1
	nasm -o 0001.obj -f obj 0001.asm || exit 1
	nasm -o 0002.obj -f obj 0002.asm || exit 1
	nasm -o 0003.obj -f obj 0003.asm || exit 1
	nasm -o 0004.obj -f obj 0004.asm || exit 1
	nasm -o 0005.obj -f obj 0005.asm || exit 1
	nasm -o 0006.obj -f obj 0006.asm || exit 1
	nasm -o 0007.obj -f obj 0007.asm || exit 1
	nasm -o 0008.obj -f obj 0008.asm || exit 1
	nasm -o 0009.obj -f obj 0009.asm || exit 1
	./masm.sh 0010.asm 0010.obj || exit 1
	./masm.sh 0011.asm 0011.obj || exit 1
	./masm.sh 0012.asm 0012.obj || exit 1
	./masm.sh 0013.asm 0013.obj || exit 1
fi

