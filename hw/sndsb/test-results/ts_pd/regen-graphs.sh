#!/bin/bash
for txt in *.txt; do
    ./sb2gnuplot.pl "$txt" || break
done

# let's not clutter the source tree...
rm -v gnuplot/*.gnuplot
(cd gnuplot && find -size 0 -delete)

