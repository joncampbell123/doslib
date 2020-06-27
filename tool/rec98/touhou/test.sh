#!/bin/bash

# ref: [https://github.com/nmlgc/ReC98/issues/8#event-3487565621]

# th01 pellet
linux-host/bmp2arr -i pellet.bmp -o pellet.test.c   -of c   -sw 8 -sh 8 -sym 'sPELLET' -pshf inner
linux-host/bmp2arr -i pellet.bmp -o pellet.test.asm -of asm -sw 8 -sh 8 -sym 'sPELLET' -pshf inner
linux-host/bmp2arr -i pellet.bmp -o pellet.test.bin -of bin -sw 8 -sh 8 -sym 'sPELLET' -pshf inner
linux-host/bmp2arr -i pellet.bmp -o pellet.test.bmp -of bmp -sw 8 -sh 8 -sym 'sPELLET' -pshf inner

# th02/th04/th05 pellet
linux-host/bmp2arr -i pellet2.bmp -o pellet2.test.c   -of c   -sw 8 -sh 8 -sym 'sPELLET' -pshf inner
linux-host/bmp2arr -i pellet2.bmp -o pellet2.test.asm -of asm -sw 8 -sh 8 -sym 'sPELLET' -pshf inner
linux-host/bmp2arr -i pellet2.bmp -o pellet2.test.bin -of bin -sw 8 -sh 8 -sym 'sPELLET' -pshf inner
linux-host/bmp2arr -i pellet2.bmp -o pellet2.test.bmp -of bmp -sw 8 -sh 8 -sym 'sPELLET' -pshf inner

# th02/th04/th05 sparks
linux-host/bmp2arr -i sparks.bmp -o sparks.test.c   -of c   -sw 8 -sh 8 -sym 'sSPARKS' -pshf outer
linux-host/bmp2arr -i sparks.bmp -o sparks.test.asm -of asm -sw 8 -sh 8 -sym 'sSPARKS' -pshf outer
linux-host/bmp2arr -i sparks.bmp -o sparks.test.bin -of bin -sw 8 -sh 8 -sym 'sSPARKS' -pshf outer
linux-host/bmp2arr -i sparks.bmp -o sparks.test.bmp -of bmp -sw 8 -sh 8 -sym 'sSPARKS' -pshf outer

# th02 pointnum
linux-host/bmp2arr -i pointnum.bmp -o pointnum.test.c   -of c   -sw 8 -sh 8 -sym 'sPOINTNUMS'
linux-host/bmp2arr -i pointnum.bmp -o pointnum.test.asm -of asm -sw 8 -sh 8 -sym 'sPOINTNUMS'
linux-host/bmp2arr -i pointnum.bmp -o pointnum.test.bin -of bin -sw 8 -sh 8 -sym 'sPOINTNUMS'
linux-host/bmp2arr -i pointnum.bmp -o pointnum.test.bmp -of bmp -sw 8 -sh 8 -sym 'sPOINTNUMS'

# th03 score
linux-host/bmp2arr -i score.bmp -o score.test.c   -of c   -sw 8 -sh 8 -sym 'sSCORE' -u
linux-host/bmp2arr -i score.bmp -o score.test.asm -of asm -sw 8 -sh 8 -sym 'sSCORE' -u
linux-host/bmp2arr -i score.bmp -o score.test.bin -of bin -sw 8 -sh 8 -sym 'sSCORE' -u
linux-host/bmp2arr -i score.bmp -o score.test.bmp -of bmp -sw 8 -sh 8 -sym 'sSCORE' -u

# th04/th05 pellet bottom
linux-host/bmp2arr -i pelletbt.bmp -o pelletbt.test.c   -of c   -sw 8 -sh 4 -sym 'sPELLET_BOTTOM' -pshf inner
linux-host/bmp2arr -i pelletbt.bmp -o pelletbt.test.asm -of asm -sw 8 -sh 4 -sym 'sPELLET_BOTTOM' -pshf inner
linux-host/bmp2arr -i pelletbt.bmp -o pelletbt.test.bin -of bin -sw 8 -sh 4 -sym 'sPELLET_BOTTOM' -pshf inner
linux-host/bmp2arr -i pelletbt.bmp -o pelletbt.test.bmp -of bmp -sw 8 -sh 4 -sym 'sPELLET_BOTTOM' -pshf inner

# th04/th05 pointnum
linux-host/bmp2arr -i pointnum4.bmp -o pointnum4.test.c   -of c   -sw 8 -sh 8 -sym 'sPOINTNUMS' -pshf inner
linux-host/bmp2arr -i pointnum4.bmp -o pointnum4.test.asm -of asm -sw 8 -sh 8 -sym 'sPOINTNUMS' -pshf inner
linux-host/bmp2arr -i pointnum4.bmp -o pointnum4.test.bin -of bin -sw 8 -sh 8 -sym 'sPOINTNUMS' -pshf inner
linux-host/bmp2arr -i pointnum4.bmp -o pointnum4.test.bmp -of bmp -sw 8 -sh 8 -sym 'sPOINTNUMS' -pshf inner

# th05 gaiji
linux-host/bmp2arr -i gaiji.bmp -o gaiji.test.c   -of c   -sw 16 -sh 16 -sym 'sGAIJI'
linux-host/bmp2arr -i gaiji.bmp -o gaiji.test.asm -of asm -sw 16 -sh 16 -sym 'sGAIJI'
linux-host/bmp2arr -i gaiji.bmp -o gaiji.test.bin -of bin -sw 16 -sh 16 -sym 'sGAIJI'
linux-host/bmp2arr -i gaiji.bmp -o gaiji.test.bmp -of bmp -sw 16 -sh 16 -sym 'sGAIJI'

# th05 piano label
linux-host/bmp2arr -i piano_l.bmp -o piano_l.test.c   -of c   -sw 8 -sh 8 -sym 'sPIANO_LABEL'
linux-host/bmp2arr -i piano_l.bmp -o piano_l.test.asm -of asm -sw 8 -sh 8 -sym 'sPIANO_LABEL'
linux-host/bmp2arr -i piano_l.bmp -o piano_l.test.bin -of bin -sw 8 -sh 8 -sym 'sPIANO_LABEL'
linux-host/bmp2arr -i piano_l.bmp -o piano_l.test.bmp -of bmp -sw 8 -sh 8 -sym 'sPIANO_LABEL'

