#!/bin/bash
linux-host/bmp2arr -i gaiji.bmp -o gaiji.test.c   -of c   -sw 16 -sh 16 -sym 'sGAIJI'
linux-host/bmp2arr -i gaiji.bmp -o gaiji.test.asm -of asm -sw 16 -sh 16 -sym 'sGAIJI'
linux-host/bmp2arr -i gaiji.bmp -o gaiji.test.bin -of bin -sw 16 -sh 16 -sym 'sGAIJI'
linux-host/bmp2arr -i gaiji.bmp -o gaiji.test.bmp -of bmp -sw 16 -sh 16 -sym 'sGAIJI'

linux-host/bmp2arr -i piano_l.bmp -o piano_l.test.c   -of c   -sw 8 -sh 8 -sym 'sPIANO_LABEL'
linux-host/bmp2arr -i piano_l.bmp -o piano_l.test.asm -of asm -sw 8 -sh 8 -sym 'sPIANO_LABEL'
linux-host/bmp2arr -i piano_l.bmp -o piano_l.test.bin -of bin -sw 8 -sh 8 -sym 'sPIANO_LABEL'
linux-host/bmp2arr -i piano_l.bmp -o piano_l.test.bmp -of bmp -sw 8 -sh 8 -sym 'sPIANO_LABEL'

linux-host/bmp2arr -i score.bmp -o score.test.c   -of c   -sw 8 -sh 8 -sym 'sSCORE' -u
linux-host/bmp2arr -i score.bmp -o score.test.asm -of asm -sw 8 -sh 8 -sym 'sSCORE' -u
linux-host/bmp2arr -i score.bmp -o score.test.bin -of bin -sw 8 -sh 8 -sym 'sSCORE' -u
linux-host/bmp2arr -i score.bmp -o score.test.bmp -of bmp -sw 8 -sh 8 -sym 'sSCORE' -u

linux-host/bmp2arr -i pointnum.bmp -o pointnum.test.c   -of c   -sw 8 -sh 8 -sym 'sPOINTNUMS'
linux-host/bmp2arr -i pointnum.bmp -o pointnum.test.asm -of asm -sw 8 -sh 8 -sym 'sPOINTNUMS'
linux-host/bmp2arr -i pointnum.bmp -o pointnum.test.bin -of bin -sw 8 -sh 8 -sym 'sPOINTNUMS'
linux-host/bmp2arr -i pointnum.bmp -o pointnum.test.bmp -of bmp -sw 8 -sh 8 -sym 'sPOINTNUMS'

