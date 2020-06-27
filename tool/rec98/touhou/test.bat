@echo off

dos86l\bmp2arr -i gaiji.bmp -o gtest.c   -of c   -sw 16 -sh 16 -sym 'sGAIJI'
dos86l\bmp2arr -i gaiji.bmp -o gtest.asm -of asm -sw 16 -sh 16 -sym 'sGAIJI'
dos86l\bmp2arr -i gaiji.bmp -o gtest.bin -of bin -sw 16 -sh 16 -sym 'sGAIJI'
dos86l\bmp2arr -i gaiji.bmp -o gtest.bmp -of bmp -sw 16 -sh 16 -sym 'sGAIJI'

dos86l\bmp2arr -i piano_l.bmp -o pitest.c   -of c   -sw 8 -sh 8 -sym 'sPIANO_LABEL'
dos86l\bmp2arr -i piano_l.bmp -o pitest.asm -of asm -sw 8 -sh 8 -sym 'sPIANO_LABEL'
dos86l\bmp2arr -i piano_l.bmp -o pitest.bin -of bin -sw 8 -sh 8 -sym 'sPIANO_LABEL'
dos86l\bmp2arr -i piano_l.bmp -o pitest.bmp -of bmp -sw 8 -sh 8 -sym 'sPIANO_LABEL'

dos86l\bmp2arr -i score.bmp -o stest.c   -of c   -sw 8 -sh 8 -sym 'sSCORE' -u
dos86l\bmp2arr -i score.bmp -o stest.asm -of asm -sw 8 -sh 8 -sym 'sSCORE' -u
dos86l\bmp2arr -i score.bmp -o stest.bin -of bin -sw 8 -sh 8 -sym 'sSCORE' -u
dos86l\bmp2arr -i score.bmp -o stest.bmp -of bmp -sw 8 -sh 8 -sym 'sSCORE' -u

dos86l\bmp2arr -i pointnum.bmp -o pttest.c   -of c   -sw 8 -sh 8 -sym 'sPOINTNUMS'
dos86l\bmp2arr -i pointnum.bmp -o pttest.asm -of asm -sw 8 -sh 8 -sym 'sPOINTNUMS'
dos86l\bmp2arr -i pointnum.bmp -o pttest.bin -of bin -sw 8 -sh 8 -sym 'sPOINTNUMS'
dos86l\bmp2arr -i pointnum.bmp -o pttest.bmp -of bmp -sw 8 -sh 8 -sym 'sPOINTNUMS'

