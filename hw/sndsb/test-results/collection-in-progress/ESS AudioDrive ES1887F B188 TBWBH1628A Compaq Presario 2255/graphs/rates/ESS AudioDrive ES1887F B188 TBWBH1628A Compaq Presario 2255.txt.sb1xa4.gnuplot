reset
set term png size 1920,1080
set output 'gnuplot/ESS AudioDrive ES1887F B188 TBWBH1628A Compaq Presario 2255.txt.sb1xa4.png'
set grid
set autoscale
set xrange [0:255]
set title 'Sound Blaster Time Constant and Playback rate 4-bit ADPCM (ESS AudioDrive ES1887F B188 TBWBH1628A Compaq Presario 2255)'
set xlabel 'Sample rate divider byte'
set ylabel 'Sample rate (Hz)'
plot 'gnuplot/ESS AudioDrive ES1887F B188 TBWBH1628A Compaq Presario 2255.txt.sb1xa4.csv' using 1:2 with lines title 'SB 1.x non-highspeed TC'
