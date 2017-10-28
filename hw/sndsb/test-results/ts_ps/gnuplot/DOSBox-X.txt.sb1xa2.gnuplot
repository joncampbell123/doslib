reset
set term png size 1920,1080
set output 'gnuplot/DOSBox-X.txt.sb1xa2.png'
set grid
set autoscale
set xrange [0:255]
set title 'Sound Blaster Time Constant and Playback rate 2-bit ADPCM (DOSBox-X)'
set xlabel 'Sample rate divider byte'
set ylabel 'Sample rate (Hz)'
plot 'gnuplot/DOSBox-X.txt.sb1xa2.csv' using 1:2 with lines title 'SB 1.x non-highspeed TC'
