# Call parameters:
#   $0 - datafilename
#   $1 - outputfilename

set terminal push
set terminal png size 1290,770

set datafile separator ","
set grid
set autoscale
set key top right box
set title  "Normalized Jitter per Message Size"
set xlabel "Message Size (bytes)"
set ylabel "Jitter Std. Dev. / Latency (percent)"
set format x "%.1s%c"
set format y "%.1s%%"

set output '$1.png'
plot '$0'\
     index 2 using 2:(100*$$8/$$3) t "TCP"                   with linespoints,\
  '' index 3 using 2:(100*$$8/$$3) t "UDP"                   with linespoints,\
  '' index 0 using 2:(100*$$8/$$3) t "multicast/best effort" with linespoints,\
  '' index 1 using 2:(100*$$8/$$3) t "multicast/reliable"    with linespoints

set xrange [0:2500]
set output '$1-zoom.png'
replot

set output
set terminal pop

