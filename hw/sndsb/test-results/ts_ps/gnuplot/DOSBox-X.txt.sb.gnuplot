reset
set term png size 1920,1080
set output 'gnuplot/DOSBox-X.txt.sb1.png'
set grid
set autoscale
set xrange [0:255]
set title 'Sound Blaster Time Constant and Playback rate (DOSBox-X)'
set xlabel 'Time constant byte'
set ylabel 'Sample rate (Hz)'
plot 'gnuplot/DOSBox-X.txt.sb.csv' using 1:2 with lines title 'SB 1.x non-highspeed TC'
reset
set term png size 1920,1080
set output 'gnuplot/DOSBox-X.txt.sb2.png'
set grid
set autoscale
set xrange [0:255]
set title 'Sound Blaster Time Constant and Playback rate (DOSBox-X)'
set xlabel 'Time constant byte'
set ylabel 'Sample rate (Hz)'
plot 'gnuplot/DOSBox-X.txt.sb.csv' using 1:3 with lines title 'SB 2.x highspeed TC'
