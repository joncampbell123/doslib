reset
set term png size 1920,1080
set output 'gnuplot/ESS AudioDrive ES1887F B188 TBWBH1628A Compaq Presario 2255.txt.ess16.png'
set grid
set autoscale
set xrange [0:255]
set title 'ESS688 sample rate divider and Playback rate 16-bit (ESS AudioDrive ES1887F B188 TBWBH1628A Compaq Presario 2255)'
set xlabel 'Sample rate divider byte'
set ylabel 'Sample rate (Hz)'
plot 'gnuplot/ESS AudioDrive ES1887F B188 TBWBH1628A Compaq Presario 2255.txt.ess16.csv' using 1:2 with lines title 'Sample rate divider byte'
