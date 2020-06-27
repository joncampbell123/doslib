#!/bin/bash
linux-host/bmp2arr -i gaiji.bmp -o gaiji.test.c   -of c   -sw 16 -sh 16 -sym 'sGAIJI'
linux-host/bmp2arr -i gaiji.bmp -o gaiji.test.asm -of asm -sw 16 -sh 16 -sym 'sGAIJI'
linux-host/bmp2arr -i gaiji.bmp -o gaiji.test.bin -of bin -sw 16 -sh 16 -sym 'sGAIJI'
linux-host/bmp2arr -i gaiji.bmp -o gaiji.test.bmp -of bmp -sw 16 -sh 16 -sym 'sGAIJI'

